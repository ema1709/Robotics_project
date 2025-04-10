#include <ros/ros.h>
#include <sensor_msgs/NavSatFix.h>
#include <tf/transform_broadcaster.h>
#include <nav_msgs/Odometry.h>
#include <geometry_msgs/Quaternion.h>
#include <tf/transform_datatypes.h>
#include <cmath>

class gps_odometry {
public:
    gps_odometry() {
        gps_sub = n.subscribe("/swiftnav/front/gps_pose", 100, &gps_odometry::callback, this);
        gps_pub_ = n.advertise<nav_msgs::Odometry>("/gps_odom", 1);

        private_n.param("semi_major_axis", a, 6378137.0);
        private_n.param("semi_minor_axis", b, 6356752.0);
    }

private:
    ros::NodeHandle n;
    ros::NodeHandle private_n{"~"};
    ros::Subscriber gps_sub;
    ros::Publisher gps_pub_;
    tf::TransformBroadcaster odom_broadcaster_;

    bool flag = false;

    double a, b;
    double Xr, Yr, Zr;
    double lat_r, longit_r;
    double x_last, y_last, z_last;

    void callback(const sensor_msgs::NavSatFix::ConstPtr& msg) {
        double lat = msg->latitude * M_PI / 180.0;
        double longit = msg->longitude * M_PI / 180.0;
        double h = msg->altitude;

        double e_squared = 1 - (b * b) / (a * a);
        double N = a / sqrt(1 - e_squared * sin(lat) * sin(lat));

        double Xp = (N + h) * cos(lat) * cos(longit);
        double Yp = (N + h) * cos(lat) * sin(longit);
        double Zp = (N * (1 - e_squared) + h) * sin(lat);

        if (!flag) {
            Xr = Xp;
            Yr = Yp;
            Zr = Zp;
            lat_r = lat;
            longit_r = longit;
        }

        double dx = Xp - Xr;
        double dy = Yp - Yr;
        double dz = Zp - Zr;

        double x = -sin(longit_r) * dx + cos(longit_r) * dy;
        double y = -sin(lat_r) * cos(longit_r) * dx - sin(lat_r) * sin(longit_r) * dy + cos(lat_r) * dz;
        double z = cos(lat_r) * cos(longit_r) * dx + cos(lat_r) * sin(longit_r) * dy + sin(lat_r) * dz;

        geometry_msgs::Quaternion gps_quat;
        if (flag) {
            double dx_enu = x - x_last;
            double dy_enu = y - y_last;

            if (dx_enu == 0 && dy_enu == 0) {
                gps_quat = tf::createQuaternionMsgFromYaw(0.0);
            } else {
                double yaw = std::atan2(dy_enu, dx_enu);
                gps_quat = tf::createQuaternionMsgFromYaw(yaw);
            }
        } else {
            gps_quat = tf::createQuaternionMsgFromYaw(0.0);
        }

        ros::Time current_time = ros::Time::now();

        // --- TF transform ---
        geometry_msgs::TransformStamped odom_tf;
        odom_tf.header.stamp = current_time;
        odom_tf.header.frame_id = "world";
        odom_tf.child_frame_id = "vehicle";
        odom_tf.transform.translation.x = x/500;
        odom_tf.transform.translation.y = y/500;
        odom_tf.transform.translation.z = 0.0;
        odom_tf.transform.rotation = gps_quat;
        odom_broadcaster_.sendTransform(odom_tf);

        // --- Odometry message ---
        nav_msgs::Odometry odom_msg;
        odom_msg.header.stamp = current_time;
        odom_msg.header.frame_id = "world";
        odom_msg.child_frame_id = "vehicle";
        odom_msg.pose.pose.position.x = x/500;
        odom_msg.pose.pose.position.y = y/500;
        odom_msg.pose.pose.position.z = 0.0;
        odom_msg.pose.pose.orientation = gps_quat;

        gps_pub_.publish(odom_msg);

        x_last = x;
        y_last = y;
        z_last = z;
        flag = true;
    }
};

int main(int argc, char** argv) {
    ros::init(argc, argv, "gps_odometer");
    gps_odometry node;
    ROS_INFO("Nodo avviato correttamente!");
    ros::spin();
    return 0;
}

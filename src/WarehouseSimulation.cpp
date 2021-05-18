#include "ros/ros.h"

#include <std_msgs/String.h>
#include <geometry_msgs/Pose.h>
#include <gazebo_msgs/SpawnModel.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <ios>
#include <math.h>
#include <memory>
#include <thread>
#include <mutex>
#include <string>
#include <future>

#include "Model.h"
#include "Storage.h"
#include "Dispatch.h"
#include "Product.h"
#include "Order.h"
#include "OrderController.h"
#include "Robot.h"

void loadModels(std::shared_ptr<Model> &modelController)
{
    modelController->Add("productA", "/home/renato/catkin_ws/src/delivery_robot_simulation/models/productA.sdf");
    modelController->Add("productB", "/home/renato/catkin_ws/src/delivery_robot_simulation/models/productB.sdf");
    modelController->Add("storageA", "/home/renato/catkin_ws/src/delivery_robot_simulation/models/storageA.sdf");
    modelController->Add("storageB", "/home/renato/catkin_ws/src/delivery_robot_simulation/models/storageA.sdf");
    modelController->Add("dispatchA", "/home/renato/catkin_ws/src/delivery_robot_simulation/models/dispatchA.sdf");
    modelController->Add("dispatchB", "/home/renato/catkin_ws/src/delivery_robot_simulation/models/dispatchA.sdf");
}

void InstatiateWarehouseObjects(std::vector<std::shared_ptr<Storage>> &storages, std::vector<std::shared_ptr<Dispatch>> &dispatches, std::shared_ptr<Model> modelController, std::string configsDirectory)
{
    std::string line;

    std::ifstream storagesConfigFs(configsDirectory + "storages");
    if (storagesConfigFs.is_open())
    {
        while (std::getline(storagesConfigFs, line))
        {
            if (line[0] == '#')
            {
                continue;
            }
            std::istringstream linestream(line);
            std::string storageModel{};
            std::string productionModel{};
            geometry_msgs::Pose storagePose{};
            geometry_msgs::Pose productOutputPose{};
            std::string psx{};
            std::string psy{};
            std::string psz{};
            std::string ppx{};
            std::string ppy{};
            std::string ppz{};

            linestream >> storageModel;

            linestream >> productionModel;

            linestream >> psx >> psy >> psz;
            storagePose.position.x = std::stof(psx);
            storagePose.position.y = std::stof(psy);
            storagePose.position.z = std::stof(psz);

            linestream >> ppx >> ppy >> ppz;
            productOutputPose.position.x = std::stof(ppx);
            productOutputPose.position.y = std::stof(ppy);
            productOutputPose.position.z = std::stof(ppz);

            storages.push_back(std::make_shared<Storage>(storageModel, productionModel, storagePose, productOutputPose, modelController));
        }
    }
    storagesConfigFs.close();

    std::ifstream dispatchesConfigFs(configsDirectory + "dispatches");
    if (dispatchesConfigFs.is_open())
    {
        while (std::getline(dispatchesConfigFs, line))
        {
            if (line[0] == '#')
            {
                continue;
            }
            std::istringstream linestream(line);
            std::string dispatchModel{};
            geometry_msgs::Pose dispatchPose{};
            std::string psx{};
            std::string psy{};
            std::string psz{};

            linestream >> dispatchModel;

            linestream >> psx >> psy >> psz;
            dispatchPose.position.x = std::stof(psx);
            dispatchPose.position.y = std::stof(psy);
            dispatchPose.position.z = std::stof(psz);

            dispatches.push_back(std::make_shared<Dispatch>(dispatchModel, dispatchPose, modelController));
        }
    }
    dispatchesConfigFs.close();
}

int main(int argc, char **argv)
{
    // Initialize the WarehouseSimulation node and create a handle to it
    ros::init(argc, argv, "WarehouseSimulation");
    ros::NodeHandle n;

    std::shared_ptr<Model> modelController = std::make_shared<Model>();

    // Create Storage objects
    std::vector<std::shared_ptr<Storage>> storages;
    std::vector<std::shared_ptr<Dispatch>> dispatches;
    std::vector<std::shared_ptr<Robot>> robots;
    std::shared_ptr<OrderController> orderController = std::make_shared<OrderController>("warehouseOrderController");

    // Load models
    loadModels(modelController);

    // Instantiate objects from config file
    InstatiateWarehouseObjects(storages, dispatches, modelController, "/home/renato/catkin_ws/src/delivery_robot_simulation/configs/");

    // Create a Robot Object and configure it
    robots.emplace_back(std::make_shared<Robot>("amr",storages,dispatches,orderController));
    

    // Add Storage model to simulation and start it
    std::for_each(storages.begin(), storages.end(), [modelController](std::shared_ptr<Storage> &s) {
        modelController->Spawn(s->GetName(), s->GetModelName(), s->getPose());
        s->Simulate();
    });

    std::for_each(dispatches.begin(), dispatches.end(), [modelController](std::shared_ptr<Dispatch> &d) {
        modelController->Spawn(d->GetName(), d->GetModelName(), d->getPose());
    });

    std::for_each(robots.begin(), robots.end(), [modelController](std::shared_ptr<Robot> &r) {
        
        r->StartOperation();
    });

    // std::for_each(robots.begin(), robots.end(), [modelController](std::shared_ptr<Robot> &r) {

    // });

    // REQ PICK WORKING DEMO
    // std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    // std::cout << "### REQ\n";
    // std::unique_ptr<Product> p = storages[0]->RequestProduct("productA", 1); // TODO: Add condition variables to wait until a product is produced

    // std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    // std::cout << "### PICK\n";
    // dispatches[0]->PickProduct(std::move(p));
    // std::cout << "\n\nSpawning RequestNextOrder threads..." << std::endl;
    // std::vector<std::future<std::shared_ptr<Order>>> futures;
    // for (int i = 0; i < 10; ++i)
    // {
    //     std::string message = "robot#" + std::to_string(i);
    //     futures.emplace_back(std::async(std::launch::async, &OrderController::RequestNextOrder, &*orderController, std::move(message)));
    // }

    ros::Subscriber ordersSubscriber = n.subscribe("warehouse/order/add", 10, &OrderController::AddOrder, &*orderController);

    ros::Subscriber sub11 = n.subscribe("t/1/1", 10, &Storage::RequestProduct1, &*storages[0]);
    ros::Subscriber sub21 = n.subscribe("t/2/1", 10, &Storage::RequestProduct1, &*storages[1]);

    ros::spin();

/*
Custom SIGINT Handler
You can install a custom SIGINT handler that plays nice with ROS like so:

Toggle line numbers
   1 #include <ros/ros.h>
   2 #include <signal.h>
   3 
   4 void mySigintHandler(int sig)
   5 {
   6   // Do some custom action.
   7   // For example, publish a stop message to some other nodes.
   8   
   9   // All the default sigint handler does is call shutdown()
  10   ros::shutdown();
  11 }
  12 
  13 int main(int argc, char** argv)
  14 {
  15   ros::init(argc, argv, "my_node_name", ros::init_options::NoSigintHandler);
  16   ros::NodeHandle nh;
  17 
  18   // Override the default ros sigint handler.
  19   // This must be set after the first NodeHandle is created.
  20   signal(SIGINT, mySigintHandler);
  21   
  22   //...
  23   ros::spin();
  24   return 0;
  25 }

*/

    // std::for_each(futures.begin(), futures.end(), [](std::future<std::shared_ptr<Order>> &ftr) {
    //     ftr.wait();
    // });
    return 0;
}

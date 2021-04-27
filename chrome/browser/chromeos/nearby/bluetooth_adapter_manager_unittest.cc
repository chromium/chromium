// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/nearby/bluetooth_adapter_manager.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_profile_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace nearby {

class BluetoothAdapterManagerTest : public testing::Test {
 public:
  BluetoothAdapterManagerTest() = default;
  ~BluetoothAdapterManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    bluetooth_adapter_manager_ = std::make_unique<BluetoothAdapterManager>();
    std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
        bluez::BluezDBusManager::GetSetterForTesting();

    dbus_setter->SetBluetoothAdapterClient(
        std::make_unique<bluez::FakeBluetoothAdapterClient>());
    dbus_setter->SetBluetoothAgentManagerClient(
        std::make_unique<bluez::FakeBluetoothAgentManagerClient>());
    dbus_setter->SetBluetoothDeviceClient(
        std::make_unique<bluez::FakeBluetoothDeviceClient>());
    dbus_setter->SetBluetoothProfileManagerClient(
        std::make_unique<bluez::FakeBluetoothProfileManagerClient>());
  }

  void Initialize() {
    scoped_refptr<device::BluetoothAdapter> adapter;
    base::RunLoop run_loop;
    device::BluetoothAdapterFactory::Get()->GetAdapter(
        base::BindLambdaForTesting(
            [&](scoped_refptr<device::BluetoothAdapter> a) {
              adapter = std::move(a);
              run_loop.Quit();
            }));
    run_loop.Run();
    ASSERT_TRUE(adapter);
    ASSERT_TRUE(adapter->IsInitialized());
    ASSERT_TRUE(adapter->IsPresent());

    bluez_adapter_ = static_cast<bluez::BluetoothAdapterBlueZ*>(adapter.get());
    default_name_ = bluez_adapter_->GetName();
    mojo::PendingReceiver<bluetooth::mojom::Adapter> pending_receiver;
    bluetooth_manager()->Initialize(std::move(pending_receiver),
                                    std::move(adapter));
  }

  // Sets a non-default name and enables discoverability.
  void EnterHighVizMode() {
    base::RunLoop name_loop;
    bluez_adapter_->SetName(
        "High Viz", base::BindLambdaForTesting([&]() { name_loop.Quit(); }),
        /*error_callback=*/base::DoNothing());
    name_loop.Run();

    base::RunLoop discoverable_loop;
    bluez_adapter_->SetDiscoverable(
        true, base::BindLambdaForTesting([&]() { discoverable_loop.Quit(); }),
        /*error_callback=*/base::DoNothing());
    discoverable_loop.Run();
  }

  bluez::BluetoothAdapterBlueZ* adapter() { return bluez_adapter_; }

  BluetoothAdapterManager* bluetooth_manager() {
    return bluetooth_adapter_manager_.get();
  }
  const std::string& default_name() { return default_name_; }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<BluetoothAdapterManager> bluetooth_adapter_manager_;
  bluez::BluetoothAdapterBlueZ* bluez_adapter_;
  std::string default_name_;
};

TEST_F(BluetoothAdapterManagerTest, Shutdown) {
  Initialize();

  EnterHighVizMode();
  ASSERT_NE(default_name(), adapter()->GetName());
  ASSERT_TRUE(adapter()->IsDiscoverable());

  dbus::ObjectPath object_path = adapter()->object_path();
  bluetooth_manager()->Shutdown();

  bluez::BluetoothAdapterClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothAdapterClient()
          ->GetProperties(object_path);
  ASSERT_EQ(default_name(), properties->alias.value());
  ASSERT_FALSE(properties->discoverable.value());
}

TEST_F(BluetoothAdapterManagerTest, Shutdown_NeverInitialized) {
  bluetooth_manager()->Shutdown();
  // Verify that nothing crashes.
}

}  // namespace nearby
}  // namespace chromeos

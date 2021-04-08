// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/bluetooth_policy_handler.h"

#include "base/test/task_environment.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class BluetoothPolicyHandlerTest : public testing::Test {
 protected:
  class TestingBluetoothAdapter : public device::MockBluetoothAdapter {
   protected:
    ~TestingBluetoothAdapter() override {}

   public:
    TestingBluetoothAdapter() : is_shutdown_(false), is_powered_(true) {}

    void Shutdown() override { is_shutdown_ = true; }
    void SetPowered(bool powered, base::OnceClosure callack,
                    ErrorCallback error_callback) override {
        is_powered_ = powered;
    }
    void SetServiceAllowList(const UUIDList& uuids,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override {
      uuids_ = uuids;
    }
    bool IsPresent() const override { return !is_shutdown_; }
    bool IsPowered() const override { return is_powered_; }
    const UUIDList& GetAllowList() const { return uuids_; }

   protected:
    bool is_shutdown_;
    bool is_powered_;
    UUIDList uuids_;
  };

  BluetoothPolicyHandlerTest() : adapter_(new TestingBluetoothAdapter) {}
  ~BluetoothPolicyHandlerTest() override {}

  // testing::Test
  void SetUp() override {
    testing::Test::SetUp();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
  }

  void TearDown() override {}

 protected:
  void SetAllowBluetooth(bool allow_bluetooth) {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        chromeos::kAllowBluetooth, allow_bluetooth);
  }

  void SetDeviceAllowedBluetoothServices(const base::ListValue& allowlist) {
    scoped_testing_cros_settings_.device_settings()->Set(
        chromeos::kDeviceAllowedBluetoothServices, allowlist);
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<TestingBluetoothAdapter> adapter_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

TEST_F(BluetoothPolicyHandlerTest, TestZeroOnOffOn) {
  BluetoothPolicyHandler shutdown_policy_handler(ash::CrosSettings::Get());
  EXPECT_TRUE(adapter_->IsPresent());

  SetAllowBluetooth(true);
  EXPECT_TRUE(adapter_->IsPresent());

  SetAllowBluetooth(false);
  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());

  // Once the Bluetooth stack goes down, it needs a reboot to come back up.
  SetAllowBluetooth(true);
  EXPECT_FALSE(adapter_->IsPresent());
}

TEST_F(BluetoothPolicyHandlerTest, OffDuringStartup) {
  SetAllowBluetooth(false);
  BluetoothPolicyHandler shutdown_policy_handler(ash::CrosSettings::Get());
  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());
}

TEST_F(BluetoothPolicyHandlerTest, OnDuringStartup) {
  SetAllowBluetooth(true);
  BluetoothPolicyHandler shutdown_policy_handler(ash::CrosSettings::Get());
  EXPECT_TRUE(adapter_->IsPresent());
}

TEST_F(BluetoothPolicyHandlerTest, TestSetServiceAllowList) {
  base::ListValue allowlist;
  const char* test_uuid1_str = "0x1124";
  const char* test_uuid2_str = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
  allowlist.Append(base::Value(test_uuid1_str));
  allowlist.Append(base::Value(test_uuid2_str));
  SetDeviceAllowedBluetoothServices(allowlist);
  BluetoothPolicyHandler bluetooth_policy_handler(ash::CrosSettings::Get());
  const device::BluetoothUUID test_uuid1(test_uuid1_str);
  const device::BluetoothUUID test_uuid2(test_uuid2_str);
  const std::vector<device::BluetoothUUID>& allowlist_ =
      adapter_->GetAllowList();
  EXPECT_EQ(test_uuid1, allowlist_[0]);
  EXPECT_EQ(test_uuid2, allowlist_[1]);
}

}  // namespace policy

// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/bluetooth_policy_handler.h"

#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
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
    TestingBluetoothAdapter() : is_shutdown_(false) {}

    void Shutdown() override { is_shutdown_ = true; }
    bool IsPresent() const override { return !is_shutdown_; }

   protected:
    bool is_shutdown_;
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

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<TestingBluetoothAdapter> adapter_;
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

TEST_F(BluetoothPolicyHandlerTest, TestZeroOnOffOn) {
  BluetoothPolicyHandler shutdown_policy_handler(chromeos::CrosSettings::Get());
  EXPECT_TRUE(adapter_->IsPresent());

  SetAllowBluetooth(true);
  EXPECT_TRUE(adapter_->IsPresent());

  SetAllowBluetooth(false);
  EXPECT_FALSE(adapter_->IsPresent());

  // Once the Bluetooth stack goes down, it needs a reboot to come back up.
  SetAllowBluetooth(true);
  EXPECT_FALSE(adapter_->IsPresent());
}

TEST_F(BluetoothPolicyHandlerTest, OffDuringStartup) {
  SetAllowBluetooth(false);
  BluetoothPolicyHandler shutdown_policy_handler(chromeos::CrosSettings::Get());
  EXPECT_FALSE(adapter_->IsPresent());
}

TEST_F(BluetoothPolicyHandlerTest, OnDuringStartup) {
  SetAllowBluetooth(true);
  BluetoothPolicyHandler shutdown_policy_handler(chromeos::CrosSettings::Get());
  EXPECT_TRUE(adapter_->IsPresent());
}

}  // namespace policy

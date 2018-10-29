// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/bluetooth_policy_handler.h"

#include "base/test/scoped_task_environment.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
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
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
  }

  void TearDown() override {}

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<TestingBluetoothAdapter> adapter_;
  chromeos::ScopedCrosSettingsTestHelper settings_helper_;
};

TEST_F(BluetoothPolicyHandlerTest, TestZeroOnOffOn) {
  BluetoothPolicyHandler shutdown_policy_handler(chromeos::CrosSettings::Get());
  EXPECT_TRUE(adapter_->IsPresent());

  settings_helper_.SetBoolean(chromeos::kAllowBluetooth, true);
  EXPECT_TRUE(adapter_->IsPresent());

  settings_helper_.SetBoolean(chromeos::kAllowBluetooth, false);
  EXPECT_FALSE(adapter_->IsPresent());

  // Once the Bluetooth stack goes down, it needs a reboot to come back up.
  settings_helper_.SetBoolean(chromeos::kAllowBluetooth, true);
  EXPECT_FALSE(adapter_->IsPresent());
}

TEST_F(BluetoothPolicyHandlerTest, OffDuringStartup) {
  settings_helper_.SetBoolean(chromeos::kAllowBluetooth, false);
  BluetoothPolicyHandler shutdown_policy_handler(chromeos::CrosSettings::Get());
  EXPECT_FALSE(adapter_->IsPresent());
}

TEST_F(BluetoothPolicyHandlerTest, OnDuringStartup) {
  settings_helper_.SetBoolean(chromeos::kAllowBluetooth, true);
  BluetoothPolicyHandler shutdown_policy_handler(chromeos::CrosSettings::Get());
  EXPECT_TRUE(adapter_->IsPresent());
}

}  // namespace policy

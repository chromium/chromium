// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_name_store.h"

#include "ash/constants/ash_features.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_chromeos.h"
#include "chrome/browser/ash/policy/handlers/fake_device_name_policy_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class DeviceNameStoreImplTest : public ::testing::Test {
 public:
  DeviceNameStoreImplTest() {
    DeviceNameStore::RegisterLocalStatePrefs(local_state_.registry());
  }
  ~DeviceNameStoreImplTest() override = default;

  // testing::Test
  void TearDown() override { DeviceNameStore::Shutdown(); }

  void InitializeDeviceNameStore(bool is_hostname_setting_flag_enabled) {
    if (is_hostname_setting_flag_enabled) {
      feature_list_.InitAndEnableFeature(ash::features::kEnableHostnameSetting);
    } else {
      feature_list_.InitAndDisableFeature(
          ash::features::kEnableHostnameSetting);
    }
    DeviceNameStore::Initialize(&local_state_,
                                &fake_device_name_policy_handler_);
  }

  std::string GetDeviceNameFromPrefs() const {
    return local_state_.GetString(prefs::kDeviceName);
  }

  policy::FakeDeviceNamePolicyHandler fake_device_name_policy_handler_;

 private:
  // Run on the UI thread.
  content::BrowserTaskEnvironment task_environment_;

  // Test backing store for prefs.
  TestingPrefServiceSimple local_state_;

  base::test::ScopedFeatureList feature_list_;
};

// Check that error is thrown if GetInstance() is called before
// initialization.
TEST_F(DeviceNameStoreImplTest, GetInstanceBeforeInitializeError) {
  EXPECT_DEATH(DeviceNameStore::GetInstance(), "");
}

// Check that error is thrown upon initialization if kEnableHostnameSetting
// flag is off.
TEST_F(DeviceNameStoreImplTest, EnableHostnameSettingFlagOff) {
  EXPECT_DEATH(
      InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/false),
      "");
}

// Verifies the device name is set to 'ChromeOS' by default upon initialization
// and that the device name is persisted to the local state.
TEST_F(DeviceNameStoreImplTest, DefaultDeviceName) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true);
  DeviceNameStore* device_name_store_ = DeviceNameStore::GetInstance();
  EXPECT_EQ(device_name_store_->GetDeviceName(), "ChromeOS");
  EXPECT_EQ(GetDeviceNameFromPrefs(), "ChromeOS");
}

// Verifies the device name is the template chosen by admin if device name
// policy is set to kPolicyHostnameChosenByAdmin.
TEST_F(DeviceNameStoreImplTest, DeviceNameChosenByAdmin) {
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true);
  DeviceNameStore* device_name_store_ = DeviceNameStore::GetInstance();
  const std::string template_set_by_admin = "AdminTemplate";
  fake_device_name_policy_handler_.SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameChosenByAdmin,
      template_set_by_admin);
  EXPECT_EQ(device_name_store_->GetDeviceName(), template_set_by_admin);
}

}  // namespace chromeos

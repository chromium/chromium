// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_name_store.h"

#include "ash/constants/ash_features.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class DeviceNameStoreTest : public ::testing::Test {
 public:
  DeviceNameStoreTest() {
    DeviceNameStore::RegisterLocalStatePrefs(local_state_.registry());
  }
  ~DeviceNameStoreTest() override = default;

  // testing::Test
  void TearDown() override { DeviceNameStore::Shutdown(); }

  void InitializeDeviceNameStore(bool is_hostname_setting_flag_enabled) {
    if (is_hostname_setting_flag_enabled) {
      feature_list_.InitAndEnableFeature(ash::features::kEnableHostnameSetting);
    } else {
      feature_list_.InitAndDisableFeature(
          ash::features::kEnableHostnameSetting);
    }
    DeviceNameStore::Initialize(&local_state_);
  }

  std::string GetDeviceNameFromPrefs() const {
    return local_state_.GetString(prefs::kDeviceName);
  }

 private:
  // Run on the UI thread.
  content::BrowserTaskEnvironment task_environment_;

  // Test backing store for prefs.
  TestingPrefServiceSimple local_state_;

  base::test::ScopedFeatureList feature_list_;
};

// Check that error is thrown if GetInstance() is called before
// initialization.
TEST_F(DeviceNameStoreTest, GetInstanceBeforeInitializeError) {
  EXPECT_DEATH(DeviceNameStore::GetInstance(), "");
}

// Check that error is thrown upon initialization if kEnableHostnameSetting
// flag is off.
TEST_F(DeviceNameStoreTest, EnableHostnameSettingFlagOff) {
  EXPECT_DEATH(
      InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/false),
      "");
}

// Verifies the device name is set to 'ChromeOS' by default upon initialization
// and that the device name is persisted to the local state.
TEST_F(DeviceNameStoreTest, DefaultDeviceName) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true);
  DeviceNameStore* device_name_store_ = DeviceNameStore::GetInstance();
  EXPECT_EQ(device_name_store_->GetDeviceName(), "ChromeOS");
  EXPECT_EQ(GetDeviceNameFromPrefs(), "ChromeOS");
}

}  // namespace chromeos

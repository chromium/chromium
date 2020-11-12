// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_name_store.h"

#include "base/strings/string_util.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class DeviceNameStoreTest : public ::testing::Test {
 public:
  DeviceNameStoreTest() {
    DeviceNameStore::RegisterLocalStatePrefs(local_state_.registry());
    device_name_store_.Initialize(&local_state_);
  }
  ~DeviceNameStoreTest() override = default;

  DeviceNameStore* device_name_store() { return &device_name_store_; }

 private:
  // Run on the UI thread.
  content::BrowserTaskEnvironment task_environment_;

  // Test backing store for prefs.
  TestingPrefServiceSimple local_state_;

  DeviceNameStore device_name_store_;
};

TEST_F(DeviceNameStoreTest, Initialize) {
  TestingPrefServiceSimple local_state;
  DeviceNameStore device_name_store;

  DeviceNameStore::RegisterLocalStatePrefs(local_state.registry());

  // The device name is not set yet.
  EXPECT_TRUE(local_state.GetString(prefs::kDeviceName).empty());

  device_name_store.Initialize(&local_state);

  // Initialize now set the device name and persisted it to the local state.
  std::string device_name = device_name_store.GetDeviceName();
  std::string persisted_device_name = local_state.GetString(prefs::kDeviceName);
  EXPECT_FALSE(device_name.empty());
  EXPECT_FALSE(persisted_device_name.empty());
  EXPECT_EQ(device_name, persisted_device_name);
}

// Tests that the format of the generated device name matches the form
// ChromeOS_123456.
TEST_F(DeviceNameStoreTest, GenerateDeviceName) {
  // The device name is already generated at this point because of the call to
  // Initialize during test setup.
  std::string device_name = device_name_store()->GetDeviceName();
  EXPECT_TRUE(base::StartsWith(device_name, "ChromeOS_"));

  // Check that the string after the prefix is composed of digits.
  std::string digits = device_name.substr(strlen("ChromeOS_"));
  for (size_t i = 0; i < digits.length(); ++i) {
    EXPECT_TRUE(base::IsAsciiDigit(digits[i]));
  }
}

}  // namespace chromeos

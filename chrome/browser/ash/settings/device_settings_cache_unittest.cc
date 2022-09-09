// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/device_settings_cache.h"

#include "chrome/common/pref_names.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace ash {

class DeviceSettingsCacheTest : public testing::Test {
 protected:
  void SetUp() override {
    // prepare some data.
    policy_.set_policy_type("google/chromeos/device");
    em::ChromeDeviceSettingsProto pol;
    pol.mutable_allow_new_users()->set_allow_new_users(false);
    policy_.set_policy_value(pol.SerializeAsString());

    device_settings_cache::RegisterPrefs(local_state_.registry());
  }

  TestingPrefServiceSimple local_state_;
  em::PolicyData policy_;
};

TEST_F(DeviceSettingsCacheTest, Basic) {
  EXPECT_TRUE(device_settings_cache::Store(policy_, &local_state_));

  em::PolicyData policy_out;
  EXPECT_TRUE(device_settings_cache::Retrieve(&policy_out, &local_state_));

  EXPECT_TRUE(policy_out.has_policy_type());
  EXPECT_TRUE(policy_out.has_policy_value());

  em::ChromeDeviceSettingsProto pol;
  pol.ParseFromString(policy_out.policy_value());
  EXPECT_TRUE(pol.has_allow_new_users());
  EXPECT_FALSE(pol.allow_new_users().allow_new_users());
}

TEST_F(DeviceSettingsCacheTest, CorruptData) {
  EXPECT_TRUE(device_settings_cache::Store(policy_, &local_state_));

  local_state_.SetString(prefs::kDeviceSettingsCache, "blaaa");

  em::PolicyData policy_out;
  EXPECT_FALSE(device_settings_cache::Retrieve(&policy_out, &local_state_));
}

}  // namespace ash

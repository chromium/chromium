// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"

#include <memory>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_constants.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class AccessCodeCastFeatureTest : public ::testing::Test {
 public:
  AccessCodeCastFeatureTest() = default;
  AccessCodeCastFeatureTest(const AccessCodeCastFeatureTest&) = delete;
  ~AccessCodeCastFeatureTest() override = default;
  AccessCodeCastFeatureTest& operator=(const AccessCodeCastFeatureTest&) =
      delete;
  void TearDown() override { ClearMediaRouterStoredPrefsForTesting(); }

 protected:
  content::BrowserTaskEnvironment test_environment_;
};

TEST_F(AccessCodeCastFeatureTest, GetAccessCodeCastEnabledPref) {
  TestingProfile profile;
  profile.GetTestingPrefService()->SetManagedPref(
      ::prefs::kEnableMediaRouter, std::make_unique<base::Value>(true));
  profile.GetTestingPrefService()->SetManagedPref(
      prefs::kAccessCodeCastEnabled, std::make_unique<base::Value>(false));

  EXPECT_FALSE(GetAccessCodeCastEnabledPref(&profile));

  // Setting the pref to true should now return true.
  profile.GetTestingPrefService()->SetManagedPref(
      prefs::kAccessCodeCastEnabled, std::make_unique<base::Value>(true));
  EXPECT_TRUE(GetAccessCodeCastEnabledPref(&profile));

  // Removing the set value should now return the default value (false).
  profile.GetTestingPrefService()->RemoveManagedPref(
      prefs::kAccessCodeCastEnabled);
  EXPECT_FALSE(GetAccessCodeCastEnabledPref(&profile));
}

TEST_F(AccessCodeCastFeatureTest,
       GetAccessCodeCastEnabledPrefMediaRouterDisabled) {
  TestingProfile profile;
  profile.GetTestingPrefService()->SetManagedPref(
      ::prefs::kEnableMediaRouter, std::make_unique<base::Value>(false));
  profile.GetTestingPrefService()->SetManagedPref(
      prefs::kAccessCodeCastEnabled, std::make_unique<base::Value>(true));

  EXPECT_FALSE(GetAccessCodeCastEnabledPref(&profile));
}

TEST_F(AccessCodeCastFeatureTest, GetAccessCodeDeviceDurationPref) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kAccessCodeCastRememberDevices}, {});
  const int non_default = 10;

  TestingProfile profile;
  profile.GetTestingPrefService()->SetManagedPref(
      ::prefs::kEnableMediaRouter, std::make_unique<base::Value>(true));
  profile.GetTestingPrefService()->SetManagedPref(
      prefs::kAccessCodeCastEnabled, std::make_unique<base::Value>(false));

  // Defaults to 0.
  EXPECT_EQ(base::Seconds(0), GetAccessCodeDeviceDurationPref(&profile));

  // Setting to a non-zero value should cause the return value to match.
  profile.GetTestingPrefService()->SetManagedPref(
      prefs::kAccessCodeCastEnabled, std::make_unique<base::Value>(true));
  profile.GetTestingPrefService()->SetManagedPref(
      prefs::kAccessCodeCastDeviceDuration,
      std::make_unique<base::Value>(non_default));
  EXPECT_EQ(base::Seconds(non_default),
            GetAccessCodeDeviceDurationPref(&profile));

  // Disabling the feature overall in policy now makes this return 0.
  profile.GetTestingPrefService()->SetManagedPref(
      prefs::kAccessCodeCastEnabled, std::make_unique<base::Value>(false));
  EXPECT_EQ(base::Seconds(0), GetAccessCodeDeviceDurationPref(&profile));

  profile.GetTestingPrefService()->SetManagedPref(
      prefs::kAccessCodeCastEnabled, std::make_unique<base::Value>(true));
  // Removing the set value should return the default.
  profile.GetTestingPrefService()->RemoveManagedPref(
      prefs::kAccessCodeCastDeviceDuration);
  EXPECT_EQ(base::Seconds(0), GetAccessCodeDeviceDurationPref(&profile));
}

}  // namespace media_router

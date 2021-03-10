// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_features.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace borealis {
namespace {

class BorealisFeaturesTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(BorealisFeaturesTest, DisallowedWhenFeatureIsDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kBorealis);
  EXPECT_FALSE(BorealisFeatures(&profile_).IsAllowed());
}

TEST_F(BorealisFeaturesTest, AllowedWhenFeatureIsEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kBorealis);
  EXPECT_TRUE(BorealisFeatures(&profile_).IsAllowed());
}

TEST_F(BorealisFeaturesTest, CanDisableByUserPolicy) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kBorealis);

  profile_.GetPrefs()->SetBoolean(prefs::kBorealisAllowedForUser, false);

  EXPECT_FALSE(BorealisFeatures(&profile_).IsAllowed());
}

TEST_F(BorealisFeaturesTest, CanDisableByDevicePolicy) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kBorealis);

  profile_.ScopedCrosSettingsTestHelper()
      ->ReplaceDeviceSettingsProviderWithStub();
  profile_.ScopedCrosSettingsTestHelper()->InstallAttributes()->SetCloudManaged(
      "unittest.com", "device_id");
  EXPECT_TRUE(BorealisFeatures(&profile_).IsAllowed());

  profile_.ScopedCrosSettingsTestHelper()->GetStubbedProvider()->SetBoolean(
      chromeos::kBorealisAllowedForDevice, false);

  EXPECT_FALSE(BorealisFeatures(&profile_).IsAllowed());
}

TEST_F(BorealisFeaturesTest, EnablednessDependsOnInstallation) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kBorealis);

  // The pref is false by default
  EXPECT_FALSE(BorealisFeatures(&profile_).IsEnabled());

  profile_.GetPrefs()->SetBoolean(prefs::kBorealisInstalledOnDevice, true);

  EXPECT_TRUE(BorealisFeatures(&profile_).IsEnabled());
}

}  // namespace
}  // namespace borealis

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_features.h"

#include "ash/components/settings/cros_settings_names.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "chrome/browser/ash/borealis/testing/features.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace borealis {
namespace {

class BorealisFeaturesTest : public testing::Test {
 public:
  BorealisFeaturesTest()
      : user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(user_manager_)) {
    AllowBorealis(&profile_, &features_, user_manager_, /*also_enable=*/false);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::test::ScopedFeatureList features_;
  ash::FakeChromeUserManager* user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
};

TEST_F(BorealisFeaturesTest, DisallowedWhenFeatureIsDisabled) {
  features_.Reset();
  features_.InitAndDisableFeature(features::kBorealis);
  EXPECT_EQ(BorealisFeatures(&profile_).MightBeAllowed(),
            BorealisFeatures::AllowStatus::kFeatureDisabled);
}

TEST_F(BorealisFeaturesTest, AllowedWhenFeatureIsEnabled) {
  EXPECT_EQ(BorealisFeatures(&profile_).MightBeAllowed(),
            BorealisFeatures::AllowStatus::kAllowed);
}

TEST_F(BorealisFeaturesTest, UnenrolledUserPolicyAllowedByDefault) {
  EXPECT_EQ(BorealisFeatures(&profile_).MightBeAllowed(),
            BorealisFeatures::AllowStatus::kAllowed);

  profile_.GetPrefs()->SetBoolean(prefs::kBorealisAllowedForUser, false);
  EXPECT_EQ(BorealisFeatures(&profile_).MightBeAllowed(),
            BorealisFeatures::AllowStatus::kUserPrefBlocked);
}

TEST_F(BorealisFeaturesTest, CanDisableByVmPolicy) {
  EXPECT_EQ(BorealisFeatures(&profile_).MightBeAllowed(),
            BorealisFeatures::AllowStatus::kAllowed);

  profile_.ScopedCrosSettingsTestHelper()
      ->ReplaceDeviceSettingsProviderWithStub();
  profile_.ScopedCrosSettingsTestHelper()->GetStubbedProvider()->SetBoolean(
      ash::kVirtualMachinesAllowed, false);

  EXPECT_EQ(BorealisFeatures(&profile_).MightBeAllowed(),
            BorealisFeatures::AllowStatus::kVmPolicyBlocked);
}

TEST_F(BorealisFeaturesTest, EnrolledUserPolicyDisabledByDefault) {
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  EXPECT_EQ(BorealisFeatures(&profile_).MightBeAllowed(),
            BorealisFeatures::AllowStatus::kUserPrefBlocked);
  profile_.GetTestingPrefService()->SetManagedPref(
      prefs::kBorealisAllowedForUser, std::make_unique<base::Value>(true));
  EXPECT_EQ(BorealisFeatures(&profile_).MightBeAllowed(),
            BorealisFeatures::AllowStatus::kAllowed);
}

TEST_F(BorealisFeaturesTest, EnablednessDependsOnInstallation) {
  // The pref is false by default
  EXPECT_FALSE(BorealisFeatures(&profile_).IsEnabled());

  profile_.GetPrefs()->SetBoolean(prefs::kBorealisInstalledOnDevice, true);

  EXPECT_TRUE(BorealisFeatures(&profile_).IsEnabled());
}

// These are meta-tests that just makes sure our feature helper does what it
// says on the tin.
//
// Due to a weird interaction between the FakeChromeUserManager and the
// ProfileHelper it is only possible to have one ScopedAllowBorealis per test.

TEST(BorealisFeaturesHelperTest, CheckFeatureHelperWithoutEnable) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  BorealisFeatures features(&profile);
  ScopedAllowBorealis allow(&profile, /*also_enable=*/false);
  EXPECT_EQ(features.MightBeAllowed(), BorealisFeatures::AllowStatus::kAllowed);
  EXPECT_FALSE(features.IsEnabled());
  NiceCallbackFactory<void(BorealisFeatures::AllowStatus)> factory;
  EXPECT_CALL(factory, Call(BorealisFeatures::AllowStatus::kAllowed));
  // We want this to return synchronously in tests, hence no base::RunLoop.
  features.IsAllowed(factory.BindOnce());
}

TEST(BorealisFeaturesHelperTest, CheckFeatureHelperWithEnable) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  BorealisFeatures features(&profile);
  ScopedAllowBorealis allow(&profile, /*also_enable=*/true);
  EXPECT_TRUE(features.IsEnabled());
}

}  // namespace
}  // namespace borealis

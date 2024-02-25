// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_features.h"

#include <cstdint>
#include <limits>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "chrome/browser/ash/borealis/testing/features.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/statistics_provider.h"
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
      : fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()) {
    AllowBorealis(&profile_, &features_, fake_user_manager_.Get(),
                  /*also_enable=*/false);
  }

  BorealisFeatures::AllowStatus GetStatus() {
    base::test::TestFuture<BorealisFeatures::AllowStatus> status_future;
    BorealisFeatures(&profile_).IsAllowed(status_future.GetCallback());
    return status_future.Get();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList features_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  TestingProfile profile_;
};

TEST_F(BorealisFeaturesTest, DisallowedWhenFeatureIsDisabled) {
  features_.Reset();
  features_.InitAndDisableFeature(features::kBorealis);
  EXPECT_EQ(GetStatus(), BorealisFeatures::AllowStatus::kFeatureDisabled);
}
TEST_F(BorealisFeaturesTest, AllowedWhenFeatureIsEnabled) {
  EXPECT_EQ(GetStatus(), BorealisFeatures::AllowStatus::kAllowed);
}

TEST_F(BorealisFeaturesTest, UnenrolledUserPolicyAllowedByDefault) {
  EXPECT_EQ(GetStatus(), BorealisFeatures::AllowStatus::kAllowed);

  profile_.GetPrefs()->SetBoolean(prefs::kBorealisAllowedForUser, false);
  EXPECT_EQ(GetStatus(), BorealisFeatures::AllowStatus::kUserPrefBlocked);
}

TEST_F(BorealisFeaturesTest, CanDisableByVmPolicy) {
  EXPECT_EQ(GetStatus(), BorealisFeatures::AllowStatus::kAllowed);

  profile_.ScopedCrosSettingsTestHelper()
      ->ReplaceDeviceSettingsProviderWithStub();
  profile_.ScopedCrosSettingsTestHelper()->GetStubbedProvider()->SetBoolean(
      ash::kVirtualMachinesAllowed, false);

  EXPECT_EQ(GetStatus(), BorealisFeatures::AllowStatus::kVmPolicyBlocked);
}

TEST_F(BorealisFeaturesTest, EnrolledUserPolicyDisabledByDefault) {
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  EXPECT_EQ(GetStatus(), BorealisFeatures::AllowStatus::kUserPrefBlocked);
  profile_.GetTestingPrefService()->SetManagedPref(
      prefs::kBorealisAllowedForUser, std::make_unique<base::Value>(true));
  EXPECT_EQ(GetStatus(), BorealisFeatures::AllowStatus::kAllowed);
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

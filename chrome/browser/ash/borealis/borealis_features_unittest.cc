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
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/borealis/borealis_features_util.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "chrome/browser/ash/borealis/testing/features.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
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
      : user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(user_manager_.get())) {
    AllowBorealis(&profile_, &features_, user_manager_, /*also_enable=*/false);
  }

  BorealisFeatures::AllowStatus GetStatus() {
    base::test::TestFuture<BorealisFeatures::AllowStatus> status_future;
    BorealisFeatures(&profile_).IsAllowed(status_future.GetCallback());
    return status_future.Get();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::test::ScopedFeatureList features_;
  raw_ptr<ash::FakeChromeUserManager, ExperimentalAsh> user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
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

TEST(BorealisFeaturesUtilTest, DeterministicHash) {
  EXPECT_EQ(TokenHardwareChecker::H("token", "salt"),
            "ZnIP7tz1bWJV7++grGG9C9lCFIQa49zw0nY0ac1bfoo=");
}

TEST(BorealisFeaturesUtilTest, TokenHardwareCheckerWorks) {
  TokenHardwareChecker::Data d{"token", "board", "model", "cpu", 42};
  TokenHardwareChecker checker(std::move(d));

  EXPECT_FALSE(checker.TokenHashMatches("foo", "bar"));
  EXPECT_TRUE(checker.TokenHashMatches(
      "salt", "ZnIP7tz1bWJV7++grGG9C9lCFIQa49zw0nY0ac1bfoo="));

  EXPECT_FALSE(checker.IsBoard("notboard"));
  EXPECT_TRUE(checker.IsBoard("board"));
  EXPECT_FALSE(checker.BoardIn(base::flat_set<std::string>{}));
  EXPECT_TRUE(
      checker.BoardIn(base::flat_set<std::string>{"board", "notboard"}));

  EXPECT_FALSE(checker.IsModel("notmodel"));
  EXPECT_TRUE(checker.IsModel("model"));
  EXPECT_FALSE(checker.ModelIn(base::flat_set<std::string>{}));
  EXPECT_TRUE(
      checker.ModelIn(base::flat_set<std::string>{"notmodel", "model"}));

  EXPECT_FALSE(checker.CpuRegexMatches("xyz"));
  EXPECT_FALSE(checker.CpuRegexMatches("[A-Z]"));
  EXPECT_TRUE(checker.CpuRegexMatches("cpu"));
  EXPECT_TRUE(checker.CpuRegexMatches("pu"));
  EXPECT_TRUE(checker.CpuRegexMatches("c.u"));
  EXPECT_TRUE(checker.CpuRegexMatches("cp"));
  EXPECT_TRUE(checker.CpuRegexMatches("^[a-z]*$"));

  EXPECT_TRUE(checker.HasMemory(0));
  EXPECT_TRUE(checker.HasMemory(41));
  EXPECT_TRUE(checker.HasMemory(42));
  EXPECT_FALSE(checker.HasMemory(43));
  EXPECT_FALSE(checker.HasMemory(std::numeric_limits<uint64_t>::max()));
}

TEST(BorealisFeaturesUtilTest, DataCanBeBuilt) {
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedChromeOSVersionInfo version(
      "CHROMEOS_RELEASE_BOARD=board\n", base::Time());
  ash::system::FakeStatisticsProvider fsp;
  fsp.SetMachineStatistic(ash::system::kCustomizationIdKey, "model");
  ash::system::StatisticsProvider::SetTestProvider(&fsp);

  base::RunLoop loop;
  TokenHardwareChecker::GetData(
      "token",
      base::BindLambdaForTesting([&loop](TokenHardwareChecker::Data data) {
        EXPECT_EQ(data.token_hash, "token");
        EXPECT_EQ(data.board, "board");
        EXPECT_EQ(data.model, "model");
        // Faking CPU and RAM are not supported, so just assert they have some
        // trivial values.
        EXPECT_NE(data.cpu, "");
        EXPECT_GT(data.memory, 0u);
        loop.Quit();
      }));
  loop.Run();
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

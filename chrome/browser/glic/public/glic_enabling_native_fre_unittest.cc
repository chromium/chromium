// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>

#include "base/android/android_info.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/startup_visibility.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/test_variations_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class GlicEnablingNativeFreTest : public testing::Test {
 public:
  GlicEnablingNativeFreTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kGlicRollout,
         features::kGlicNativeFreAndroid},
        {features::kGlicCountryFiltering, features::kGlicLocaleFiltering});
  }

  void SetUp() override {
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kTestType);

    if (base::android::android_info::sdk_int() <
        base::android::android_info::SDK_VERSION_S) {
      GTEST_SKIP() << "Glic requires Android S+";
    }

    GlicEnabling::SetSystemRequirementMetForTesting(true);

    TestingProfileManager* testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);

    profile_ = testing_profile_manager->CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName, /*prefs=*/nullptr,
        base::UTF8ToUTF16(TestingProfile::kDefaultProfileUserName),
        /*avatar_id=*/0,
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());

    testing_profile_manager->profile_manager()
        ->GetProfileAttributesStorage()
        .GetProfileAttributesWithPath(profile_->GetPath())
        ->SetAvatarIconIndex(1);

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);

    variations::TestVariationsService::RegisterPrefs(local_state_.registry());
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &local_state_, &enabled_state_provider_, std::wstring(),
        base::FilePath(), metrics::StartupVisibility::kUnknown);
    variations_service_ = std::make_unique<variations::TestVariationsService>(
        &local_state_, metrics_state_manager_.get());
    variations_service_->OverrideStoredPermanentCountry("us");
    TestingBrowserProcess::GetGlobal()->SetVariationsService(
        variations_service_.get());
  }

  void TearDown() override {
    GlicEnabling::SetSystemRequirementMetForTesting(std::nullopt);
    if (IsSkipped()) {
      testing::Test::TearDown();
      return;
    }
    TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);
    variations_service_.reset();
    metrics_state_manager_.reset();
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
    testing::Test::TearDown();
  }

  Profile* profile() { return profile_; }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  variations::TestVariationsService* variations_service() {
    return variations_service_.get();
  }

  void SignInAsAdult() {
    AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
        "adult@example.com", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_gemini_in_chrome(true);
    mutator.set_can_use_model_execution_features(true);
    signin::UpdateAccountInfoForAccount(identity_test_env()->identity_manager(),
                                        account_info);
  }

  void SignInAsMinor() {
    AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
        "minor@example.com", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_gemini_in_chrome(true);
    mutator.set_can_use_model_execution_features(false);
    signin::UpdateAccountInfoForAccount(identity_test_env()->identity_manager(),
                                        account_info);
  }

  void SignInWithUnknownCapabilities() {
    identity_test_env()->MakePrimaryAccountAvailable(
        "unknown@example.com", signin::ConsentLevel::kSignin);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedCommandLine scoped_command_line_;
  base::test::ScopedFeatureList scoped_feature_list_;

  TestingPrefServiceSimple local_state_;
  metrics::TestEnabledStateProvider enabled_state_provider_{/*consent=*/false,
                                                            /*enabled=*/false};
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::TestVariationsService> variations_service_;

  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

TEST_F(GlicEnablingNativeFreTest, FeatureFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kGlic, features::kGlicRollout},
      {features::kGlicNativeFreAndroid, features::kGlicLocaleFiltering});

  SignInAsAdult();
  EXPECT_FALSE(GlicEnabling::IsProfileEligibleForNativeFre(profile()));
}

TEST_F(GlicEnablingNativeFreTest, IneligibleProfilesReturnFalse) {
  EXPECT_FALSE(GlicEnabling::IsProfileEligibleForNativeFre(nullptr));
  EXPECT_FALSE(GlicEnabling::IsProfileEligibleForNativeFre(profile()));

  SignInAsAdult();
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_FALSE(GlicEnabling::IsProfileEligibleForNativeFre(otr_profile));
}

TEST_F(GlicEnablingNativeFreTest, AccountCapabilitiesAndEnterprise) {
  SignInWithUnknownCapabilities();
  EXPECT_FALSE(GlicEnabling::IsProfileEligibleForNativeFre(profile()));

  identity_test_env()->ClearPrimaryAccount();
  SignInAsMinor();
  EXPECT_FALSE(GlicEnabling::IsProfileEligibleForNativeFre(profile()));

  identity_test_env()->ClearPrimaryAccount();
  SignInAsAdult();
  EXPECT_TRUE(GlicEnabling::IsProfileEligibleForNativeFre(profile()));

  identity_test_env()->ClearPrimaryAccount();
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "enterprise_user@enterprise.com", signin::ConsentLevel::kSignin);
  identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
      account_info.GetAccountId(), account_info.GetEmail(),
      account_info.GetGaiaId(), "enterprise.com", "full_name", "given_name",
      "locale", "picture_url");
  account_info =
      identity_test_env()
          ->identity_manager()
          ->FindExtendedAccountInfoByAccountId(account_info.GetAccountId());
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_gemini_in_chrome(true);
  mutator.set_can_use_model_execution_features(true);
  signin::UpdateAccountInfoForAccount(identity_test_env()->identity_manager(),
                                      account_info);
  EXPECT_TRUE(GlicEnabling::IsEnterpriseAccount(profile()));
  EXPECT_TRUE(GlicEnabling::IsProfileEligibleForNativeFre(profile()));
}

TEST_F(GlicEnablingNativeFreTest, CountryFiltering) {
  SignInAsAdult();

  // Default enabled_countries ("us,in") matches case-insensitively.
  variations_service()->OverrideStoredPermanentCountry("us");
  EXPECT_TRUE(GlicEnabling::IsProfileEligibleForNativeFre(profile()));
  variations_service()->OverrideStoredPermanentCountry("IN");
  EXPECT_TRUE(GlicEnabling::IsProfileEligibleForNativeFre(profile()));
  variations_service()->OverrideStoredPermanentCountry("kr");
  EXPECT_FALSE(GlicEnabling::IsProfileEligibleForNativeFre(profile()));

  // Session country is ignored when kGlicUseSessionCountryForFiltering is off,
  // and OR'd with permanent country when on.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        features::kGlicUseSessionCountryForFiltering);
    variations_service()->OverrideStoredPermanentCountry("kr");
    local_state_.SetString(variations::prefs::kVariationsCountry, "us");
    EXPECT_FALSE(GlicEnabling::IsProfileEligibleForNativeFre(profile()));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kGlicUseSessionCountryForFiltering);
    variations_service()->OverrideStoredPermanentCountry("kr");
    local_state_.SetString(variations::prefs::kVariationsCountry, "IN");
    EXPECT_TRUE(GlicEnabling::IsProfileEligibleForNativeFre(profile()));
    local_state_.SetString(variations::prefs::kVariationsCountry, "");
  }

  // Custom enabled_countries param and "*" wildcard.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kGlicNativeFreAndroid, {{"enabled_countries", "gb,DE"}});
    variations_service()->OverrideStoredPermanentCountry("de");
    EXPECT_TRUE(GlicEnabling::IsProfileEligibleForNativeFre(profile()));
    variations_service()->OverrideStoredPermanentCountry("us");
    EXPECT_FALSE(GlicEnabling::IsProfileEligibleForNativeFre(profile()));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kGlicNativeFreAndroid, {{"enabled_countries", "*"}});
    variations_service()->OverrideStoredPermanentCountry("kr");
    EXPECT_TRUE(GlicEnabling::IsProfileEligibleForNativeFre(profile()));
  }
}

TEST_F(GlicEnablingNativeFreTest, GlicDevBypassesAdultAndCountryChecks) {
  SignInAsMinor();
  variations_service()->OverrideStoredPermanentCountry("kr");
  EXPECT_FALSE(GlicEnabling::IsProfileEligibleForNativeFre(profile()));

  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      ::switches::kGlicDev);
  EXPECT_TRUE(GlicEnabling::IsProfileEligibleForNativeFre(profile()));
}

class GlicEnablingNativeFreGeicTest : public GlicEnablingNativeFreTest {
 public:
  GlicEnablingNativeFreGeicTest() {
    geic_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGeic,
          {{"enabled", "true"},
           {"geic-guest-url", "https://business.gemini.google/home/cid/abc"}}},
         {features::kGeicFre, {}}},
        {});
  }

 private:
  base::test::ScopedFeatureList geic_feature_list_;
};

TEST_F(GlicEnablingNativeFreGeicTest, GeicProfileReturnsFalse) {
  SignInAsAdult();

  EXPECT_FALSE(GlicEnabling::IsProfileEligibleForNativeFre(profile()));
}

}  // namespace
}  // namespace glic

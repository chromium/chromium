// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_delegate.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/webapps/webapp_registry.h"
#endif

namespace {

constexpr char kTestEmail[] = "test@test.com";

}

class PrivacySandboxSettingsDelegateTest : public testing::Test {
 public:
  PrivacySandboxSettingsDelegateTest() {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    adapter_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    delegate_ =
        std::make_unique<PrivacySandboxSettingsDelegate>(profile_.get());
  }

 protected:
  void SetPrivacySandboxAccountCapability(const std::string& account,
                                          bool enabled) {
    auto account_info = identity_test_env()
                            ->identity_manager()
                            ->FindExtendedAccountInfoByEmailAddress(kTestEmail);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_run_chrome_privacy_sandbox_trials(enabled);
    signin::UpdateAccountInfoForAccount(identity_test_env()->identity_manager(),
                                        account_info);
  }

  void SetRestrictedNoticeCapability(const std::string& account, bool enabled) {
    auto account_info = identity_test_env()
                            ->identity_manager()
                            ->FindExtendedAccountInfoByEmailAddress(kTestEmail);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator
        .set_is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(
            enabled);
    signin::UpdateAccountInfoForAccount(identity_test_env()->identity_manager(),
                                        account_info);
  }

  PrivacySandboxSettingsDelegate* delegate() { return delegate_.get(); }
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }
  signin::IdentityTestEnvironment* identity_test_env() {
    return adapter_->identity_test_env();
  }
  TestingProfile* profile() { return profile_.get(); }
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> adapter_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<PrivacySandboxSettingsDelegate> delegate_;
};

TEST_F(PrivacySandboxSettingsDelegateTest,
       CapabilityRestrictionForSignedInUser) {
  // Sign the user in.
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  // Initially the account capability will be in an unknown state, which
  // should be interpreted as no restriction.
  EXPECT_FALSE(delegate()->IsPrivacySandboxRestricted());

  // When the capability is restricted, the delegate should return as such.
  SetPrivacySandboxAccountCapability(kTestEmail, false);
  EXPECT_TRUE(delegate()->IsPrivacySandboxRestricted());
  // Even when the capability is currently unrestricted, the sandbox should
  // remain restricted. The capability should be reported as currently
  // unrestricted.
  // TODO (crbug.com/1428546): Adjust when we have a graduation flow.
  SetPrivacySandboxAccountCapability(kTestEmail, true);
  EXPECT_TRUE(delegate()->IsPrivacySandboxRestricted());
  EXPECT_TRUE(delegate()->IsPrivacySandboxCurrentlyUnrestricted());
}

TEST_F(PrivacySandboxSettingsDelegateTest,
       CapabilityRestrictionForSignedOutUser) {
  feature_list()->InitAndEnableFeature(
      privacy_sandbox::kPrivacySandboxSettings3);
  // If the user is not signed in to Chrome then we don't use any age signal and
  // don't restrict the feature.
  EXPECT_FALSE(delegate()->IsPrivacySandboxRestricted());
  EXPECT_FALSE(delegate()->IsPrivacySandboxCurrentlyUnrestricted());
}

TEST_F(PrivacySandboxSettingsDelegateTest,
       RestrictedNoticeRequiredForSignedInUser) {
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4RestrictedNotice.name,
        "true"}});
  // Sign the user in.
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  // Initially the account capability will be in an unknown state, which
  // should be interpreted as no restriction.
  EXPECT_FALSE(delegate()->IsSubjectToM1NoticeRestricted());

  // Validate that the notice is not required when the account is not configured
  // to show it.
  SetRestrictedNoticeCapability(kTestEmail, false);
  EXPECT_FALSE(delegate()->IsSubjectToM1NoticeRestricted());

  // Validate that the notice is required when the account is configured to show
  // it.
  SetRestrictedNoticeCapability(kTestEmail, true);
  EXPECT_TRUE(delegate()->IsSubjectToM1NoticeRestricted());
}

TEST_F(PrivacySandboxSettingsDelegateTest,
       RestrictedNoticeRequiredWithoutAccountToken) {
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4RestrictedNotice.name,
        "true"}});
  // Sign the user in.
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  // Initially the account capability will be in an unknown state
  EXPECT_FALSE(delegate()->IsSubjectToM1NoticeRestricted());

  // Enable the account capability
  SetRestrictedNoticeCapability(kTestEmail, true);

  // Remove the refresh token for the account
  signin::RemoveRefreshTokenForPrimaryAccount(
      identity_test_env()->identity_manager());

  // Capability is fetched even if the token is not available
  EXPECT_TRUE(delegate()->IsSubjectToM1NoticeRestricted());
}

TEST_F(PrivacySandboxSettingsDelegateTest,
       RestrictedNoticeRequiredForSignedOutUser) {
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4RestrictedNotice.name,
        "true"}});
  // If the user is not signed in to Chrome then we don't use any age signal and
  // don't restrict the feature.
  EXPECT_FALSE(delegate()->IsSubjectToM1NoticeRestricted());
}

TEST_F(PrivacySandboxSettingsDelegateTest,
       RestrictedNoticeRequiredFeatureDisabled) {
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4RestrictedNotice.name,
        "false"}});
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);
  SetRestrictedNoticeCapability(kTestEmail, true);
  // Even if the user is signed in to Chrome, the feature being disabled means
  // no notice should be shown.
  EXPECT_FALSE(delegate()->IsSubjectToM1NoticeRestricted());
}

TEST_F(PrivacySandboxSettingsDelegateTest,
       AppropriateTopicsConsent_ConsentNotRequired) {
  // When the V4 consent required parameter is not present, Topics always has
  // an appropriate level of consent.
  prefs()->SetBoolean(prefs::kPrivacySandboxTopicsConsentGiven, false);
  feature_list()->InitAndEnableFeature(
      privacy_sandbox::kPrivacySandboxSettings4);

  EXPECT_TRUE(delegate()->HasAppropriateTopicsConsent());

  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4NoticeRequired.name, "true"}});

  EXPECT_TRUE(delegate()->HasAppropriateTopicsConsent());

  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{privacy_sandbox::kPrivacySandboxSettings3ConsentRequired.name,
        "true"}});

  EXPECT_TRUE(delegate()->HasAppropriateTopicsConsent());
}

TEST_F(PrivacySandboxSettingsDelegateTest,
       AppropriateTopicsConsent_ConsentRequired) {
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4ConsentRequired.name,
        "true"}});

  // Default state should be a not-active consent.
  EXPECT_FALSE(delegate()->HasAppropriateTopicsConsent());

  prefs()->SetBoolean(prefs::kPrivacySandboxTopicsConsentGiven, true);
  EXPECT_TRUE(delegate()->HasAppropriateTopicsConsent());

  prefs()->SetBoolean(prefs::kPrivacySandboxTopicsConsentGiven, false);
  EXPECT_FALSE(delegate()->HasAppropriateTopicsConsent());
}

TEST_F(PrivacySandboxSettingsDelegateTest,
       CapabilityRestrictionWhenForcedRestictedUser) {
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4ForceRestrictedUserForTesting
            .name,
        "true"}});
  EXPECT_TRUE(delegate()->IsPrivacySandboxRestricted());
  EXPECT_FALSE(delegate()->IsPrivacySandboxCurrentlyUnrestricted());
}

namespace {

const base::Time kCurrentTime = base::Time::Now();
const base::Time kValidInstallDate = kCurrentTime - base::Days(31);

#if BUILDFLAG(IS_ANDROID)
class MockWebappRegistry : public WebappRegistry {
 public:
  // WebappRegistry:
  MOCK_METHOD(std::vector<std::string>,
              GetOriginsWithInstalledApp,
              (),
              (override));
};
#endif

struct CookieDeprecationExperimentEligibilityTestCase {
  absl::optional<bool> is_subject_to_enterprise_policies;
  content_settings::CookieControlsMode cookie_controls_mode_pref =
      content_settings::CookieControlsMode::kOff;
  ContentSetting cookie_content_setting = ContentSetting::CONTENT_SETTING_ALLOW;
  bool privacy_sandbox_eea_notice_acknowledged_pref = false;
  bool privacy_sandbox_row_notice_acknowledged_pref = false;
  absl::optional<base::Time> install_date = kValidInstallDate;
#if BUILDFLAG(IS_ANDROID)
  std::vector<std::string> origins_with_installed_app;
#endif
  // The eligibility before the set up, which should be sticky.
  absl::optional<bool> expected_eligible_before;
  bool expected_eligible;
  bool expected_currently_eligible;
};

const CookieDeprecationExperimentEligibilityTestCase
    kCookieDeprecationExperimentEligibilityTestCases[] = {
        {
            .expected_eligible = false,
            .expected_currently_eligible = false,
        },
        {
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .expected_eligible = true,
            .expected_currently_eligible = true,
        },
        {
            .privacy_sandbox_row_notice_acknowledged_pref = true,
            .expected_eligible = true,
            .expected_currently_eligible = true,
        },
        {
            .cookie_controls_mode_pref =
                content_settings::CookieControlsMode::kBlockThirdParty,
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .expected_eligible = false,
            .expected_currently_eligible = false,
        },
        {
            .cookie_content_setting = ContentSetting::CONTENT_SETTING_BLOCK,
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .expected_eligible = false,
            .expected_currently_eligible = false,
        },
        {
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .install_date = absl::nullopt,
            .expected_eligible = false,
            .expected_currently_eligible = false,
        },
        {
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .install_date = kCurrentTime - base::Days(29),
            .expected_eligible = false,
            .expected_currently_eligible = false,
        },
        {
            .is_subject_to_enterprise_policies = true,
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .expected_eligible = false,
            .expected_currently_eligible = false,
        },
        {
            .is_subject_to_enterprise_policies = false,
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .expected_eligible = true,
            .expected_currently_eligible = true,
        },
#if BUILDFLAG(IS_ANDROID)
        {
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .origins_with_installed_app =
                std::vector<std::string>({"https://a.test"}),
            .expected_eligible = false,
            .expected_currently_eligible = false,
        },
#endif
        {
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .expected_eligible_before = false,
            .expected_eligible = false,
            .expected_currently_eligible = true,
        },
};

class CookieDeprecationExperimentEligibilityTest
    : public PrivacySandboxSettingsDelegateTest,
      public ::testing::WithParamInterface<
          CookieDeprecationExperimentEligibilityTestCase> {
 public:
  CookieDeprecationExperimentEligibilityTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {
#if BUILDFLAG(IS_ANDROID)
    auto webapp_registry = std::make_unique<MockWebappRegistry>();
    webapp_registry_ = webapp_registry.get();
    delegate()->OverrideWebappRegistryForTesting(std::move(webapp_registry));
#endif
  }

 protected:
  content_settings::CookieSettings* cookie_settings() {
    return CookieSettingsFactory::GetForProfile(profile()).get();
  }

  void SetSubjectToEnterprisePoliciesCapability(const std::string& account,
                                                bool enabled) {
    auto account_info = identity_test_env()
                            ->identity_manager()
                            ->FindExtendedAccountInfoByEmailAddress(kTestEmail);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_is_subject_to_enterprise_policies(enabled);
    signin::UpdateAccountInfoForAccount(identity_test_env()->identity_manager(),
                                        account_info);
  }

  ScopedTestingLocalState local_state_;
#if BUILDFLAG(IS_ANDROID)
  raw_ptr<MockWebappRegistry> webapp_registry_;
#endif
};

}  // namespace

TEST_P(CookieDeprecationExperimentEligibilityTest, IsEligible) {
  const CookieDeprecationExperimentEligibilityTestCase& test_case = GetParam();

  if (test_case.expected_eligible_before) {
    EXPECT_EQ(delegate()->IsCookieDeprecationExperimentEligible(),
              *test_case.expected_eligible_before);
  }

  if (test_case.is_subject_to_enterprise_policies.has_value()) {
    // Sign the user in.
    identity_test_env()->MakePrimaryAccountAvailable(
        kTestEmail, signin::ConsentLevel::kSignin);
    SetSubjectToEnterprisePoliciesCapability(
        kTestEmail, *test_case.is_subject_to_enterprise_policies);
  }

  prefs()->SetInteger(prefs::kCookieControlsMode,
                      static_cast<int>(test_case.cookie_controls_mode_pref));
  prefs()->SetBoolean(prefs::kPrivacySandboxM1RowNoticeAcknowledged,
                      test_case.privacy_sandbox_row_notice_acknowledged_pref);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1EEANoticeAcknowledged,
                      test_case.privacy_sandbox_eea_notice_acknowledged_pref);

  cookie_settings()->SetDefaultCookieSetting(test_case.cookie_content_setting);

  if (test_case.install_date.has_value()) {
    local_state_.Get()->SetInt64(metrics::prefs::kInstallDate,
                                 test_case.install_date->ToTimeT());
  }

#if BUILDFLAG(IS_ANDROID)
  ON_CALL(*webapp_registry_, GetOriginsWithInstalledApp)
      .WillByDefault(testing::Return(test_case.origins_with_installed_app));
#endif

  EXPECT_EQ(delegate()->IsCookieDeprecationExperimentCurrentlyEligible(),
            test_case.expected_currently_eligible);
  EXPECT_EQ(delegate()->IsCookieDeprecationExperimentEligible(),
            test_case.expected_eligible);
}

INSTANTIATE_TEST_SUITE_P(
    CookieDeprecationExperimentEligibility,
    CookieDeprecationExperimentEligibilityTest,
    ::testing::ValuesIn(kCookieDeprecationExperimentEligibilityTestCases));

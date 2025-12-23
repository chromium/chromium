// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_delegate.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/strings/to_string.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tpcd_experiment_eligibility.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/webapps/webapp_registry.h"
#endif

namespace {

constexpr char kTestEmail[] = "test@test.com";

class PrivacySandboxSettingsDelegateTest : public testing::Test {
 public:
  PrivacySandboxSettingsDelegateTest() {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    adapter_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    delegate_ = std::make_unique<PrivacySandboxSettingsDelegate>(
        profile_.get(), GetSingletonPrivacySandboxCountries());
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
  // If the user is not signed in to Chrome then we don't use any age signal and
  // don't restrict the feature.
  EXPECT_FALSE(delegate()->IsPrivacySandboxRestricted());
  EXPECT_FALSE(delegate()->IsPrivacySandboxCurrentlyUnrestricted());
}

TEST_F(PrivacySandboxSettingsDelegateTest,
       RestrictedNoticeRequiredForSignedInUser) {
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4RestrictedNoticeName,
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
      {{privacy_sandbox::kPrivacySandboxSettings4RestrictedNoticeName,
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
      {{privacy_sandbox::kPrivacySandboxSettings4RestrictedNoticeName,
        "true"}});
  // If the user is not signed in to Chrome then we don't use any age signal and
  // don't restrict the feature.
  EXPECT_FALSE(delegate()->IsSubjectToM1NoticeRestricted());
}

TEST_F(PrivacySandboxSettingsDelegateTest,
       RestrictedNoticeRequiredFeatureDisabled) {
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4RestrictedNoticeName,
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

using TpcdExperimentEligibility = privacy_sandbox::TpcdExperimentEligibility;

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
  bool force_eligible = false;
  bool exclude_3pc_blocked = true;
  bool exclude_not_seen_notice = true;
  bool exclude_dasher_account = true;
  bool exclude_new_user = true;
  std::string install_time_new_user = "30d";
#if BUILDFLAG(IS_ANDROID)
  bool exclude_pwa_twa_installed = true;
#endif
  std::optional<bool> is_subject_to_enterprise_features;
  content_settings::CookieControlsMode cookie_controls_mode_pref =
      content_settings::CookieControlsMode::kOff;
  ContentSetting cookie_content_setting = ContentSetting::CONTENT_SETTING_ALLOW;
  bool tracking_protection_3pcd_enabled_pref = false;
  bool privacy_sandbox_eea_notice_acknowledged_pref = false;
  bool privacy_sandbox_row_notice_acknowledged_pref = false;
  std::optional<base::Time> install_date = kValidInstallDate;
#if BUILDFLAG(IS_ANDROID)
  std::vector<std::string> origins_with_installed_app;
#endif
  // The eligibility before the set up, which should be sticky.
  std::optional<bool> expected_eligible_before;
  bool expected_eligible;
  TpcdExperimentEligibility::Reason expected_current_eligibility;
};

const CookieDeprecationExperimentEligibilityTestCase
    kCookieDeprecationExperimentEligibilityTestCases[] = {
        {
            .expected_eligible = false,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::kHasNotSeenNotice,
        },
        {
            .exclude_not_seen_notice = false,
            .expected_eligible = true,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::kEligible,
        },
        {
            .force_eligible = true,
            .expected_eligible = true,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::kForcedEligible,
        },
        {
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .expected_eligible = true,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::kEligible,
        },
        {
            .privacy_sandbox_row_notice_acknowledged_pref = true,
            .expected_eligible = true,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::kEligible,
        },
        {
            .cookie_controls_mode_pref =
                content_settings::CookieControlsMode::kBlockThirdParty,
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .expected_eligible = false,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::k3pCookiesBlocked,
        },
        {
            .exclude_3pc_blocked = false,
            .cookie_controls_mode_pref =
                content_settings::CookieControlsMode::kBlockThirdParty,
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .expected_eligible = true,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::kEligible,
        },
        {
            .cookie_content_setting = ContentSetting::CONTENT_SETTING_BLOCK,
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .expected_eligible = false,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::k3pCookiesBlocked,
        },
        {
            .tracking_protection_3pcd_enabled_pref = true,
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .expected_eligible = true,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::kEligible,
        },
        {
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .install_date = std::nullopt,
            .expected_eligible = false,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::kNewUser,
        },
        {
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .install_date = kCurrentTime - base::Days(29),
            .expected_eligible = false,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::kNewUser,
        },
        {
            .install_time_new_user = "4d",  // base::Days(4)
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .install_date = kCurrentTime - base::Days(5),
            .expected_eligible = true,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::kEligible,
        },
        {
            .exclude_new_user = false,
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .install_date = kCurrentTime - base::Days(5),
            .expected_eligible = true,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::kEligible,
        },
        {
            .is_subject_to_enterprise_features = true,
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .expected_eligible = false,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::kEnterpriseUser,
        },
        {
            .exclude_dasher_account = false,
            .is_subject_to_enterprise_features = true,
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .expected_eligible = true,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::kEligible,
        },
        {
            .is_subject_to_enterprise_features = false,
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .expected_eligible = true,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::kEligible,
        },
#if BUILDFLAG(IS_ANDROID)
        {
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .origins_with_installed_app =
                std::vector<std::string>({"https://a.test"}),
            .expected_eligible = false,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::kPwaOrTwaInstalled,
        },
#endif
        {
            .privacy_sandbox_eea_notice_acknowledged_pref = true,
            .expected_eligible_before = false,
            .expected_eligible = false,
            .expected_current_eligibility =
                TpcdExperimentEligibility::Reason::kEligible,
        },
};

class CookieDeprecationExperimentEligibilityTest
    : public PrivacySandboxSettingsDelegateTest,
      public ::testing::WithParamInterface<
          CookieDeprecationExperimentEligibilityTestCase> {
 public:
  CookieDeprecationExperimentEligibilityTest() {
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
    mutator.set_is_subject_to_enterprise_features(enabled);
    signin::UpdateAccountInfoForAccount(identity_test_env()->identity_manager(),
                                        account_info);
  }

#if BUILDFLAG(IS_ANDROID)
  raw_ptr<MockWebappRegistry> webapp_registry_;
#endif
};

// The parameter indicates whether to use per-profile filtering.
class CookieDeprecationExperimentEligibilityOTRProfileTest
    : public PrivacySandboxSettingsDelegateTest,
      public testing::WithParamInterface<bool> {};

// The parameter indicates whether to disable 3pcs.
class CookieDeprecationLabelAllowedTest
    : public PrivacySandboxSettingsDelegateTest,
      public testing::WithParamInterface<bool> {};

}  // namespace

TEST_P(CookieDeprecationExperimentEligibilityTest, IsEligible) {
  const CookieDeprecationExperimentEligibilityTestCase& test_case = GetParam();
  std::vector<base::test::FeatureRefAndParams> enabled_features = {
      {features::kCookieDeprecationFacilitatedTesting,
       {{"use_profile_filtering", "true"},
        {"force_eligible", base::ToString(test_case.force_eligible)},
        {tpcd::experiment::kExclude3PCBlockedName,
         base::ToString(test_case.exclude_3pc_blocked)},
        {tpcd::experiment::kExcludeNotSeenAdsAPIsNoticeName,
         base::ToString(test_case.exclude_not_seen_notice)},
        {tpcd::experiment::kExcludeDasherAccountName,
         base::ToString(test_case.exclude_dasher_account)},
        {tpcd::experiment::kExcludeNewUserName,
         base::ToString(test_case.exclude_new_user)},
        {tpcd::experiment::kInstallTimeForNewUserName,
         test_case.install_time_new_user},
#if BUILDFLAG(IS_ANDROID)
        {tpcd::experiment::kExcludePwaOrTwaInstalledName,
         base::ToString(test_case.exclude_pwa_twa_installed)}
#endif
       }}};
  if (test_case.tracking_protection_3pcd_enabled_pref) {
    enabled_features.push_back(
        {content_settings::features::kTrackingProtection3pcd, {}});
  }
  feature_list()->InitWithFeaturesAndParameters(enabled_features, {});

  if (test_case.expected_eligible_before) {
    EXPECT_EQ(delegate()->IsCookieDeprecationExperimentEligible(),
              *test_case.expected_eligible_before);
  }

  if (test_case.is_subject_to_enterprise_features.has_value()) {
    // Sign the user in.
    identity_test_env()->MakePrimaryAccountAvailable(
        kTestEmail, signin::ConsentLevel::kSignin);
    SetSubjectToEnterprisePoliciesCapability(
        kTestEmail, *test_case.is_subject_to_enterprise_features);
  }

  prefs()->SetInteger(prefs::kCookieControlsMode,
                      static_cast<int>(test_case.cookie_controls_mode_pref));
  prefs()->SetBoolean(prefs::kPrivacySandboxM1RowNoticeAcknowledged,
                      test_case.privacy_sandbox_row_notice_acknowledged_pref);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1EEANoticeAcknowledged,
                      test_case.privacy_sandbox_eea_notice_acknowledged_pref);

  cookie_settings()->SetDefaultCookieSetting(test_case.cookie_content_setting);

  if (test_case.install_date.has_value()) {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInt64(
        metrics::prefs::kInstallDate, test_case.install_date->ToTimeT());
  }

#if BUILDFLAG(IS_ANDROID)
  ON_CALL(*webapp_registry_, GetOriginsWithInstalledApp)
      .WillByDefault(testing::Return(test_case.origins_with_installed_app));
#endif

  EXPECT_EQ(
      delegate()->GetCookieDeprecationExperimentCurrentEligibility().reason(),
      test_case.expected_current_eligibility);
  EXPECT_EQ(delegate()->IsCookieDeprecationExperimentEligible(),
            test_case.expected_eligible);
}

INSTANTIATE_TEST_SUITE_P(
    CookieDeprecationExperimentEligibility,
    CookieDeprecationExperimentEligibilityTest,
    ::testing::ValuesIn(kCookieDeprecationExperimentEligibilityTestCases));

TEST_P(CookieDeprecationExperimentEligibilityOTRProfileTest, IsEligible) {
  Profile* off_the_record_profile = profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);
  PrivacySandboxSettingsDelegate otr_delegate_under_test(
      off_the_record_profile, GetSingletonPrivacySandboxCountries());

  // Android does not have guest profiles.
#if !BUILDFLAG(IS_ANDROID)
  auto guest_profile = TestingProfile::Builder().SetGuestSession().Build();
  PrivacySandboxSettingsDelegate guest_delegate_under_test(
      guest_profile.get(), GetSingletonPrivacySandboxCountries());
#endif  // !BUILDFLAG(IS_ANDROID)

  const bool use_profile_filtering = GetParam();

  const std::string use_profile_filtering_param =
      base::ToString(use_profile_filtering);

  {
    feature_list()->InitAndEnableFeatureWithParameters(
        features::kCookieDeprecationFacilitatedTesting,
        {{"force_eligible", "true"},
         {"use_profile_filtering", use_profile_filtering_param},
         {"enable_otr_profiles", "true"}});

    // Profile is only eligible if `use_profile_filtering` is true.
    // TODO(crbug.com/469047728): Remove this test entirely after the
    // `use_profile_filtering` eligibility check is removed.
    EXPECT_EQ(otr_delegate_under_test.IsCookieDeprecationExperimentEligible(),
              use_profile_filtering);
#if !BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(guest_delegate_under_test.IsCookieDeprecationExperimentEligible(),
              use_profile_filtering);
#endif  // !BUILDFLAG(IS_ANDROID)

    feature_list()->Reset();
  }

  {
    feature_list()->InitAndEnableFeatureWithParameters(
        features::kCookieDeprecationFacilitatedTesting,
        {{"force_eligible", "true"},
         {"use_profile_filtering", use_profile_filtering_param},
         {"enable_otr_profiles", "false"}});
    EXPECT_FALSE(
        otr_delegate_under_test.IsCookieDeprecationExperimentEligible());

#if !BUILDFLAG(IS_ANDROID)
    EXPECT_FALSE(
        guest_delegate_under_test.IsCookieDeprecationExperimentEligible());
#endif  // !BUILDFLAG(IS_ANDROID)

    feature_list()->Reset();
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         CookieDeprecationExperimentEligibilityOTRProfileTest,
                         testing::Bool());

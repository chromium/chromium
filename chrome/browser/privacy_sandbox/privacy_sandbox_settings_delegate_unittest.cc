// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_delegate.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

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

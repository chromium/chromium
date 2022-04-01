// Copyright 2022 The Chromium Authors. All rights reserved.
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
    identity_test_env()->MakePrimaryAccountAvailable(
        kTestEmail, signin::ConsentLevel::kSignin);
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

TEST_F(PrivacySandboxSettingsDelegateTest, CapabilityRestriction) {
  feature_list()->InitAndEnableFeature(
      privacy_sandbox::kPrivacySandboxSettings3);
  // Initially the account capability will be in an unknown state, which
  // should be interpreted as no restriction.
  EXPECT_FALSE(delegate()->IsPrivacySandboxRestricted());

  // Altering the capability to either enabled or disabled should be reflected
  // as a restriction on the sandbox.
  SetPrivacySandboxAccountCapability(kTestEmail, false);
  EXPECT_TRUE(delegate()->IsPrivacySandboxRestricted());
  SetPrivacySandboxAccountCapability(kTestEmail, true);
  EXPECT_FALSE(delegate()->IsPrivacySandboxRestricted());

  // If the Privacy Sandbox Settings 3 feature is disabled the capability
  // restriction should not apply.
  feature_list()->Reset();
  feature_list()->InitAndDisableFeature(
      privacy_sandbox::kPrivacySandboxSettings3);
  SetPrivacySandboxAccountCapability(kTestEmail, false);
  EXPECT_FALSE(delegate()->IsPrivacySandboxRestricted());
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
TEST_F(PrivacySandboxSettingsDelegateTest, SupervisedUser) {
  // The sandbox should always be restricted for supervised users.
  profile()->SetIsSupervisedProfile();
  feature_list()->InitAndEnableFeature(
      privacy_sandbox::kPrivacySandboxSettings3);
  EXPECT_TRUE(delegate()->IsPrivacySandboxRestricted());

  // The capability should not override profile supervision.
  SetPrivacySandboxAccountCapability(kTestEmail, true);
  EXPECT_TRUE(delegate()->IsPrivacySandboxRestricted());

  // If the Privacy Sandbox Settings 3 feature is disabled, the supervised
  // user restriction should not apply.
  feature_list()->Reset();
  feature_list()->InitAndDisableFeature(
      privacy_sandbox::kPrivacySandboxSettings3);
  EXPECT_FALSE(delegate()->IsPrivacySandboxRestricted());
}
#endif

TEST_F(PrivacySandboxSettingsDelegateTest, Confirmation_Release3Enabled) {
  feature_list()->InitAndEnableFeature(
      privacy_sandbox::kPrivacySandboxSettings3);
  EXPECT_FALSE(delegate()->IsPrivacySandboxConfirmed());

  // Manually controlling V1 should not count as confirmation, while V2 should.
  prefs()->SetBoolean(prefs::kPrivacySandboxManuallyControlled, true);
  EXPECT_FALSE(delegate()->IsPrivacySandboxConfirmed());
  prefs()->SetBoolean(prefs::kPrivacySandboxManuallyControlledV2, true);
  EXPECT_TRUE(delegate()->IsPrivacySandboxConfirmed());
  prefs()->SetBoolean(prefs::kPrivacySandboxManuallyControlledV2, false);

  // A non-user controlled Privacy Sandbox should count as confirmation.
  prefs()->SetManagedPref(prefs::kPrivacySandboxApisEnabledV2,
                          base::Value(true));
  EXPECT_TRUE(delegate()->IsPrivacySandboxConfirmed());
  prefs()->RemoveManagedPref(prefs::kPrivacySandboxApisEnabledV2);
  EXPECT_FALSE(delegate()->IsPrivacySandboxConfirmed());

  // While a consent is not required, seeing a notice should suffice.
  prefs()->SetBoolean(prefs::kPrivacySandboxNoticeDisplayed, true);
  EXPECT_TRUE(delegate()->IsPrivacySandboxConfirmed());

  // If a notice is required, it should not suffice.
  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{"consent-required", "true"}});
  EXPECT_FALSE(delegate()->IsPrivacySandboxConfirmed());
  prefs()->SetBoolean(prefs::kPrivacySandboxConsentDecisionMade, true);
  EXPECT_TRUE(delegate()->IsPrivacySandboxConfirmed());
}

TEST_F(PrivacySandboxSettingsDelegateTest, Confirmation_Release3Disabled) {
  // If the Privacy Sandbox Settings 3 feature is disabled, no confirmation is
  // required.
  feature_list()->InitAndDisableFeature(
      privacy_sandbox::kPrivacySandboxSettings3);
  EXPECT_TRUE(delegate()->IsPrivacySandboxConfirmed());
}

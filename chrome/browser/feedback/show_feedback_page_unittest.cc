// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/show_feedback_page.h"

#include "base/test/with_feature_override.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

class ShowFeedbackPageTest : public testing::Test,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
                             public base::test::WithFeatureOverride
#else
                             public testing::WithParamInterface<bool>
#endif
{
 public:
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  ShowFeedbackPageTest()
      : base::test::WithFeatureOverride(features::kFeedbackDisabledDialog) {}
#else
  ShowFeedbackPageTest() = default;
#endif

  bool IsFeedbackDisabledDialogEnabled() const {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    return IsParamFeatureEnabled();
#else
    return false;
#endif
  }

 protected:
  void SetUp() override {
    TestingProfile::Builder builder;
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

TEST_P(ShowFeedbackPageTest, CanShowFeedback_NoProfile) {
  EXPECT_FALSE(CanSubmitFeedback(nullptr));
  EXPECT_FALSE(CanShowFeedback(nullptr));
}

TEST_P(ShowFeedbackPageTest, CanShowFeedback_PolicyDisabled) {
  profile_->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed, false);
  EXPECT_FALSE(CanSubmitFeedback(profile_.get()));
  EXPECT_EQ(IsFeedbackDisabledDialogEnabled(), CanShowFeedback(profile_.get()));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_P(ShowFeedbackPageTest, CanShowFeedback_NotSignedIn) {
  EXPECT_TRUE(CanSubmitFeedback(profile_.get()));
  EXPECT_TRUE(CanShowFeedback(profile_.get()));
}

TEST_P(ShowFeedbackPageTest, CanShowFeedback_CanSubmit) {
  AccountInfo account_info =
      identity_test_env_adaptor_->identity_test_env()
          ->MakePrimaryAccountAvailable("test@example.com",
                                        signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_submit_feedback(true);
  signin::UpdateAccountInfoForAccount(
      identity_test_env_adaptor_->identity_test_env()->identity_manager(),
      account_info);

  EXPECT_TRUE(CanSubmitFeedback(profile_.get()));
  EXPECT_TRUE(CanShowFeedback(profile_.get()));
}

TEST_P(ShowFeedbackPageTest, CanShowFeedback_CannotSubmit) {
  AccountInfo account_info =
      identity_test_env_adaptor_->identity_test_env()
          ->MakePrimaryAccountAvailable("test@example.com",
                                        signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_submit_feedback(false);
  signin::UpdateAccountInfoForAccount(
      identity_test_env_adaptor_->identity_test_env()->identity_manager(),
      account_info);

  EXPECT_FALSE(CanSubmitFeedback(profile_.get()));
  EXPECT_EQ(IsFeedbackDisabledDialogEnabled(), CanShowFeedback(profile_.get()));
}

TEST_P(ShowFeedbackPageTest, CanShowFeedback_CanSubmit_Incognito) {
  AccountInfo account_info =
      identity_test_env_adaptor_->identity_test_env()
          ->MakePrimaryAccountAvailable("test@example.com",
                                        signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_submit_feedback(true);
  signin::UpdateAccountInfoForAccount(
      identity_test_env_adaptor_->identity_test_env()->identity_manager(),
      account_info);

  Profile* incognito_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_TRUE(CanSubmitFeedback(incognito_profile));
  EXPECT_TRUE(CanShowFeedback(incognito_profile));
}

TEST_P(ShowFeedbackPageTest, CanShowFeedback_CannotSubmit_Incognito) {
  AccountInfo account_info =
      identity_test_env_adaptor_->identity_test_env()
          ->MakePrimaryAccountAvailable("test@example.com",
                                        signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_submit_feedback(false);
  signin::UpdateAccountInfoForAccount(
      identity_test_env_adaptor_->identity_test_env()->identity_manager(),
      account_info);

  Profile* incognito_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_FALSE(CanSubmitFeedback(incognito_profile));
  EXPECT_EQ(IsFeedbackDisabledDialogEnabled(),
            CanShowFeedback(incognito_profile));
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(ShowFeedbackPageTest);
#else
TEST_P(ShowFeedbackPageTest, CanShowFeedback_OtherPlatforms) {
  EXPECT_TRUE(CanSubmitFeedback(profile_.get()));
  EXPECT_TRUE(CanShowFeedback(profile_.get()));
}

INSTANTIATE_TEST_SUITE_P(All, ShowFeedbackPageTest, testing::Values(false));
#endif

}  // namespace chrome

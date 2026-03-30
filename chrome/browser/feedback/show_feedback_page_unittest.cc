// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/show_feedback_page.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

class ShowFeedbackPageTest : public testing::Test {
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

TEST_F(ShowFeedbackPageTest, CanShowFeedback_NoProfile) {
  EXPECT_FALSE(CanShowFeedback(nullptr));
}

TEST_F(ShowFeedbackPageTest, CanShowFeedback_PolicyDisabled) {
  profile_->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed, false);
  EXPECT_FALSE(CanShowFeedback(profile_.get()));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_F(ShowFeedbackPageTest, CanShowFeedback_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      switches::kDisableU18FeedbackDesktop);

  EXPECT_TRUE(CanShowFeedback(profile_.get()));
}

TEST_F(ShowFeedbackPageTest,
       CanShowFeedback_FeatureEnabled_Enabled_NotSignedIn) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kDisableU18FeedbackDesktop};
  EXPECT_TRUE(CanShowFeedback(profile_.get()));
}

TEST_F(ShowFeedbackPageTest, CanShowFeedback_FeatureEnabled_Enabled_CanSubmit) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kDisableU18FeedbackDesktop};

  AccountInfo account_info =
      identity_test_env_adaptor_->identity_test_env()
          ->MakePrimaryAccountAvailable("test@example.com",
                                        signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_submit_feedback(true);
  signin::UpdateAccountInfoForAccount(
      identity_test_env_adaptor_->identity_test_env()->identity_manager(),
      account_info);

  EXPECT_TRUE(CanShowFeedback(profile_.get()));
}

TEST_F(ShowFeedbackPageTest,
       CanShowFeedback_FeatureEnabled_Enabled_CannotSubmit) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kDisableU18FeedbackDesktop};

  AccountInfo account_info =
      identity_test_env_adaptor_->identity_test_env()
          ->MakePrimaryAccountAvailable("test@example.com",
                                        signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_submit_feedback(false);
  signin::UpdateAccountInfoForAccount(
      identity_test_env_adaptor_->identity_test_env()->identity_manager(),
      account_info);

  EXPECT_FALSE(CanShowFeedback(profile_.get()));
}

TEST_F(ShowFeedbackPageTest,
       CanShowFeedback_FeatureEnabled_Enabled_CanSubmit_Incognito) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kDisableU18FeedbackDesktop};

  AccountInfo account_info =
      identity_test_env_adaptor_->identity_test_env()
          ->MakePrimaryAccountAvailable("test@example.com",
                                        signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_submit_feedback(true);
  signin::UpdateAccountInfoForAccount(
      identity_test_env_adaptor_->identity_test_env()->identity_manager(),
      account_info);

  Profile* incognito_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_TRUE(CanShowFeedback(incognito_profile));
}

TEST_F(ShowFeedbackPageTest,
       CanShowFeedback_FeatureEnabled_Enabled_CannotSubmit_Incognito) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kDisableU18FeedbackDesktop};

  AccountInfo account_info =
      identity_test_env_adaptor_->identity_test_env()
          ->MakePrimaryAccountAvailable("test@example.com",
                                        signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_submit_feedback(false);
  signin::UpdateAccountInfoForAccount(
      identity_test_env_adaptor_->identity_test_env()->identity_manager(),
      account_info);

  Profile* incognito_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_FALSE(CanShowFeedback(incognito_profile));
}
#else
TEST_F(ShowFeedbackPageTest, CanShowFeedback_OtherPlatforms) {
  EXPECT_TRUE(CanShowFeedback(profile_.get()));
}
#endif

}  // namespace chrome

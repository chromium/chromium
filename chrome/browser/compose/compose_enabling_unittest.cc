// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/compose/compose_enabling.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/compose/core/browser/compose_features.h"  // nogncheck - https://crbug.com/1125897
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kEmail[] = "example@gmail.com";
}  // namespace

class ComposeEnablingTest : public testing::Test {
 public:
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    test_profile_ = profile_manager_->CreateTestingProfile("test");
  }

  void TearDown() override {
    test_profile_ = nullptr;
    profile_manager_.reset();
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  void SignIn(signin::ConsentLevel consent_level) {
    identity_test_env_.MakePrimaryAccountAvailable(kEmail, consent_level);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  bool IsEnabled(Profile* profile, signin::IdentityManager* identity_manager) {
    return ComposeEnabling::IsEnabled(profile, identity_manager);
  }

  void SetMsbbState(bool new_state) {
    PrefService* prefs = test_profile_->GetPrefs();
    prefs->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        new_state);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TestingProfile> test_profile_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;

 private:
  std::unique_ptr<TestingProfileManager> profile_manager_;
};

TEST_F(ComposeEnablingTest, EverythingDisabledTest) {
  scoped_feature_list_.InitAndDisableFeature(compose::features::kEnableCompose);
  // We intentionally don't call sign in to make our state not signed in.
  SetMsbbState(false);
  EXPECT_FALSE(IsEnabled(test_profile_, identity_test_env_.identity_manager()));
}

TEST_F(ComposeEnablingTest, FeatureNotEnabledTest) {
  // Ensure feature flag is off.
  scoped_feature_list_.InitAndDisableFeature(compose::features::kEnableCompose);
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);
  // Turn on MSBB.
  SetMsbbState(true);

  EXPECT_FALSE(IsEnabled(test_profile_, identity_test_env_.identity_manager()));
}

TEST_F(ComposeEnablingTest, MsbbDisabledTest) {
  // Turn on our chrome feature
  scoped_feature_list_.InitAndEnableFeature(compose::features::kEnableCompose);
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);
  // MSBB turned off.
  SetMsbbState(false);
  EXPECT_FALSE(IsEnabled(test_profile_, identity_test_env_.identity_manager()));
}

TEST_F(ComposeEnablingTest, NotSignedInTest) {
  // Turn on our chrome feature
  scoped_feature_list_.InitAndEnableFeature(compose::features::kEnableCompose);
  // Turn on MSBB.
  SetMsbbState(true);
  EXPECT_FALSE(IsEnabled(test_profile_, identity_test_env_.identity_manager()));
}

TEST_F(ComposeEnablingTest, EverythingEnabledTest) {
  // Turn on our chrome feature
  scoped_feature_list_.InitAndEnableFeature(compose::features::kEnableCompose);
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);
  // Turn on MSBB.
  SetMsbbState(true);
  EXPECT_TRUE(IsEnabled(test_profile_, identity_test_env_.identity_manager()));
}

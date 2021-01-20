// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_global_error.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_global_error_factory.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

static const char kTestEmail[] = "testuser@test.com";

class SigninGlobalErrorTest : public testing::Test {
 public:
  SigninGlobalErrorTest() :
      profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    // Create a signed-in profile.
    TestingProfile::TestingFactories testing_factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();

    profile_ = profile_manager_.CreateTestingProfile(
        "Person 1", std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::UTF8ToUTF16("Person 1"), 0, std::string(),
        std::move(testing_factories));

    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

    AccountInfo account_info =
        identity_test_env_profile_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable(kTestEmail);
    ProfileAttributesEntry* entry;
    ASSERT_TRUE(profile_manager_.profile_attributes_storage()->
        GetProfileAttributesWithPath(profile()->GetPath(), &entry));

    entry->SetAuthInfo(account_info.gaia, base::UTF8ToUTF16(kTestEmail),
                       /*is_consented_primary_account=*/true);

    global_error_ = SigninGlobalErrorFactory::GetForProfile(profile());
    error_controller_ = SigninErrorControllerFactory::GetForProfile(profile());
  }

  TestingProfile* profile() { return profile_; }
  TestingProfileManager* testing_profile_manager() {
    return &profile_manager_;
  }

  SigninGlobalError* global_error() { return global_error_; }
  SigninErrorController* error_controller() { return error_controller_; }

  void SetAuthError(GoogleServiceAuthError::State state) {
    signin::IdentityTestEnvironment* identity_test_env =
        identity_test_env_profile_adaptor_->identity_test_env();
    CoreAccountId primary_account_id =
        identity_test_env->identity_manager()->GetPrimaryAccountId();

    signin::UpdatePersistentErrorOfRefreshTokenForAccount(
        identity_test_env->identity_manager(), primary_account_id,
        GoogleServiceAuthError(state));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;

  SigninGlobalError* global_error_;
  SigninErrorController* error_controller_;
};

TEST_F(SigninGlobalErrorTest, Basic) {
  ASSERT_FALSE(global_error()->HasMenuItem());

  SetAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_TRUE(global_error()->HasMenuItem());

  SetAuthError(GoogleServiceAuthError::NONE);
  EXPECT_FALSE(global_error()->HasMenuItem());
}

// Verify that SigninGlobalError ignores certain errors.
TEST_F(SigninGlobalErrorTest, AuthStatusEnumerateAllErrors) {
  typedef struct {
    GoogleServiceAuthError::State error_state;
    bool is_error;
  } ErrorTableEntry;

  ErrorTableEntry table[] = {
      {GoogleServiceAuthError::NONE, false},
      {GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS, true},
      {GoogleServiceAuthError::USER_NOT_SIGNED_UP, true},
      {GoogleServiceAuthError::CONNECTION_FAILED, false},
      {GoogleServiceAuthError::SERVICE_UNAVAILABLE, false},
      {GoogleServiceAuthError::REQUEST_CANCELED, false},
      {GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE, true},
      {GoogleServiceAuthError::SERVICE_ERROR, true},
  };
  static_assert(
      base::size(table) == GoogleServiceAuthError::NUM_STATES -
                               GoogleServiceAuthError::kDeprecatedStateCount,
      "table size should match number of auth error types");

  // Mark the profile with an active timestamp so profile_metrics logs it.
  testing_profile_manager()->UpdateLastUser(profile());

  for (ErrorTableEntry entry : table) {
    SetAuthError(GoogleServiceAuthError::NONE);

    base::HistogramTester histogram_tester;
    SetAuthError(entry.error_state);

    EXPECT_EQ(global_error()->HasMenuItem(), entry.is_error);
    EXPECT_EQ(global_error()->MenuItemLabel().empty(), !entry.is_error);
    EXPECT_EQ(global_error()->GetBubbleViewMessages().empty(), !entry.is_error);
    EXPECT_FALSE(global_error()->GetBubbleViewTitle().empty());
    EXPECT_FALSE(global_error()->GetBubbleViewAcceptButtonLabel().empty());
    EXPECT_TRUE(global_error()->GetBubbleViewCancelButtonLabel().empty());

    ProfileMetrics::LogNumberOfProfiles(&testing_profile_manager()
                                             ->profile_manager()
                                             ->GetProfileAttributesStorage());

    if (entry.is_error) {
      histogram_tester.ExpectBucketCount("Signin.AuthError", entry.error_state,
                                         1);
    }
    histogram_tester.ExpectBucketCount("Profile.NumberOfProfilesWithAuthErrors",
                                       entry.is_error, 1);
  }
}

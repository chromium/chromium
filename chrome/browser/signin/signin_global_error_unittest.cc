// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_global_error.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_global_error_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

static const char kTestAccountId[] = "id-testuser@test.com";
static const char kTestGaiaId[] = "gaiaid-testuser@test.com";
static const char kTestUsername[] = "testuser@test.com";

class SigninGlobalErrorTest : public testing::Test {
 public:
  SigninGlobalErrorTest() :
      profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    // Create a signed-in profile.
    TestingProfile::TestingFactories testing_factories;
    testing_factories.emplace_back(
        SigninManagerFactory::GetInstance(),
        base::BindRepeating(&BuildFakeSigninManagerForTesting));
    profile_ = profile_manager_.CreateTestingProfile(
        "Person 1", std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::UTF8ToUTF16("Person 1"), 0, std::string(),
        std::move(testing_factories));

    SigninManagerFactory::GetForProfile(profile())
        ->SetAuthenticatedAccountInfo(kTestAccountId, kTestUsername);
    ProfileAttributesEntry* entry;
    ASSERT_TRUE(profile_manager_.profile_attributes_storage()->
        GetProfileAttributesWithPath(profile()->GetPath(), &entry));
    entry->SetAuthInfo(kTestGaiaId, base::UTF8ToUTF16(kTestUsername));

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
    ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile());
    token_service->UpdateCredentials(kTestAccountId, "refresh_token");
    // TODO(https://crbug.com/836212): Do not use the delegate directly, because
    // it is internal API.
    token_service->GetDelegate()->UpdateAuthError(
        kTestAccountId, GoogleServiceAuthError(state));
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
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
    { GoogleServiceAuthError::NONE, false },
    { GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS, true },
    { GoogleServiceAuthError::USER_NOT_SIGNED_UP, true },
    { GoogleServiceAuthError::CONNECTION_FAILED, false },
    { GoogleServiceAuthError::CAPTCHA_REQUIRED, true },
    { GoogleServiceAuthError::ACCOUNT_DELETED, true },
    { GoogleServiceAuthError::ACCOUNT_DISABLED, true },
    { GoogleServiceAuthError::SERVICE_UNAVAILABLE, false },
    { GoogleServiceAuthError::TWO_FACTOR, true },
    { GoogleServiceAuthError::REQUEST_CANCELED, false },
    { GoogleServiceAuthError::HOSTED_NOT_ALLOWED_DEPRECATED, false },
    { GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE, true },
    { GoogleServiceAuthError::SERVICE_ERROR, true },
    { GoogleServiceAuthError::WEB_LOGIN_REQUIRED, true },
  };
  static_assert(arraysize(table) == GoogleServiceAuthError::NUM_STATES,
      "table size should match number of auth error types");

  // Mark the profile with an active timestamp so profile_metrics logs it.
  testing_profile_manager()->UpdateLastUser(profile());

  for (size_t i = 0; i < arraysize(table); ++i) {
    if (GoogleServiceAuthError::IsDeprecated(table[i].error_state))
      continue;
    SetAuthError(GoogleServiceAuthError::NONE);

    base::HistogramTester histogram_tester;
    SetAuthError(table[i].error_state);

    EXPECT_EQ(global_error()->HasMenuItem(), table[i].is_error);
    EXPECT_EQ(global_error()->MenuItemLabel().empty(), !table[i].is_error);
    EXPECT_EQ(global_error()->GetBubbleViewMessages().empty(),
              !table[i].is_error);
    EXPECT_FALSE(global_error()->GetBubbleViewTitle().empty());
    EXPECT_FALSE(global_error()->GetBubbleViewAcceptButtonLabel().empty());
    EXPECT_TRUE(global_error()->GetBubbleViewCancelButtonLabel().empty());

    ProfileMetrics::LogNumberOfProfiles(
        testing_profile_manager()->profile_manager());

    if (table[i].is_error)
      histogram_tester.ExpectBucketCount("Signin.AuthError", i, 1);
    histogram_tester.ExpectBucketCount(
        "Profile.NumberOfProfilesWithAuthErrors", table[i].is_error, 1);
  }
}

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<KeyedService> BuildTestSigninClient(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<TestSigninClient>(profile->GetPrefs());
}

}  // namespace

class ChildAccountServiceTest : public ::testing::Test {
 public:
  ChildAccountServiceTest() = default;

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ChromeSigninClientFactory::GetInstance(),
                              base::BindRepeating(&BuildTestSigninClient));
    builder.SetIsSupervisedProfile();

    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);

    adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(&*profile_);

    child_account_service_ =
        ChildAccountServiceFactory::GetForProfile(&*profile_);
  }

 protected:
  network::TestURLLoaderFactory* GetTestURLLoaderFactory() {
    auto* signin_client =
        ChromeSigninClientFactory::GetForProfile(profile_.get());
    return static_cast<TestSigninClient*>(signin_client)
        ->GetTestURLLoaderFactory();
  }

  signin::IdentityTestEnvironment* identity_test_environment() {
    return adaptor_->identity_test_env();
  }

  signin::AccountsCookieMutator* GetAccountsCookieMutator() {
    return identity_test_environment()
        ->identity_manager()
        ->GetAccountsCookieMutator();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> adaptor_;
  signin::IdentityTestEnvironment identity_test_environment_;

  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<ChildAccountService> child_account_service_;
};

TEST_F(ChildAccountServiceTest, GetGoogleAuthState) {
  auto* accounts_cookie_mutator = GetAccountsCookieMutator();
  auto* test_url_loader_factory = GetTestURLLoaderFactory();

  signin::SetListAccountsResponseNoAccounts(test_url_loader_factory);

  // Initial state should be PENDING.
  EXPECT_EQ(ChildAccountService::AuthState::PENDING,
            child_account_service_->GetGoogleAuthState());

  // Wait until the response to the ListAccount request triggered by the call
  // above comes back.
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(ChildAccountService::AuthState::NOT_AUTHENTICATED,
            child_account_service_->GetGoogleAuthState());

  // A valid, signed-in account means authenticated.
  signin::SetListAccountsResponseOneAccountWithParams({"me@example.com",
                                                       /*gaia_id=*/"abcdef",
                                                       /*valid= */ true,
                                                       /*signed_out=*/false,
                                                       /*verified=*/true},
                                                      test_url_loader_factory);
  accounts_cookie_mutator->TriggerCookieJarUpdate();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(ChildAccountService::AuthState::AUTHENTICATED,
            child_account_service_->GetGoogleAuthState());

  // An invalid (but signed-in) account means not authenticated.
  signin::SetListAccountsResponseOneAccountWithParams(
      {"me@example.com", /*gaia_id=*/"abcdef",
       /*valid=*/false,
       /*signed_out=*/false,
       /*verified=*/true},
      test_url_loader_factory);
  accounts_cookie_mutator->TriggerCookieJarUpdate();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(ChildAccountService::AuthState::NOT_AUTHENTICATED,
            child_account_service_->GetGoogleAuthState());

  // A valid but not signed-in account means not authenticated.
  signin::SetListAccountsResponseOneAccountWithParams(
      {"me@example.com", /*gaia_id=*/"abcdef",
       /*valid=*/true,
       /*signed_out=*/true,
       /*verified=*/true},
      test_url_loader_factory);
  accounts_cookie_mutator->TriggerCookieJarUpdate();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(ChildAccountService::AuthState::NOT_AUTHENTICATED,
            child_account_service_->GetGoogleAuthState());
}

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
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

using ::testing::IsEmpty;
using ::testing::Not;

std::unique_ptr<KeyedService> BuildTestSigninClient(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<TestSigninClient>(profile->GetPrefs());
}

struct FamilyMember {
  std::string display_name;
  std::string email;
  std::string profile_url;
  std::string profile_image_url;
  std::string gaia_id;
};
void SetFamilyMemberAttributes(
    kids_chrome_management::FamilyMember* mutable_member,
    const FamilyMember& description,
    kids_chrome_management::FamilyRole role) {
  mutable_member->mutable_profile()->set_display_name(description.display_name);
  mutable_member->mutable_profile()->set_email(description.email);
  mutable_member->mutable_profile()->set_profile_url(description.profile_url);
  mutable_member->mutable_profile()->set_profile_image_url(
      description.profile_image_url);
  mutable_member->set_role(role);
  mutable_member->set_user_id(description.gaia_id);
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
    child_account_service_ =
        ChildAccountServiceFactory::GetForProfile(profile_.get());
  }

 protected:
  network::TestURLLoaderFactory* GetTestURLLoaderFactory() {
    auto* signin_client =
        ChromeSigninClientFactory::GetForProfile(profile_.get());
    return static_cast<TestSigninClient*>(signin_client)
        ->GetTestURLLoaderFactory();
  }

  signin::AccountsCookieMutator* GetAccountsCookieMutator() {
    IdentityTestEnvironmentProfileAdaptor identity_test_env_profile_adaptor(
        profile_.get());
    return identity_test_env_profile_adaptor.identity_test_env()
        ->identity_manager()
        ->GetAccountsCookieMutator();
  }

  // Assumes a successful response from a family info fetch with the given list
  // of family members.
  void OnGetFamilyMembersSuccess(
      const kids_chrome_management::ListFamilyMembersResponse& family_members) {
    child_account_service_->OnSuccess(family_members);
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<ChildAccountService> child_account_service_ = nullptr;
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
  signin::SetListAccountsResponseOneAccountWithParams(
      {"me@example.com", "abcdef",
       /* valid = */ true,
       /* is_signed_out = */ false,
       /* verified = */ true},
      test_url_loader_factory);
  accounts_cookie_mutator->TriggerCookieJarUpdate();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(ChildAccountService::AuthState::AUTHENTICATED,
            child_account_service_->GetGoogleAuthState());

  // An invalid (but signed-in) account means not authenticated.
  signin::SetListAccountsResponseOneAccountWithParams(
      {"me@example.com", "abcdef",
       /* valid = */ false,
       /* is_signed_out = */ false,
       /* verified = */ true},
      test_url_loader_factory);
  accounts_cookie_mutator->TriggerCookieJarUpdate();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(ChildAccountService::AuthState::NOT_AUTHENTICATED,
            child_account_service_->GetGoogleAuthState());

  // A valid but not signed-in account means not authenticated.
  signin::SetListAccountsResponseOneAccountWithParams(
      {"me@example.com", "abcdef",
       /* valid = */ true,
       /* is_signed_out = */ true,
       /* verified = */ true},
      test_url_loader_factory);
  accounts_cookie_mutator->TriggerCookieJarUpdate();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(ChildAccountService::AuthState::NOT_AUTHENTICATED,
            child_account_service_->GetGoogleAuthState());
}

TEST_F(ChildAccountServiceTest, StartFetchingFamilyInfo) {
  kids_chrome_management::ListFamilyMembersResponse
      list_family_members_response;
  SetFamilyMemberAttributes(list_family_members_response.add_members(),
                            {.display_name = "Marge Simpson"},
                            kids_chrome_management::HEAD_OF_HOUSEHOLD);
  SetFamilyMemberAttributes(list_family_members_response.add_members(),
                            {.display_name = "Homer Simpson"},
                            kids_chrome_management::PARENT);

  OnGetFamilyMembersSuccess(list_family_members_response);

  EXPECT_EQ("Marge Simpson", profile_->GetPrefs()->GetString(
                                 prefs::kSupervisedUserCustodianName));
  EXPECT_EQ("Homer Simpson", profile_->GetPrefs()->GetString(
                                 prefs::kSupervisedUserSecondCustodianName));
}

TEST_F(ChildAccountServiceTest, FieldsAreClearedForNonChildAccounts) {
  {
    kids_chrome_management::ListFamilyMembersResponse
        list_family_members_response;
    SetFamilyMemberAttributes(list_family_members_response.add_members(),
                              {.display_name = "Marge Simpson",
                               .email = "marge@simpsons.com",
                               .profile_url = "http://profile.url/marge",
                               .profile_image_url = "http://image.url/marge",
                               .gaia_id = "obfuscatedGaiaId1"},
                              kids_chrome_management::HEAD_OF_HOUSEHOLD);
    SetFamilyMemberAttributes(list_family_members_response.add_members(),
                              {.display_name = "Homer Simpson",
                               .email = "homer@simpsons.com",
                               .profile_url = "http://profile.url/homer",
                               .profile_image_url = "http://image.url/homer",
                               .gaia_id = "obfuscatedGaiaId2"},
                              kids_chrome_management::PARENT);

    OnGetFamilyMembersSuccess(list_family_members_response);
    for (const char* property : supervised_user::kCustodianInfoPrefs) {
      EXPECT_THAT(profile_->GetPrefs()->GetString(property), Not(IsEmpty()));
    }
  }

  {
    kids_chrome_management::ListFamilyMembersResponse
        list_family_members_response;
    OnGetFamilyMembersSuccess(list_family_members_response);
    for (const char* property : supervised_user::kCustodianInfoPrefs) {
      EXPECT_THAT(profile_->GetPrefs()->GetString(property), IsEmpty());
    }
  }
}

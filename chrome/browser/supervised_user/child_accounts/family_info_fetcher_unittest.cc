// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/family_info_fetcher.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kids_external_fetcher.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kAccountId[] = "user@gmail.com";
const char kDifferentAccountId[] = "some_other_user@gmail.com";

bool operator==(const FamilyInfoFetcher::FamilyProfile& family1,
                const FamilyInfoFetcher::FamilyProfile& family2) {
  return family1.id == family2.id &&
         family1.name == family2.name;
}

bool operator==(const FamilyInfoFetcher::FamilyMember& account1,
                const FamilyInfoFetcher::FamilyMember& account2) {
  return account1.obfuscated_gaia_id == account2.obfuscated_gaia_id &&
         account1.role == account2.role &&
         account1.display_name == account2.display_name &&
         account1.email == account2.email &&
         account1.profile_url == account2.profile_url &&
         account1.profile_image_url == account2.profile_image_url;
}

namespace {

std::string BuildGetFamilyProfileResponse(
    const FamilyInfoFetcher::FamilyProfile& family) {
  base::Value::Dict dict;
  base::Value::Dict family_dict;
  family_dict.Set("familyId", family.id);
  base::Value::Dict profile_dict;
  profile_dict.Set("name", family.name);
  family_dict.Set("profile", std::move(profile_dict));
  dict.Set("family", std::move(family_dict));
  std::string result;
  base::JSONWriter::Write(dict, &result);
  return result;
}

std::string BuildEmptyGetFamilyProfileResponse() {
  base::Value::Dict dict;
  dict.Set("family", base::Value::Dict());
  std::string result;
  base::JSONWriter::Write(dict, &result);
  return result;
}

std::string BuildGetFamilyMembersResponse(
    const std::vector<FamilyInfoFetcher::FamilyMember>& members) {
  base::Value::Dict dict;
  base::Value::List list;
  for (size_t i = 0; i < members.size(); i++) {
    const FamilyInfoFetcher::FamilyMember& member = members[i];
    base::Value::Dict member_dict;
    member_dict.Set("userId", member.obfuscated_gaia_id);
    member_dict.Set("role", FamilyInfoFetcher::RoleToString(member.role));
    if (!member.display_name.empty() ||
        !member.email.empty() ||
        !member.profile_url.empty() ||
        !member.profile_image_url.empty()) {
      base::Value::Dict profile_dict;
      if (!member.display_name.empty())
        profile_dict.Set("displayName", member.display_name);
      if (!member.email.empty())
        profile_dict.Set("email", member.email);
      if (!member.profile_url.empty())
        profile_dict.Set("profileUrl", member.profile_url);
      if (!member.profile_image_url.empty())
        profile_dict.Set("profileImageUrl", member.profile_image_url);

      member_dict.Set("profile", std::move(profile_dict));
    }
    list.Append(std::move(member_dict));
  }
  dict.Set("members", std::move(list));
  std::string result;
  base::JSONWriter::Write(dict, &result);
  return result;
}

std::string BuildEmptyGetFamilyMembersResponse() {
  base::Value::Dict dict;
  dict.Set("members", base::Value::Dict());
  std::string result;
  base::JSONWriter::Write(dict, &result);
  return result;
}

std::string BuildGarbageResponse() {
  return "garbage";
}

} // namespace

class FamilyInfoFetcherTest
    : public testing::Test,
      public FamilyInfoFetcher::Consumer {
 public:
  MOCK_METHOD1(OnGetFamilyProfileSuccess,
               void(const FamilyInfoFetcher::FamilyProfile& family));
  MOCK_METHOD1(OnGetFamilyMembersSuccess,
               void(const std::vector<FamilyInfoFetcher::FamilyMember>&
                        members));
  MOCK_METHOD1(OnFailure, void(FamilyInfoFetcher::ErrorCode error));

 private:
  void EnsureFamilyInfoFetcher() {
    DCHECK(!fetcher_);
    fetcher_ = std::make_unique<FamilyInfoFetcher>(
        this, identity_test_env_.identity_manager(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
  }

 protected:
  void StartGetFamilyProfile() {
    EnsureFamilyInfoFetcher();
    fetcher_->StartGetFamilyProfile();
  }

  void StartGetFamilyMembers() {
    EnsureFamilyInfoFetcher();
    fetcher_->StartGetFamilyMembers();
  }

  CoreAccountInfo SetPrimaryAccount() {
#if BUILDFLAG(IS_CHROMEOS)
    return identity_test_env_.SetPrimaryAccount(kAccountId,
                                                signin::ConsentLevel::kSync);
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_LINUX)
    // Android and desktop support Unicorn accounts in signed in state with sync
    // disabled. Using that setup in these tests checks that we aren't overly
    // restrictive.
    return identity_test_env_.SetPrimaryAccount(kAccountId,
                                                signin::ConsentLevel::kSignin);
#else
#error Unsupported platform.
#endif
  }

  void ClearPrimaryAccount() {
    return identity_test_env_.ClearPrimaryAccount();
  }

  void IssueRefreshToken() {
#if BUILDFLAG(IS_CHROMEOS)
    identity_test_env_.MakePrimaryAccountAvailable(kAccountId,
                                                   signin::ConsentLevel::kSync);
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_LINUX)
    // Android and desktop support Unicorn accounts in signed in state with sync
    // disabled. Using that setup in these tests checks that we aren't overly
    // restrictive.
    identity_test_env_.MakePrimaryAccountAvailable(
        kAccountId, signin::ConsentLevel::kSignin);
#else
#error Unsupported platform.
#endif
  }

  void IssueRefreshTokenForDifferentAccount() {
    identity_test_env_.MakeAccountAvailable(kDifferentAccountId);
  }

  void WaitForAccessTokenRequestAndIssueToken() {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        identity_test_env_.identity_manager()->GetPrimaryAccountId(
            signin::ConsentLevel::kSignin),
        "access_token", base::Time::Now() + base::Hours(1));
  }

  void SendResponse(int net_error,
                    int response_code,
                    const std::string& response) {
    fetcher_->OnSimpleLoaderCompleteInternal(net_error, response_code,
                                             response);
  }

  void SendValidGetFamilyProfileResponse(
      const FamilyInfoFetcher::FamilyProfile& family) {
    SendResponse(net::OK, net::HTTP_OK, BuildGetFamilyProfileResponse(family));
  }

  void SendValidGetFamilyMembersResponse(
      const std::vector<FamilyInfoFetcher::FamilyMember>& members) {
    SendResponse(net::OK, net::HTTP_OK, BuildGetFamilyMembersResponse(members));
  }

  void SendInvalidGetFamilyProfileResponse() {
    SendResponse(net::OK, net::HTTP_OK, BuildEmptyGetFamilyProfileResponse());
  }

  void SendInvalidGetFamilyMembersResponse() {
    SendResponse(net::OK, net::HTTP_OK, BuildEmptyGetFamilyMembersResponse());
  }

  void SendGarbageResponse() {
    SendResponse(net::OK, net::HTTP_OK, BuildGarbageResponse());
  }

  void SendFailedResponse() {
    SendResponse(net::ERR_ABORTED, -1, std::string());
  }

  void SendUnauthorizedResponse() {
    SendResponse(net::OK, net::HTTP_UNAUTHORIZED, std::string());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<FamilyInfoFetcher> fetcher_;
  base::HistogramTester histogram_tester_;
};

TEST_F(FamilyInfoFetcherTest, GetFamilyProfileSuccess) {
  IssueRefreshToken();

  StartGetFamilyProfile();

  WaitForAccessTokenRequestAndIssueToken();

  FamilyInfoFetcher::FamilyProfile family("test", "My Test Family");
  EXPECT_CALL(*this, OnGetFamilyProfileSuccess(family));
  SendValidGetFamilyProfileResponse(family);
}

TEST_F(FamilyInfoFetcherTest, GetFamilyMembersSuccess) {
  IssueRefreshToken();

  StartGetFamilyMembers();

  WaitForAccessTokenRequestAndIssueToken();

  std::vector<FamilyInfoFetcher::FamilyMember> members;
  members.push_back(
      FamilyInfoFetcher::FamilyMember("someObfuscatedGaiaId",
                                      FamilyInfoFetcher::HEAD_OF_HOUSEHOLD,
                                      "Homer Simpson",
                                      "homer@simpson.com",
                                      "http://profile.url/homer",
                                      "http://profile.url/homer/image"));
  members.push_back(
      FamilyInfoFetcher::FamilyMember("anotherObfuscatedGaiaId",
                                      FamilyInfoFetcher::PARENT,
                                      "Marge Simpson",
                                      std::string(),
                                      "http://profile.url/marge",
                                      std::string()));
  members.push_back(
      FamilyInfoFetcher::FamilyMember("obfuscatedGaiaId3",
                                      FamilyInfoFetcher::CHILD,
                                      "Lisa Simpson",
                                      "lisa@gmail.com",
                                      std::string(),
                                      "http://profile.url/lisa/image"));
  members.push_back(
      FamilyInfoFetcher::FamilyMember("obfuscatedGaiaId4",
                                      FamilyInfoFetcher::CHILD,
                                      "Bart Simpson",
                                      "bart@bart.bart",
                                      std::string(),
                                      std::string()));
  members.push_back(
      FamilyInfoFetcher::FamilyMember("obfuscatedGaiaId5",
                                      FamilyInfoFetcher::MEMBER,
                                      std::string(),
                                      std::string(),
                                      std::string(),
                                      std::string()));

  EXPECT_CALL(*this, OnGetFamilyMembersSuccess(members));
  SendValidGetFamilyMembersResponse(members);

  EXPECT_EQ(histogram_tester_.GetBucketCount(
                "Signin.ListFamilyMembersRequest.Status",
                KidsExternalFetcherStatus::State::NO_ERROR),
            1);
}


TEST_F(FamilyInfoFetcherTest, SuccessAfterWaitingForRefreshToken) {
  // Early set the primary account so that the fetcher is created with a proper
  // account_id. We don't use IssueRefreshToken() as it also sets a refresh
  // token for the primary account and that's something we don't want for this
  // test.
  CoreAccountInfo account_info = SetPrimaryAccount();
  StartGetFamilyProfile();

  // Since there is no refresh token yet, we should not get a request for an
  // access token at this point.
  base::MockCallback<base::OnceClosure> access_token_requested;
  EXPECT_CALL(access_token_requested, Run()).Times(0);
  identity_test_env_.SetCallbackForNextAccessTokenRequest(
      access_token_requested.Get());

  // In this case we don't directly call IssueRefreshToken() as it sets the
  // primary account. We already have a primary account set so we cannot set
  // another one.
  identity_test_env_.SetRefreshTokenForAccount(account_info.account_id);

  // Do reset the callback for access token request before using the Wait* APIs.
  identity_test_env_.SetCallbackForNextAccessTokenRequest(base::OnceClosure());
  WaitForAccessTokenRequestAndIssueToken();

  FamilyInfoFetcher::FamilyProfile family("test", "My Test Family");
  EXPECT_CALL(*this, OnGetFamilyProfileSuccess(family));
  SendValidGetFamilyProfileResponse(family);
}

TEST_F(FamilyInfoFetcherTest, NoRefreshToken) {
  // Set the primary account before creating the fetcher to allow it to properly
  // retrieve the primary account_id from IdentityManager. We don't call
  // IssueRefreshToken because we don't want it to precisely issue a refresh
  // token for the primary account, just set it.
  SetPrimaryAccount();
  StartGetFamilyProfile();

  IssueRefreshTokenForDifferentAccount();

  // Credentials for a different user should be ignored, i.e. not result in a
  // request for an access token.
  base::MockCallback<base::OnceClosure> access_token_requested;
  EXPECT_CALL(access_token_requested, Run()).Times(0);
  identity_test_env_.SetCallbackForNextAccessTokenRequest(
      access_token_requested.Get());
}

TEST_F(FamilyInfoFetcherTest, GetTokenFailureForStartGetFamilyProfile) {
  IssueRefreshToken();

  StartGetFamilyProfile();

  // On failure to get an access token we expect a token error.
  EXPECT_CALL(*this, OnFailure(FamilyInfoFetcher::ErrorCode::kTokenError));
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      identity_test_env_.identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin),
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
}

TEST_F(FamilyInfoFetcherTest, GetTokenFailureForStartGetFamilyMembers) {
  IssueRefreshToken();

  StartGetFamilyMembers();

  // On failure to get an access token we expect a token error.
  EXPECT_CALL(*this, OnFailure(FamilyInfoFetcher::ErrorCode::kTokenError));
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      identity_test_env_.identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin),
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  EXPECT_EQ(histogram_tester_.GetBucketCount(
                "Signin.ListFamilyMembersRequest.Status",
                KidsExternalFetcherStatus::State::GOOGLE_SERVICE_AUTH_ERROR),
            1);
}

TEST_F(FamilyInfoFetcherTest, InvalidFamilyProfileResponse) {
  IssueRefreshToken();

  StartGetFamilyProfile();

  WaitForAccessTokenRequestAndIssueToken();

  // Invalid response data should result in a service error.
  EXPECT_CALL(*this, OnFailure(FamilyInfoFetcher::ErrorCode::kServiceError));
  SendInvalidGetFamilyProfileResponse();
}

TEST_F(FamilyInfoFetcherTest, InvalidFamilyMembersResponse) {
  IssueRefreshToken();

  StartGetFamilyMembers();

  WaitForAccessTokenRequestAndIssueToken();

  // Invalid response data should result in a service error.
  EXPECT_CALL(*this, OnFailure(FamilyInfoFetcher::ErrorCode::kServiceError));
  SendInvalidGetFamilyMembersResponse();

  EXPECT_EQ(histogram_tester_.GetBucketCount(
                "Signin.ListFamilyMembersRequest.Status",
                KidsExternalFetcherStatus::State::DATA_ERROR),
            1);
}

TEST_F(FamilyInfoFetcherTest, GarbageFamilyMembersResponse) {
  IssueRefreshToken();

  StartGetFamilyMembers();

  WaitForAccessTokenRequestAndIssueToken();

  // Invalid response data should result in a service error.
  EXPECT_CALL(*this, OnFailure(FamilyInfoFetcher::ErrorCode::kServiceError));
  SendGarbageResponse();

  EXPECT_EQ(histogram_tester_.GetBucketCount(
                "Signin.ListFamilyMembersRequest.Status",
                KidsExternalFetcherStatus::State::INVALID_RESPONSE),
            1);
}

TEST_F(FamilyInfoFetcherTest, FailedResponse) {
  IssueRefreshToken();

  StartGetFamilyProfile();

  WaitForAccessTokenRequestAndIssueToken();

  // Failed API call should result in a network error.
  EXPECT_CALL(*this, OnFailure(FamilyInfoFetcher::ErrorCode::kNetworkError));
  SendFailedResponse();
}

TEST_F(FamilyInfoFetcherTest, UnauthorizedResponseThenSuccess) {
  IssueRefreshToken();

  StartGetFamilyProfile();

  WaitForAccessTokenRequestAndIssueToken();

  // The first fetch returns an Unauthorized response.
  // The fetcher attempts to retry by requesting a fresh token.
  SendUnauthorizedResponse();
  WaitForAccessTokenRequestAndIssueToken();

  // The above should trigger a second request with a fresh token.
  // Succeed the request and check that the client gets a success callback.
  FamilyInfoFetcher::FamilyProfile family("test", "My Test Family");
  EXPECT_CALL(*this, OnGetFamilyProfileSuccess(family));
  SendValidGetFamilyProfileResponse(family);
}

TEST_F(FamilyInfoFetcherTest, UnauthorizedResponseTwice) {
  IssueRefreshToken();

  StartGetFamilyProfile();

  WaitForAccessTokenRequestAndIssueToken();

  // The first fetch returns an Unauthorized response.
  // The fetcher attempts to retry by requesting a fresh token.
  SendUnauthorizedResponse();
  WaitForAccessTokenRequestAndIssueToken();

  // The second fetch also returns an Unauthorized response.
  // This time the fetcher gives up and passes the unsuccessful response to the
  // client.
  EXPECT_CALL(*this, OnFailure(FamilyInfoFetcher::ErrorCode::kNetworkError));
  SendUnauthorizedResponse();
}

// Disabled on ChromeOS as clearing the primary account isn't supported.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(FamilyInfoFetcherTest, PrimaryAccountClearedThenUnauthorizedResponse) {
  IssueRefreshToken();

  StartGetFamilyProfile();

  WaitForAccessTokenRequestAndIssueToken();

  // Clear the primary account (simulating signout happening during an ongoing
  // fetch).
  ClearPrimaryAccount();

  // The fetch returns an Unauthorized response.
  // Rather than triggering a fresh fetch, the client is immediately given a
  // failed return code.
  EXPECT_CALL(*this, OnFailure(FamilyInfoFetcher::ErrorCode::kTokenError));
  SendUnauthorizedResponse();
}
#endif

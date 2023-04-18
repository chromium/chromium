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
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/kids_external_fetcher.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kAccountId[] = "user@gmail.com";

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

}  // namespace

class FamilyInfoFetcherTest
    : public testing::Test,
      public FamilyInfoFetcher::Consumer {
 public:
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

  void SendValidGetFamilyMembersResponse(
      const std::vector<FamilyInfoFetcher::FamilyMember>& members) {
    SendResponse(net::OK, net::HTTP_OK, BuildGetFamilyMembersResponse(members));
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

  EXPECT_EQ(
      histogram_tester_.GetBucketCount("Signin.ListFamilyMembersRequest.Status",
                                       KidsExternalFetcherStatus::State::OK),
      1);
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

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/access_token_fetcher.h"

#include <memory>

#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "components/os_crypt/os_crypt.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr char kTokenEndpoint[] = "https://boxtokenendpoint.com/";

}  // namespace

TEST(SetGetFileSystemOAuth2Token, Box) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();

  OSCryptMocker::SetUp();

  ASSERT_TRUE(
      SetFileSystemOAuth2Tokens(prefs, "box", "testAToken", "testRToken"));

  std::string atoken;
  std::string rtoken;
  ASSERT_TRUE(GetFileSystemOAuth2Tokens(prefs, "box", &atoken, &rtoken));
  EXPECT_EQ(atoken, "testAToken");
  EXPECT_EQ(rtoken, "testRToken");

  OSCryptMocker::TearDown();
}

class AccessTokenFetcherForTest : public AccessTokenFetcher {
 public:
  using AccessTokenFetcher::AccessTokenFetcher;
  using AccessTokenFetcher::GetAccessTokenURL;
  using AccessTokenFetcher::OnGetTokenFailure;
  using AccessTokenFetcher::OnGetTokenSuccess;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessTokenFetcherForTest);
};

class AccessTokenFetcherTest : public testing::Test {
 protected:
  void SetUp() override {
    fetcher_ = std::make_unique<AccessTokenFetcherForTest>(
        url_loader_factory_.GetSafeWeakWrapper(),  // dummy; not for unit tests.
        "box", GURL(kTokenEndpoint), "refresh token",
        "",  // use existing refresh token to get access token.
        base::BindOnce(&AccessTokenFetcherTest::OnResponse,
                       factory_.GetWeakPtr()));
  }

  void OnResponse(const GoogleServiceAuthError& status,
                  const std::string& access_token,
                  const std::string& refresh_token) {
    fetch_success_ = (status.state() == GoogleServiceAuthError::State::NONE);
    access_token_fetched_ = access_token;
    refresh_token_fetched_ = refresh_token;
  }

  OAuth2AccessTokenConsumer::TokenResponse MakeTokenResponse(
      const std::string& access_token,
      const std::string& refresh_token) {
    OAuth2AccessTokenConsumer::TokenResponse::Builder builder;
    builder.WithAccessToken(access_token);
    builder.WithRefreshToken(refresh_token);
    builder.WithExpirationTime(base::Time::Now() +
                               base::TimeDelta::FromDays(1));
    builder.WithIdToken("id token");
    return builder.build();
  }

  std::unique_ptr<AccessTokenFetcherForTest> fetcher_;
  bool fetch_success_ = false;
  std::string access_token_fetched_ = "defaultAToken";
  std::string refresh_token_fetched_ = "defaultRToken";

  network::TestURLLoaderFactory url_loader_factory_;
  base::WeakPtrFactory<AccessTokenFetcherTest> factory_{this};
};

TEST_F(AccessTokenFetcherTest, URL) {
  ASSERT_EQ(fetcher_->GetAccessTokenURL(), kTokenEndpoint);
}

TEST_F(AccessTokenFetcherTest, Success) {
  auto token_response = MakeTokenResponse("goodAToken", "goodRToken");
  fetcher_->OnGetTokenSuccess(token_response);
  ASSERT_TRUE(fetch_success_);
  ASSERT_EQ(access_token_fetched_, "goodAToken");
  ASSERT_EQ(refresh_token_fetched_, "goodRToken");
}

TEST_F(AccessTokenFetcherTest, Failure) {
  auto error = GoogleServiceAuthError::FromConnectionError(1);
  fetcher_->OnGetTokenFailure(error);
  ASSERT_FALSE(fetch_success_);
  ASSERT_TRUE(access_token_fetched_.empty()) << access_token_fetched_;
  ASSERT_TRUE(refresh_token_fetched_.empty()) << refresh_token_fetched_;
}

}  // namespace enterprise_connectors

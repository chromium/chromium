// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_fetcher.h"

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kFakeToken[] = "fake-token";
constexpr char kFakeAccessToken[] = "fake-access-token";
constexpr char kFakeRefreshTokenHash[] = "fake-refresh-token-hash";
constexpr char kFakeEmail[] = "fake-email@example.com";
constexpr char kTokenInfoResponse[] =
    R"(
      { "email": "%s",
        "user_id": "1234567890",
        "token_handle": "%s"
      }
   )";
constexpr char kTokenInfoResponseMissingToken[] =
    R"(
      { "email": "%s",
        "user_id": "1234567890",
      }
   )";

std::string GetTokenInfoResponse(const std::string& email,
                                 const std::string& token) {
  return base::StringPrintf(kTokenInfoResponse, email, token);
}

std::string GetTokenInfoResponseWithMissingToken(const std::string& email) {
  return base::StringPrintf(kTokenInfoResponseMissingToken, email);
}

}  // namespace

class TokenHandleFetcherTest : public ::testing::Test {
 public:
  using TokenFetchedFuture =
      base::test::TestFuture<const AccountId&, bool, const std::string&>;

  TokenHandleFetcherTest() = default;
  ~TokenHandleFetcherTest() override = default;

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return url_loader_factory_.GetSafeWeakWrapper();
  }

  std::unique_ptr<TokenHandleFetcher> MakeTokenHandleFetcher(
      const AccountId& account_id) {
    return std::make_unique<TokenHandleFetcher>(GetSharedURLLoaderFactory(),
                                                account_id);
  }

  void AddFakeResponse(const std::string& response,
                       net::HttpStatusCode http_status) {
    url_loader_factory_.AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_info_url().spec(), response,
        http_status);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory url_loader_factory_;
};

TEST_F(TokenHandleFetcherTest, TokenHandleFetcherReturnsCorrectToken) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  std::unique_ptr<TokenHandleFetcher> token_handle_fetcher =
      MakeTokenHandleFetcher(account_id);
  AddFakeResponse(GetTokenInfoResponse(kFakeEmail, kFakeToken), net::HTTP_OK);

  TokenFetchedFuture future;
  token_handle_fetcher->Fetch(kFakeAccessToken, kFakeRefreshTokenHash,
                              future.GetCallback());

  EXPECT_EQ(future.Get<AccountId>(), account_id);
  EXPECT_TRUE(future.Get<bool>());
  EXPECT_EQ(future.Get<std::string>(), kFakeToken);
}

TEST_F(TokenHandleFetcherTest,
       TokenHandleFetcherReturnsEmptyTokenWhenFieldMissing) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  std::unique_ptr<TokenHandleFetcher> token_handle_fetcher =
      MakeTokenHandleFetcher(account_id);
  AddFakeResponse(GetTokenInfoResponseWithMissingToken(kFakeEmail),
                  net::HTTP_OK);

  TokenFetchedFuture future;
  token_handle_fetcher->Fetch(kFakeAccessToken, kFakeRefreshTokenHash,
                              future.GetCallback());

  EXPECT_EQ(future.Get<AccountId>(), account_id);
  EXPECT_FALSE(future.Get<bool>());
  EXPECT_EQ(future.Get<std::string>(), std::string());
}

TEST_F(TokenHandleFetcherTest,
       TokenHandleFetcherRecordsResponseTimeOnValidResponse) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  std::unique_ptr<TokenHandleFetcher> token_handle_fetcher =
      MakeTokenHandleFetcher(account_id);
  AddFakeResponse(GetTokenInfoResponse(kFakeEmail, kFakeToken), net::HTTP_OK);
  base::HistogramTester histogram_tester;

  TokenFetchedFuture future;
  token_handle_fetcher->Fetch(kFakeAccessToken, kFakeRefreshTokenHash,
                              future.GetCallback());

  EXPECT_TRUE(future.Wait());
  histogram_tester.ExpectTotalCount("Login.TokenObtainResponseTime", 1);
}

TEST_F(TokenHandleFetcherTest,
       TokenHandleFetcherReturnsEmptyTokenOnOAuthError) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  std::unique_ptr<TokenHandleFetcher> token_handle_fetcher =
      MakeTokenHandleFetcher(account_id);
  AddFakeResponse(std::string(), net::HTTP_UNAUTHORIZED);

  TokenFetchedFuture future;
  token_handle_fetcher->Fetch(kFakeAccessToken, kFakeRefreshTokenHash,
                              future.GetCallback());

  EXPECT_EQ(future.Get<AccountId>(), account_id);
  EXPECT_FALSE(future.Get<bool>());
  EXPECT_EQ(future.Get<std::string>(), std::string());
}

TEST_F(TokenHandleFetcherTest,
       TokenHandleFetcherReturnsEmptyTokenOnNetworkError) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  std::unique_ptr<TokenHandleFetcher> token_handle_fetcher =
      MakeTokenHandleFetcher(account_id);
  url_loader_factory_.AddResponse(
      GURL(GaiaUrls::GetInstance()->oauth2_token_info_url().spec()),
      network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus{});

  TokenFetchedFuture future;
  token_handle_fetcher->Fetch(kFakeAccessToken, kFakeRefreshTokenHash,
                              future.GetCallback());

  EXPECT_EQ(future.Get<AccountId>(), account_id);
  EXPECT_FALSE(future.Get<bool>());
  EXPECT_EQ(future.Get<std::string>(), std::string());
}

}  // namespace ash

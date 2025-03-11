// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_checker.h"

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/account_id/account_id.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_manager_test_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kValidTokenInfoResponse[] =
    R"(
      { "email": "%s",
        "user_id": "1234567890",
        "expires_in": %d
      }
   )";

std::string GetValidTokenInfoResponse(const std::string& email,
                                      int expires_in) {
  return base::StringPrintf(kValidTokenInfoResponse, email, expires_in);
}

}  // namespace

class TokenHandleCheckerTest : public ::testing::Test {
 public:
  using TokenCheckedFuture =
      base::test::TestFuture<const AccountId&,
                             const std::string&,
                             const TokenHandleChecker::Status&>;

  TokenHandleCheckerTest() = default;
  ~TokenHandleCheckerTest() override = default;

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return url_loader_factory_.GetSafeWeakWrapper();
  }

  std::unique_ptr<TokenHandleChecker> MakeTokenHandleChecker(
      const AccountId& account_id,
      const std::string& token) {
    return std::make_unique<TokenHandleChecker>(account_id, token,
                                                GetSharedURLLoaderFactory());
  }

  void AddFakeResponse(const std::string& response,
                       net::HttpStatusCode http_status) {
    url_loader_factory_.AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_info_url().spec(), response,
        http_status);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory url_loader_factory_;
};

TEST_F(TokenHandleCheckerTest, ValidTokenReturnsValidStatus) {
  const std::string token = "test-token";
  const std::string email = "test@example.com";
  AccountId account_id = AccountId::FromUserEmail(email);
  std::unique_ptr<TokenHandleChecker> token_handle_checker_ =
      MakeTokenHandleChecker(account_id, token);

  AddFakeResponse(GetValidTokenInfoResponse(email, /*expires_in=*/1000),
                  net::HTTP_OK);

  TokenCheckedFuture future;

  token_handle_checker_->StartCheck(future.GetCallback());

  EXPECT_EQ(future.Get<AccountId>(), account_id);
  EXPECT_EQ(future.Get<std::string>(), token);
  EXPECT_EQ(future.Get<TokenHandleChecker::Status>(),
            TokenHandleChecker::Status::kValid);
}

TEST_F(TokenHandleCheckerTest, NegativeExpiryTimeReturnsExpiredStatus) {
  const std::string token = "test-token";
  const std::string email = "test@example.com";
  AccountId account_id = AccountId::FromUserEmail(email);
  std::unique_ptr<TokenHandleChecker> token_handle_checker_ =
      MakeTokenHandleChecker(account_id, token);

  AddFakeResponse(GetValidTokenInfoResponse(email, /*expires_in=*/-1),
                  net::HTTP_OK);

  TokenCheckedFuture future;

  token_handle_checker_->StartCheck(future.GetCallback());

  EXPECT_EQ(future.Get<AccountId>(), account_id);
  EXPECT_EQ(future.Get<std::string>(), token);
  EXPECT_EQ(future.Get<TokenHandleChecker::Status>(),
            TokenHandleChecker::Status::kExpired);
}

TEST_F(TokenHandleCheckerTest, OAuthErrorReturnsInvalidStatus) {
  const std::string token = "test-token";
  const std::string email = "test@example.com";
  AccountId account_id = AccountId::FromUserEmail(email);
  std::unique_ptr<TokenHandleChecker> token_handle_checker_ =
      MakeTokenHandleChecker(account_id, token);

  AddFakeResponse(GetValidTokenInfoResponse(email, /*expires_in=*/1000),
                  net::HTTP_UNAUTHORIZED);

  TokenCheckedFuture future;

  token_handle_checker_->StartCheck(future.GetCallback());

  EXPECT_EQ(future.Get<AccountId>(), account_id);
  EXPECT_EQ(future.Get<std::string>(), token);
  EXPECT_EQ(future.Get<TokenHandleChecker::Status>(),
            TokenHandleChecker::Status::kInvalid);
}

TEST_F(TokenHandleCheckerTest, NetworkErrorReturnsUnknownStatus) {
  const std::string token = "test-token";
  const std::string email = "test@example.com";
  AccountId account_id = AccountId::FromUserEmail(email);
  std::unique_ptr<TokenHandleChecker> token_handle_checker_ =
      MakeTokenHandleChecker(account_id, token);

  AddFakeResponse(std::string(), net::HTTP_OK);

  TokenCheckedFuture future;

  token_handle_checker_->StartCheck(future.GetCallback());

  EXPECT_EQ(future.Get<AccountId>(), account_id);
  EXPECT_EQ(future.Get<std::string>(), token);
  EXPECT_EQ(future.Get<TokenHandleChecker::Status>(),
            TokenHandleChecker::Status::kUnknown);
}

TEST_F(TokenHandleCheckerTest, ValidTokenRecordsResponseTime) {
  const std::string token = "test-token";
  const std::string email = "test@example.com";
  AccountId account_id = AccountId::FromUserEmail(email);
  std::unique_ptr<TokenHandleChecker> token_handle_checker_ =
      MakeTokenHandleChecker(account_id, token);

  base::HistogramTester histogram_tester;

  AddFakeResponse(GetValidTokenInfoResponse(email, /*expires_in=*/1000),
                  net::HTTP_OK);

  TokenCheckedFuture future;

  token_handle_checker_->StartCheck(future.GetCallback());

  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectTotalCount(kTokenCheckResponseTime, 1);
}

TEST_F(TokenHandleCheckerTest,
       NetworkErrorWithResponseCodeRecordsResponseTime) {
  const std::string token = "test-token";
  const std::string email = "test@example.com";
  AccountId account_id = AccountId::FromUserEmail(email);
  std::unique_ptr<TokenHandleChecker> token_handle_checker_ =
      MakeTokenHandleChecker(account_id, token);

  base::HistogramTester histogram_tester;

  AddFakeResponse(std::string(), net::HTTP_OK);

  TokenCheckedFuture future;

  token_handle_checker_->StartCheck(future.GetCallback());

  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectTotalCount(kTokenCheckResponseTime, 1);
}

TEST_F(TokenHandleCheckerTest,
       NetworkErrorWithoutResponseCodeDoesNotRecordResponseTime) {
  const std::string token = "test-token";
  const std::string email = "test@example.com";
  AccountId account_id = AccountId::FromUserEmail(email);
  std::unique_ptr<TokenHandleChecker> token_handle_checker_ =
      MakeTokenHandleChecker(account_id, token);

  base::HistogramTester histogram_tester;

  url_loader_factory_.AddResponse(
      GURL(GaiaUrls::GetInstance()->oauth2_token_info_url().spec()),
      network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus{});

  TokenCheckedFuture future;

  token_handle_checker_->StartCheck(future.GetCallback());

  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectTotalCount(kTokenCheckResponseTime, 0);
}

TEST_F(TokenHandleCheckerTest, OAuthErrorRecordsResponseTime) {
  const std::string token = "test-token";
  const std::string email = "test@example.com";
  AccountId account_id = AccountId::FromUserEmail(email);
  std::unique_ptr<TokenHandleChecker> token_handle_checker_ =
      MakeTokenHandleChecker(account_id, token);

  base::HistogramTester histogram_tester;

  AddFakeResponse(GetValidTokenInfoResponse(email, /*expires_in=*/1000),
                  net::HTTP_UNAUTHORIZED);

  TokenCheckedFuture future;

  token_handle_checker_->StartCheck(future.GetCallback());

  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectTotalCount(kTokenCheckResponseTime, 1);
}

}  // namespace ash

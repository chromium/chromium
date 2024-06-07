// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/throttled_gaia_auth_fetcher.h"

#include <memory>

#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/common/renderer_configuration.mojom-shared.h"
#include "components/signin/public/base/signin_switches.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::IsTrue;
using testing::ResultOf;
using testing::StrictMock;
using UnblockRequestCallback =
    BoundSessionRequestThrottledHandler::ResumeOrCancelThrottledRequestCallback;
using UnblockAction = BoundSessionRequestThrottledHandler::UnblockAction;

namespace {

gaia::GaiaSource::Type kGaiaSourceType = gaia::GaiaSource::kChrome;
chrome::mojom::ResumeBlockedRequestsTrigger kResumeTrigger =
    chrome::mojom::ResumeBlockedRequestsTrigger::kObservedFreshCookies;

class MockGaiaAuthConsumer : public GaiaAuthConsumer {
 public:
  MOCK_METHOD(void,
              OnListAccountsSuccess,
              (const std::string& data),
              (override));
  MOCK_METHOD(void,
              OnListAccountsFailure,
              (const GoogleServiceAuthError& error),
              (override));
  MOCK_METHOD(void, OnLogOutSuccess, (), (override));
  MOCK_METHOD(void,
              OnOAuthMultiloginFinished,
              (const OAuthMultiloginResult&),
              (override));
};

class MockBoundSessionRequestThrottledHandler
    : public BoundSessionRequestThrottledHandler {
 public:
  MOCK_METHOD(void,
              HandleRequestBlockedOnCookie,
              (const GURL&, ResumeOrCancelThrottledRequestCallback),
              (override));
};

std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>
CreateBlockingParams() {
  std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr> result;
  result.push_back(chrome::mojom::BoundSessionThrottlerParams::New(
      "google.com", "/", base::Time::Now() - base::Seconds(10)));
  return result;
}

std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>
CreateNonBlockingParams() {
  std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr> result;
  result.push_back(chrome::mojom::BoundSessionThrottlerParams::New(
      "example.org", "/", base::Time::Now() - base::Seconds(10)));
  return result;
}

}  // namespace

class ThrottledGaiaAuthFetcherTest : public testing::Test {
 public:
  void CreateFetcher(
      std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr> params) {
    auto request_throttled_handler =
        std::make_unique<StrictMock<MockBoundSessionRequestThrottledHandler>>();
    mock_request_throttled_handler_ = request_throttled_handler.get();
    fetcher_ = std::make_unique<ThrottledGaiaAuthFetcher>(
        &mock_consumer_, kGaiaSourceType,
        test_url_loader_factory_.GetSafeWeakWrapper(), std::move(params),
        std::move(request_throttled_handler));
  }

  void CompleteRequest() {
    if (size_t request_count =
            test_url_loader_factory_.pending_requests()->size();
        request_count != 1) {
      ADD_FAILURE() << "Expected exactly one pending requests but found "
                    << request_count;
      return;
    }
    GURL request_url =
        test_url_loader_factory_.GetPendingRequest(0)->request.url;
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        request_url.spec(),
        /*content=*/"");
  }

  StrictMock<MockGaiaAuthConsumer>& consumer() { return mock_consumer_; }

  ThrottledGaiaAuthFetcher* fetcher() { return fetcher_.get(); }

  StrictMock<MockBoundSessionRequestThrottledHandler>*
  request_throttled_handler() {
    return mock_request_throttled_handler_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_{
      switches::kEnableBoundSessionCredentials};
  network::TestURLLoaderFactory test_url_loader_factory_;
  StrictMock<MockGaiaAuthConsumer> mock_consumer_;
  std::unique_ptr<ThrottledGaiaAuthFetcher> fetcher_;
  raw_ptr<StrictMock<MockBoundSessionRequestThrottledHandler>>
      mock_request_throttled_handler_ = nullptr;
};

TEST_F(ThrottledGaiaAuthFetcherTest, ThrottleListAccounts) {
  CreateFetcher(CreateBlockingParams());
  UnblockRequestCallback unblock_callback;
  EXPECT_CALL(*request_throttled_handler(),
              HandleRequestBlockedOnCookie(
                  ResultOf(gaia::HasGaiaSchemeHostPort, IsTrue()), _))
      .WillOnce(MoveArg<1>(&unblock_callback));

  fetcher()->StartListAccounts();
  ASSERT_TRUE(unblock_callback);
  std::move(unblock_callback).Run(UnblockAction::kResume, kResumeTrigger);
  EXPECT_CALL(consumer(), OnListAccountsSuccess(_));
  CompleteRequest();
}

TEST_F(ThrottledGaiaAuthFetcherTest, ThrottleListAccountsCancel) {
  CreateFetcher(CreateBlockingParams());
  UnblockRequestCallback unblock_callback;
  EXPECT_CALL(*request_throttled_handler(),
              HandleRequestBlockedOnCookie(
                  ResultOf(gaia::HasGaiaSchemeHostPort, IsTrue()), _))
      .WillOnce(MoveArg<1>(&unblock_callback));

  fetcher()->StartListAccounts();
  ASSERT_TRUE(unblock_callback);
  EXPECT_CALL(consumer(), OnListAccountsFailure(_));
  std::move(unblock_callback).Run(UnblockAction::kCancel, kResumeTrigger);
}

TEST_F(ThrottledGaiaAuthFetcherTest, ListAccountsNotThrottledNoBoundSessions) {
  CreateFetcher({});
  EXPECT_CALL(*request_throttled_handler(), HandleRequestBlockedOnCookie(_, _))
      .Times(0);

  fetcher()->StartListAccounts();
  EXPECT_CALL(consumer(), OnListAccountsSuccess(_));
  CompleteRequest();
}

TEST_F(ThrottledGaiaAuthFetcherTest,
       ListAccountsNotThrottledNotCoveredByBoundSession) {
  CreateFetcher(CreateNonBlockingParams());
  EXPECT_CALL(*request_throttled_handler(), HandleRequestBlockedOnCookie(_, _))
      .Times(0);

  fetcher()->StartListAccounts();
  EXPECT_CALL(consumer(), OnListAccountsSuccess(_));
  CompleteRequest();
}

TEST_F(ThrottledGaiaAuthFetcherTest, ThrottleMultilogin) {
  CreateFetcher(CreateBlockingParams());
  UnblockRequestCallback unblock_callback;
  EXPECT_CALL(*request_throttled_handler(),
              HandleRequestBlockedOnCookie(
                  ResultOf(gaia::HasGaiaSchemeHostPort, IsTrue()), _))
      .WillOnce(MoveArg<1>(&unblock_callback));

  fetcher()->StartOAuthMultilogin(
      gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER,
      {{"token1", "id1"}, {"token2", "id2"}}, "cc_result");
  ASSERT_TRUE(unblock_callback);
  std::move(unblock_callback).Run(UnblockAction::kResume, kResumeTrigger);
  EXPECT_CALL(consumer(), OnOAuthMultiloginFinished(_));
  CompleteRequest();
}

TEST_F(ThrottledGaiaAuthFetcherTest, OtherRequestNotThrottled) {
  CreateFetcher(CreateBlockingParams());
  EXPECT_CALL(*request_throttled_handler(), HandleRequestBlockedOnCookie(_, _))
      .Times(0);

  fetcher()->StartLogOut();
  EXPECT_CALL(consumer(), OnLogOutSuccess());
  CompleteRequest();
}

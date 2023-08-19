// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/win_key_network_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/fetcher/mock_win_network_fetcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/fetcher/mock_win_network_fetcher_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/fetcher/win_network_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;

namespace enterprise_connectors {

using test::MockWinNetworkFetcher;
using test::MockWinNetworkFetcherFactory;

using HttpResponseCode = KeyNetworkDelegate::HttpResponseCode;

namespace {

constexpr char kFakeBody1[] = "fake-body-1";
constexpr char kFakeBody2[] = "fake-body-2";

constexpr char kFakeDMServerUrl1[] =
    "https://example.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";
constexpr char kFakeDMServerUrl2[] =
    "https://google.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";

constexpr char kFakeDMToken1[] = "fake-browser-dm-token-1";
constexpr char kFakeDMToken2[] = "fake-browser-dm-token-2";

constexpr char kUploadTriesHistogramName[] =
    "Enterprise.DeviceTrust.RotateSigningKey.Tries";

constexpr HttpResponseCode kSuccessCode = 200;
constexpr HttpResponseCode kHardFailureCode = 400;
constexpr HttpResponseCode kTransientFailureCode = 500;
constexpr int kMaxRetryCount = 7;

}  // namespace

class WinKeyNetworkDelegateTest : public testing::Test {
 protected:
  void SetUp() override {
    auto mock_network_fetcher_factory =
        std::make_unique<MockWinNetworkFetcherFactory>();
    mock_network_fetcher_factory_ = mock_network_fetcher_factory.get();

    network_delegate_ = base::WrapUnique(
        new WinKeyNetworkDelegate(std::move(mock_network_fetcher_factory)));
  }

  // Calls the SendPublicKeyToDmServer function using the test `body`,
  // `dm_token`, and `dm_server_url`; and triggers a number of retry attempts
  // given by the param `max_retries`.
  void TestRequest(const HttpResponseCode& response_code,
                   int max_retries,
                   const std::string& test_body,
                   const std::string& dm_token,
                   const GURL& dm_server_url) {
    ::testing::InSequence sequence;

    auto mock_win_network_fetcher = std::make_unique<MockWinNetworkFetcher>();

    base::flat_map<std::string, std::string> test_headers;
    test_headers.emplace("Authorization", "GoogleDMToken token=" + dm_token);

    EXPECT_CALL(*mock_network_fetcher_factory_,
                CreateNetworkFetcher(dm_server_url, test_body, _))
        .WillOnce([&dm_server_url, &test_body, &test_headers,
                   &mock_win_network_fetcher](
                      const GURL& url, const std::string& body,
                      base::flat_map<std::string, std::string> headers) {
          EXPECT_EQ(dm_server_url, url);
          EXPECT_EQ(test_body, body);
          EXPECT_EQ(test_headers, headers);
          return std::move(mock_win_network_fetcher);
        });

    EXPECT_CALL(*mock_win_network_fetcher, Fetch(_))
        .Times(max_retries)
        .WillRepeatedly(
            [this](WinNetworkFetcher::FetchCompletedCallback callback) {
              task_environment_.FastForwardBy(
                  network_delegate_->backoff_entry_.GetTimeUntilRelease());
              std::move(callback).Run(kTransientFailureCode);
            });

    EXPECT_CALL(*mock_win_network_fetcher, Fetch(_))
        .WillOnce([response_code](
                      WinNetworkFetcher::FetchCompletedCallback callback) {
          std::move(callback).Run(response_code);
        });
    base::test::TestFuture<HttpResponseCode> future;
    network_delegate_->SendPublicKeyToDmServer(dm_server_url, dm_token,
                                               test_body, future.GetCallback());
    EXPECT_EQ(response_code, future.Get());
  }

  raw_ptr<MockWinNetworkFetcherFactory, DanglingUntriaged>
      mock_network_fetcher_factory_ = nullptr;
  std::unique_ptr<WinKeyNetworkDelegate> network_delegate_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Test the send public key request by transiently failing 3 times
// before a success. 200 error codes are treated as success.
TEST_F(WinKeyNetworkDelegateTest, SendPublicKeyRequest_Success) {
  base::HistogramTester histogram_tester;
  TestRequest(kSuccessCode, 3, kFakeBody1, kFakeDMToken1,
              GURL(kFakeDMServerUrl1));
  histogram_tester.ExpectUniqueSample(kUploadTriesHistogramName, 3, 1);
}

// Test the key upload request by transiently failing 3 times before
// a permanent failure. 400 error codes are treated as permanent
// failures.
TEST_F(WinKeyNetworkDelegateTest, SendPublicKeyRequest_PermanentFailure) {
  base::HistogramTester histogram_tester;
  TestRequest(kHardFailureCode, 3, kFakeBody1, kFakeDMToken1,
              GURL(kFakeDMServerUrl1));
  histogram_tester.ExpectUniqueSample(kUploadTriesHistogramName, 3, 1);
}

// Test the exponential backoff by transiently failing max times.
// 500 error codes are treated as transient failures.
TEST_F(WinKeyNetworkDelegateTest, SendPublicKeyRequest_TransientFailure) {
  base::HistogramTester histogram_tester;
  TestRequest(kTransientFailureCode, kMaxRetryCount, kFakeBody1, kFakeDMToken1,
              GURL(kFakeDMServerUrl1));
  histogram_tester.ExpectUniqueSample(kUploadTriesHistogramName, kMaxRetryCount,
                                      1);
}

// Tests multiple send public key requests. The mocked network fetcher
// instance is set per request.
TEST_F(WinKeyNetworkDelegateTest, SendPublicKeyRequest_MulitpleRequests) {
  base::HistogramTester histogram_tester;
  TestRequest(kSuccessCode, 1, kFakeBody1, kFakeDMToken1,
              GURL(kFakeDMServerUrl1));
  histogram_tester.ExpectUniqueSample(kUploadTriesHistogramName, 1, 1);

  TestRequest(kHardFailureCode, 1, kFakeBody2, kFakeDMToken2,
              GURL(kFakeDMServerUrl2));
  histogram_tester.ExpectUniqueSample(kUploadTriesHistogramName, 1, 2);
}

}  // namespace enterprise_connectors

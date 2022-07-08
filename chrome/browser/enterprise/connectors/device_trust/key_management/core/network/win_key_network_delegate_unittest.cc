// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/win_key_network_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/fetcher/mock_win_network_fetcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/fetcher/win_network_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;

namespace enterprise_connectors {

using test::MockWinNetworkFetcher;
using HttpResponseCode = KeyNetworkDelegate::HttpResponseCode;

namespace {

constexpr char kFakeBody[] = "fake-body";
constexpr char kFakeDMServerUrl[] =
    "https://example.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";
constexpr char kFakeDMToken[] = "fake-browser-dm-token";
constexpr char kUmaHistogramName[] =
    "Enterprise.DeviceTrust.RotateSigningKey.Tries";

constexpr HttpResponseCode kSuccessCode = 200;
constexpr HttpResponseCode kHardFailureCode = 400;
constexpr HttpResponseCode kTransientFailureCode = 500;

}  // namespace

class WinKeyNetworkDelegateTest : public testing::Test {
 protected:
  void SetNewTestFetcherInstance() {
    auto mock_win_network_fetcher = std::make_unique<MockWinNetworkFetcher>();
    mock_win_network_fetcher_ = mock_win_network_fetcher.get();
    WinNetworkFetcher::SetInstanceForTesting(
        std::move(mock_win_network_fetcher));
  }

  // Calls the SendPublicKeyToDmServer function and triggers a number
  // of retry attempts given by the param `max_retries`.
  void TestRequest(const HttpResponseCode& response_code, int max_retries) {
    SetNewTestFetcherInstance();

    ::testing::InSequence sequence;
    DCHECK(mock_win_network_fetcher_);

    EXPECT_CALL(*mock_win_network_fetcher_, Fetch(_))
        .Times(max_retries)
        .WillRepeatedly(
            [this](WinNetworkFetcher::FetchCompletedCallback callback) {
              task_environment_.FastForwardBy(
                  network_delegate_.backoff_entry_.GetTimeUntilRelease());
              std::move(callback).Run(kTransientFailureCode);
            });

    EXPECT_CALL(*mock_win_network_fetcher_, Fetch(_))
        .WillOnce([response_code](
                      WinNetworkFetcher::FetchCompletedCallback callback) {
          std::move(callback).Run(response_code);
        });
    base::test::TestFuture<HttpResponseCode> future;
    network_delegate_.SendPublicKeyToDmServer(
        GURL(kFakeDMServerUrl), kFakeDMToken, kFakeBody, future.GetCallback());
    EXPECT_EQ(response_code, future.Get());
  }

  WinKeyNetworkDelegate network_delegate_;
  MockWinNetworkFetcher* mock_win_network_fetcher_ = nullptr;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Test the send public key request by transiently failing 3 times
// before a success. 200 error codes are treated as success.
TEST_F(WinKeyNetworkDelegateTest, SendPublicKeyRequest_Success) {
  base::HistogramTester histogram_tester;
  TestRequest(kSuccessCode, 3);
  histogram_tester.ExpectUniqueSample(kUmaHistogramName, 3, 1);
}

// Test the key upload request by transiently failing 3 times before
// a permanent failure. 400 error codes are treated as permanent
// failures.
TEST_F(WinKeyNetworkDelegateTest, SendPublicKeyRequest_PermanentFailure) {
  base::HistogramTester histogram_tester;
  TestRequest(kHardFailureCode, 3);
  histogram_tester.ExpectUniqueSample(kUmaHistogramName, 3, 1);
}

// Test the exponential backoff by transiently failing max times.
// 500 error codes are treated as transient failures.
TEST_F(WinKeyNetworkDelegateTest, SendPublicKeyRequest_TransientFailure) {
  base::HistogramTester histogram_tester;
  TestRequest(kTransientFailureCode, 10);
  histogram_tester.ExpectUniqueSample(kUmaHistogramName, 10, 1);
}

// Tests multiple send public key requests. The mocked network fetcher
// instance is set per request.
TEST_F(WinKeyNetworkDelegateTest, SendPublicKeyRequest_MulitpleRequests) {
  base::HistogramTester histogram_tester;

  TestRequest(kSuccessCode, 1);
  histogram_tester.ExpectUniqueSample(kUmaHistogramName, 1, 1);

  TestRequest(kHardFailureCode, 1);
  histogram_tester.ExpectUniqueSample(kUmaHistogramName, 1, 2);
}

}  // namespace enterprise_connectors

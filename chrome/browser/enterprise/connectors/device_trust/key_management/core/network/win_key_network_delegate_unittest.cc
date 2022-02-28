// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/win_key_network_delegate.h"

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;

namespace enterprise_connectors {

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

constexpr int kSuccessCode = 200;
constexpr int kHardFailureCode = 400;
constexpr int kTransientFailureCode = 500;

}  // namespace

class WinKeyNetworkDelegateTest : public testing::Test {
 protected:
  WinKeyNetworkDelegateTest()
      : network_delegate_(mock_upload_callback_.Get(), false) {}

  // Calls the SendPublicKeyToDmServerSync function and triggers the
  // certain number of failures given by the param `num_failures`.
  int TestRequest(const int& response_code, int num_failures) {
    base::flat_map<std::string, std::string> fake_headers;
    fake_headers.emplace("Authorization",
                         "GoogleDMToken token=" + std::string(kFakeDMToken));

    EXPECT_CALL(mock_upload_callback_,
                Run(_, fake_headers, GURL(kFakeDMServerUrl), kFakeBody))
        .WillRepeatedly(
            [&response_code, &num_failures](
                base::OnceCallback<void(int)> callback,
                const base::flat_map<std::string, std::string>& headers,
                const GURL& url, const std::string& body) {
              if (num_failures > 0) {
                --num_failures;
                std::move(callback).Run(kTransientFailureCode);
              } else {
                std::move(callback).Run(response_code);
              }
            });
    return network_delegate_.SendPublicKeyToDmServerSync(
        GURL(kFakeDMServerUrl), kFakeDMToken, kFakeBody);
  }

 private:
  base::MockCallback<WinKeyNetworkDelegate::UploadKeyCallback>
      mock_upload_callback_;
  WinKeyNetworkDelegate network_delegate_;
  base::test::TaskEnvironment task_environment_;
};

// Test the send public key request by transiently failing 3 times
// before a success. 200 error codes are treated as success.
TEST_F(WinKeyNetworkDelegateTest, SendPublicKeyRequest_Success) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(kSuccessCode, TestRequest(kSuccessCode, 3));
  histogram_tester.ExpectUniqueSample(kUmaHistogramName, 3, 1);
}

// Test the key upload request by transiently failing 3 times before
// a permanent failure. 400 error codes are treated as permanent
// failures.
TEST_F(WinKeyNetworkDelegateTest, SendPublicKeyRequest_PermanentFailure) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(kHardFailureCode, TestRequest(kHardFailureCode, 3));
  histogram_tester.ExpectUniqueSample(kUmaHistogramName, 3, 1);
}

// Test the exponential backoff by transiently failing max times.
// 500 error codes are treated as transient failures.
TEST_F(WinKeyNetworkDelegateTest, SendPublicKeyRequest_TransientFailure) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(kTransientFailureCode, TestRequest(kTransientFailureCode, 0));
  histogram_tester.ExpectUniqueSample(kUmaHistogramName, 10, 1);
}

}  // namespace enterprise_connectors

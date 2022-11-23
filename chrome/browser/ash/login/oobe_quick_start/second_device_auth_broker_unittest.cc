// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/second_device_auth_broker.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_start {

using ::testing::Eq;
using ::testing::Gt;
using ::testing::IsFalse;
using ::testing::IsTrue;

namespace {

constexpr char kGetChallengeDataUrl[] =
    "https://devicesignin-pa.googleapis.com/v1/getchallengedata";
constexpr char kFakeChallengeDataResponse[] =
    "{"
    "\"challengeData\": {"
    "  \"challenge\": "
    "\"AKVcFQJJ0zreBQSrWiDJlFmeTr6K1Ik+"
    "i58k4p5A64dYYcofARHmhUNQrh0vpYZ4zbOvyBSamG/"
    "hyOxa7WdmZHLfEyobJ2FyifgY114deg==\""
    "}"
    "}";
constexpr char kInvalidBase64ChallengeDataResponse[] =
    "{"
    "\"challengeData\": {"
    "  \"challenge\": "
    "\"Not-a-base64-character()\""
    "}"
    "}";

}  // namespace

class SecondDeviceAuthBrokerTest : public ::testing::Test {
 public:
  SecondDeviceAuthBrokerTest()
      : second_device_auth_broker_(test_factory_.GetSafeWeakWrapper()) {}
  SecondDeviceAuthBrokerTest(const SecondDeviceAuthBrokerTest&) = delete;
  SecondDeviceAuthBrokerTest& operator=(const SecondDeviceAuthBrokerTest&) =
      delete;
  ~SecondDeviceAuthBrokerTest() override = default;

 protected:
  base::expected<std::string, GoogleServiceAuthError> GetChallengeBytes() {
    base::expected<std::string, GoogleServiceAuthError> response;
    base::RunLoop run_loop;
    base::OnceCallback<void(
        const base::expected<std::string, GoogleServiceAuthError>&)>
        callback = base::BindLambdaForTesting(
            [&response, &run_loop](
                const base::expected<std::string, GoogleServiceAuthError>&
                    returned_response) -> void {
              response = returned_response;
              run_loop.Quit();
            });
    second_device_auth_broker().GetChallengeBytes(std::move(callback));
    run_loop.Run();

    return response;
  }

  void AddFakeResponse(const std::string& url, const std::string& response) {
    test_factory_.AddResponse(url, response);
  }

  void SimulateAuthError(const std::string& url) {
    test_factory_.AddResponse(url, /*content=*/std::string(),
                              net::HTTP_UNAUTHORIZED);
  }

  SecondDeviceAuthBroker& second_device_auth_broker() {
    return second_device_auth_broker_;
  }

 private:
  // `task_environment_` must be the first member.
  base::test::TaskEnvironment task_environment_;

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  network::TestURLLoaderFactory test_factory_;
  SecondDeviceAuthBroker second_device_auth_broker_;
};

TEST_F(SecondDeviceAuthBrokerTest,
       GetChallengeBytesReturnsAnErrorForAuthErrors) {
  SimulateAuthError(kGetChallengeDataUrl);
  base::expected<std::string, GoogleServiceAuthError> response =
      GetChallengeBytes();
  ASSERT_THAT(response.has_value(), IsFalse());
  EXPECT_THAT(response.error().state(),
              Eq(GoogleServiceAuthError::State::SERVICE_ERROR));
}

TEST_F(SecondDeviceAuthBrokerTest,
       GetChallengeBytesReturnsAnErrorForMalformedResponse) {
  AddFakeResponse(kGetChallengeDataUrl, "");
  base::expected<std::string, GoogleServiceAuthError> response =
      GetChallengeBytes();
  ASSERT_THAT(response.has_value(), IsFalse());
  EXPECT_THAT(response.error().state(),
              Eq(GoogleServiceAuthError::State::UNEXPECTED_SERVICE_RESPONSE));

  AddFakeResponse(kGetChallengeDataUrl, "{}");
  response = GetChallengeBytes();
  ASSERT_THAT(response.has_value(), IsFalse());
  EXPECT_THAT(response.error().state(),
              Eq(GoogleServiceAuthError::State::UNEXPECTED_SERVICE_RESPONSE));

  AddFakeResponse(kGetChallengeDataUrl, "{\"challengeData\": \"\"}");
  response = GetChallengeBytes();
  ASSERT_THAT(response.has_value(), IsFalse());
  EXPECT_THAT(response.error().state(),
              Eq(GoogleServiceAuthError::State::UNEXPECTED_SERVICE_RESPONSE));
}

TEST_F(SecondDeviceAuthBrokerTest,
       GetChallengeBytesReturnsAnErrorForBase64ParsingError) {
  AddFakeResponse(kGetChallengeDataUrl, kInvalidBase64ChallengeDataResponse);
  base::expected<std::string, GoogleServiceAuthError> response =
      GetChallengeBytes();
  ASSERT_THAT(response.has_value(), IsFalse());
  EXPECT_THAT(response.error().state(),
              Eq(GoogleServiceAuthError::State::UNEXPECTED_SERVICE_RESPONSE));
}

TEST_F(SecondDeviceAuthBrokerTest, GetChallengeBytesReturnsChallengeBytes) {
  AddFakeResponse(kGetChallengeDataUrl, kFakeChallengeDataResponse);
  base::expected<std::string, GoogleServiceAuthError> response =
      GetChallengeBytes();
  ASSERT_THAT(response.has_value(), IsTrue());
  EXPECT_THAT(response->size(), Gt(0UL));
}

}  //  namespace ash::quick_start

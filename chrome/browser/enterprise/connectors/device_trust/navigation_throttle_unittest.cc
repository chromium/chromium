// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/navigation_throttle.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/device_trust_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/fake_device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/mock_device_trust_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::NavigationThrottle;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace enterprise_connectors {

namespace {

// Add a couple of seconds to the exact timeout time.
const base::TimeDelta kTimeoutTime =
    timeouts::kHandshakeTimeout + base::Seconds(2);

base::Value::List GetTrustedUrls() {
  base::Value::List trusted_urls;
  trusted_urls.Append("https://www.example.com");
  trusted_urls.Append("example2.example.com");
  return trusted_urls;
}

constexpr char kChallenge[] = R"({"challenge": "encrypted_challenge_string"})";
constexpr char kChallengeResponse[] =
    R"({"challengeResponse": "sample response"})";
constexpr char kLatencyHistogramName[] =
    "Enterprise.DeviceTrust.Attestation.ResponseLatency.%s";
constexpr char kFunnelHistogramName[] =
    "Enterprise.DeviceTrust.Attestation.Funnel";
constexpr char kHandshakeResultHistogram[] =
    "Enterprise.DeviceTrust.Handshake.Result";

scoped_refptr<net::HttpResponseHeaders> GetHeaderChallenge(
    const std::string& challenge) {
  std::string raw_response_headers =
      "HTTP/1.1 200 OK\r\n"
      "x-verified-access-challenge: " +
      challenge + "\r\n";
  return base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_response_headers));
}

}  // namespace

class DeviceTrustNavigationThrottleTest : public testing::Test {
 public:
  DeviceTrustNavigationThrottleTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kDeviceTrustConnectorEnabled);
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);

    fake_connector_ = std::make_unique<FakeDeviceTrustConnectorService>(
        profile_.GetTestingPrefService());
    fake_connector_->Initialize();

    fake_connector_->update_policy(GetTrustedUrls());

    EXPECT_CALL(mock_device_trust_service_, Watches(_))
        .WillRepeatedly(Invoke(
            [this](const GURL& url) { return fake_connector_->Watches(url); }));
    EXPECT_CALL(mock_device_trust_service_, IsEnabled())
        .WillRepeatedly(Return(true));
  }

  std::unique_ptr<DeviceTrustNavigationThrottle> CreateThrottle(
      content::NavigationHandle* navigation_handle) {
    return std::make_unique<DeviceTrustNavigationThrottle>(
        &mock_device_trust_service_, navigation_handle);
  }

  content::WebContents* web_contents() const { return web_contents_.get(); }
  content::RenderFrameHost* main_frame() const {
    return web_contents()->GetPrimaryMainFrame();
  }

  void TestReplyChallengeResponseAndResume(DeviceTrustResponse response,
                                           std::string expected_json,
                                           DTHandshakeResult expected_result) {
    content::MockNavigationHandle test_handle(GURL("https://www.example.com/"),
                                              main_frame());
    test_handle.set_response_headers(GetHeaderChallenge(kChallenge));
    auto throttle = CreateThrottle(&test_handle);
    base::RunLoop run_loop;
    throttle->set_resume_callback_for_testing(run_loop.QuitClosure());
    EXPECT_CALL(mock_device_trust_service_,
                BuildChallengeResponse(kChallenge, _))
        .WillOnce(
            [&response](
                const std::string& serialized_challenge,
                test::MockDeviceTrustService::DeviceTrustCallback callback) {
              std::move(callback).Run(response);
            });
    EXPECT_CALL(test_handle, RemoveRequestHeader("X-Device-Trust"));
    EXPECT_CALL(test_handle,
                SetRequestHeader("X-Verified-Access-Challenge-Response",
                                 expected_json));
    EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest().action());
    histogram_tester_.ExpectUniqueSample(
        kFunnelHistogramName, DTAttestationFunnelStep::kChallengeReceived, 1);
    run_loop.Run();
    histogram_tester_.ExpectTotalCount(
        base::StringPrintf(kLatencyHistogramName,
                           (response.error || response.challenge_response == "")
                               ? "Failure"
                               : "Success"),
        1);
    histogram_tester_.ExpectUniqueSample(kHandshakeResultHistogram,
                                         expected_result, 1);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  test::MockDeviceTrustService mock_device_trust_service_;
  std::unique_ptr<FakeDeviceTrustConnectorService> fake_connector_;
  base::HistogramTester histogram_tester_;
};

TEST_F(DeviceTrustNavigationThrottleTest, ExpectHeaderDeviceTrustOnRequest) {
  content::MockNavigationHandle test_handle(GURL("https://www.example.com/"),
                                            main_frame());
  EXPECT_CALL(test_handle,
              SetRequestHeader("X-Device-Trust", "VerifiedAccess"));
  auto throttle = CreateThrottle(&test_handle);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
}

TEST_F(DeviceTrustNavigationThrottleTest, NullService) {
  content::MockNavigationHandle test_handle(GURL("https://www.example.com/"),
                                            main_frame());
  EXPECT_CALL(test_handle, SetRequestHeader("X-Device-Trust", "VerifiedAccess"))
      .Times(0);
  auto throttle =
      std::make_unique<DeviceTrustNavigationThrottle>(nullptr, &test_handle);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
}

TEST_F(DeviceTrustNavigationThrottleTest, NoHeaderDeviceTrustOnRequest) {
  content::MockNavigationHandle test_handle(GURL("https://www.no-example.com/"),
                                            main_frame());
  EXPECT_CALL(test_handle, SetRequestHeader("X-Device-Trust", "VerifiedAccess"))
      .Times(0);
  auto throttle = CreateThrottle(&test_handle);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
}

TEST_F(DeviceTrustNavigationThrottleTest, InvalidURL) {
  GURL invalid_url = GURL("https://www.invalid.com/", url::Parsed(), false);
  content::MockNavigationHandle test_handle(invalid_url, main_frame());
  EXPECT_CALL(test_handle, SetRequestHeader("X-Device-Trust", "VerifiedAccess"))
      .Times(0);
  auto throttle = CreateThrottle(&test_handle);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
}

TEST_F(DeviceTrustNavigationThrottleTest, BuildChallengeResponseFromHeader) {
  content::MockNavigationHandle test_handle(GURL("https://www.example.com/"),
                                            main_frame());

  test_handle.set_response_headers(GetHeaderChallenge(kChallenge));
  auto throttle = CreateThrottle(&test_handle);

  EXPECT_CALL(test_handle, RemoveRequestHeader("X-Device-Trust"));
  EXPECT_CALL(mock_device_trust_service_,
              BuildChallengeResponse(kChallenge, _));

  EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest().action());

  base::RunLoop().RunUntilIdle();
}

TEST_F(DeviceTrustNavigationThrottleTest, TestReplyValidChallengeResponse) {
  DeviceTrustResponse test_response_valid = {kChallengeResponse, absl::nullopt,
                                             absl::nullopt};
  std::string valid_challenge_json = kChallengeResponse;
  TestReplyChallengeResponseAndResume(test_response_valid, valid_challenge_json,
                                      DTHandshakeResult::kSuccess);
  histogram_tester_.ExpectBucketCount(
      kFunnelHistogramName, DTAttestationFunnelStep::kChallengeResponseSent, 1);

  // Advance time and make sure that the timeout code doesn't get triggered.
  task_environment_.FastForwardBy(kTimeoutTime);
  histogram_tester_.ExpectTotalCount(
      base::StringPrintf(kLatencyHistogramName, "Failure"), 0);
}

TEST_F(DeviceTrustNavigationThrottleTest,
       TestReplyEmptyChallengeResponseUnknownError) {
  DeviceTrustResponse test_response_unknown = {"", absl::nullopt,
                                               absl::nullopt};
  std::string unknown_error_json = "{\"error\":\"unknown\"}";
  TestReplyChallengeResponseAndResume(test_response_unknown, unknown_error_json,
                                      DTHandshakeResult::kUnknown);
}

TEST_F(DeviceTrustNavigationThrottleTest,
       TestReplyChallengeResponseAttestationFailure) {
  DeviceTrustResponse test_response_timeout = {
      kChallengeResponse, DeviceTrustError::kTimeout,
      DTAttestationResult::kMissingSigningKey};
  std::string timeout_json =
      "{\"code\":\"missing_signing_key\",\"error\":\"timeout\"}";
  TestReplyChallengeResponseAndResume(test_response_timeout, timeout_json,
                                      DTHandshakeResult::kTimeout);
}

TEST_F(DeviceTrustNavigationThrottleTest, TestChallengeNotFromIdp) {
  content::MockNavigationHandle test_handle(GURL("https://www.example.com/"),
                                            main_frame());

  std::string raw_response_headers =
      "HTTP/1.1 200 OK\r\n non-idp-challenge: some challenge \r\n";
  test_handle.set_response_headers(
      base::MakeRefCounted<net::HttpResponseHeaders>(
          net::HttpUtil::AssembleRawHeaders(raw_response_headers)));
  auto throttle = CreateThrottle(&test_handle);

  EXPECT_CALL(test_handle, RemoveRequestHeader(_)).Times(0);
  EXPECT_CALL(mock_device_trust_service_, BuildChallengeResponse(_, _))
      .Times(0);

  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());

  base::RunLoop().RunUntilIdle();
}

TEST_F(DeviceTrustNavigationThrottleTest, TestTimeout) {
  content::MockNavigationHandle test_handle(GURL("https://www.example.com/"),
                                            main_frame());
  test_handle.set_response_headers(GetHeaderChallenge(kChallenge));
  auto throttle = CreateThrottle(&test_handle);

  base::RunLoop run_loop;
  throttle->set_resume_callback_for_testing(run_loop.QuitClosure());

  test::MockDeviceTrustService::DeviceTrustCallback captured_callback;
  EXPECT_CALL(mock_device_trust_service_, BuildChallengeResponse(kChallenge, _))
      .WillOnce(
          [&captured_callback](
              const std::string& serialized_challenge,
              test::MockDeviceTrustService::DeviceTrustCallback callback) {
            captured_callback = std::move(callback);
          });

  std::string timeout_json = "{\"error\":\"timeout\"}";
  EXPECT_CALL(test_handle, RemoveRequestHeader("X-Device-Trust"));
  EXPECT_CALL(
      test_handle,
      SetRequestHeader("X-Verified-Access-Challenge-Response", timeout_json));

  EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest().action());
  histogram_tester_.ExpectUniqueSample(
      kFunnelHistogramName, DTAttestationFunnelStep::kChallengeReceived, 1);

  task_environment_.FastForwardBy(kTimeoutTime);

  run_loop.Run();

  // Mimic as if the challenge response generation succeeded after the
  // timeout.
  ASSERT_TRUE(captured_callback);
  std::move(captured_callback)
      .Run({kChallengeResponse, absl::nullopt, absl::nullopt});

  histogram_tester_.ExpectTotalCount(
      base::StringPrintf(kLatencyHistogramName, "Failure"), 1);
  histogram_tester_.ExpectTotalCount(
      base::StringPrintf(kLatencyHistogramName, "Success"), 0);

  histogram_tester_.ExpectUniqueSample(kHandshakeResultHistogram,
                                       DTHandshakeResult::kTimeout, 1);
}

}  // namespace enterprise_connectors

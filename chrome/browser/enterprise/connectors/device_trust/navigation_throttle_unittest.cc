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
#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/device_trust_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/fake_device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/mock_device_trust_service.h"
#include "chrome/browser/enterprise/signals/user_permission_service_factory.h"
#include "chrome/browser/ui/device_signals_consent/mock_consent_requester.h"
#include "chrome/test/base/testing_profile.h"
#include "components/device_signals/core/browser/mock_user_permission_service.h"
#include "components/device_signals/core/browser/pref_names.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/device_signals/core/common/signals_features.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
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

constexpr char kTrustedUrl[] = "https://www.example.com/";
constexpr char kChallenge[] = R"({"challenge": "encrypted_challenge_string"})";
constexpr char kChallengeResponse[] =
    R"({"challengeResponse": "sample response"})";
constexpr char kLatencyHistogramName[] =
    "Enterprise.DeviceTrust.Attestation.ResponseLatency.%s";
constexpr char kFunnelHistogramName[] =
    "Enterprise.DeviceTrust.Attestation.Funnel";
constexpr char kHandshakeResultHistogram[] =
    "Enterprise.DeviceTrust.Handshake.Result";
constexpr char kPolicyLevelsHistogramName[] =
    "Enterprise.DeviceTrust.Attestation.PolicyLevel";

const std::set<DTCPolicyLevel> kUserPolicyLevel = {DTCPolicyLevel::kUser};

class MockDeviceTrustNavigationHandle : public content::MockNavigationHandle {
 public:
  MockDeviceTrustNavigationHandle(const GURL& url,
                                  content::RenderFrameHost* render_frame_host)
      : content::MockNavigationHandle(url, render_frame_host) {}
  ~MockDeviceTrustNavigationHandle() override = default;

  MockDeviceTrustNavigationHandle(const MockDeviceTrustNavigationHandle&) =
      delete;
  MockDeviceTrustNavigationHandle& operator=(
      const MockDeviceTrustNavigationHandle&) = delete;

  void set_is_main_frame(bool is_main_frame) { is_main_frame_ = is_main_frame; }

  bool IsInMainFrame() const override { return is_main_frame_; }

 private:
  bool is_main_frame_ = true;
};

base::Value::List GetTrustedUrls() {
  base::Value::List trusted_urls;
  trusted_urls.Append(kTrustedUrl);
  trusted_urls.Append("example2.example.com");
  return trusted_urls;
}

scoped_refptr<net::HttpResponseHeaders> GetHeaderChallenge(
    const std::string& challenge) {
  std::string raw_response_headers =
      "HTTP/1.1 200 OK\r\n"
      "x-verified-access-challenge: " +
      challenge + "\r\n";
  return base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_response_headers));
}

scoped_refptr<net::HttpResponseHeaders> GetRedirectedHeader() {
  std::string raw_response_headers =
      "HTTP/1.1 302\r\n"
      "content-type:text/html";
  return base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_response_headers));
}

}  // namespace

class DeviceTrustNavigationThrottleTest : public testing::Test {
 protected:
  DeviceTrustNavigationThrottleTest() {
    scoped_feature_list_.InitWithFeatures(
        {enterprise_signals::features::kDeviceSignalsConsentDialog}, {});
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    test_prefs_ = profile_.GetTestingPrefService();
    fake_connector_ =
        std::make_unique<FakeDeviceTrustConnectorService>(test_prefs_);

    ON_CALL(mock_device_trust_service_, Watches(_))
        .WillByDefault(Invoke(
            [this](const GURL& url) { return fake_connector_->Watches(url); }));
  }

  void CreateAndSetMockConsentRequester() {
    auto mock_consent_requester = std::make_unique<MockConsentRequester>();
    mock_consent_requester_ = mock_consent_requester.get();
    ConsentRequester::SetConsentRequesterForTest(
        std::move(mock_consent_requester));
  }

  void EnableDTCPolicy() {
    fake_connector_->UpdateInlinePolicy(GetTrustedUrls(),
                                        DTCPolicyLevel::kUser);
    EXPECT_CALL(mock_device_trust_service_, IsEnabled())
        .WillRepeatedly(Return(true));
  }

  void SetCanCollectSignals(bool can_collect = true) {
    EXPECT_CALL(mock_user_permission_service_, CanCollectSignals())
        .WillOnce(Return(
            can_collect ? device_signals::UserPermission::kGranted
                        : device_signals::UserPermission::kMissingConsent));
  }

  void SetShouldCollectConsent(bool should_collect = true) {
    EXPECT_CALL(mock_user_permission_service_, ShouldCollectConsent())
        .WillOnce(Return(should_collect));
  }

  void SetHasUserGesture(content::MockNavigationHandle* navigation_handle,
                         bool has_user_gesture = true) {
    EXPECT_CALL(*navigation_handle, HasUserGesture())
        .WillOnce(Return(has_user_gesture));
  }

  std::unique_ptr<DeviceTrustNavigationThrottle> CreateThrottle(
      content::NavigationHandle* navigation_handle) {
    CreateAndSetMockConsentRequester();
    auto test_throttle = std::make_unique<DeviceTrustNavigationThrottle>(
        &mock_device_trust_service_, &mock_user_permission_service_,
        navigation_handle);
    return test_throttle;
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
    SetShouldCollectConsent(/*should_collect=*/false);
    test_handle.set_response_headers(GetHeaderChallenge(kChallenge));
    auto throttle = CreateThrottle(&test_handle);
    base::RunLoop run_loop;
    throttle->set_resume_callback_for_testing(run_loop.QuitClosure());
    EXPECT_CALL(mock_device_trust_service_,
                BuildChallengeResponse(kChallenge, kUserPolicyLevel, _))
        .WillOnce(
            [&response](
                const std::string& serialized_challenge,
                std::set<DTCPolicyLevel> levels,
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
    histogram_tester_.ExpectUniqueSample(kPolicyLevelsHistogramName,
                                         DTAttestationPolicyLevel::kUser, 1);
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

  void VerifyConsentDialogFlowSuccessful(
      std::unique_ptr<DeviceTrustNavigationThrottle> throttle) {
    EXPECT_CALL(*mock_consent_requester_, RequestConsent(_))
        .WillOnce(
            [](RequestConsentCallback callback) { std::move(callback).Run(); });
    base::RunLoop run_loop;
    throttle->set_resume_callback_for_testing(run_loop.QuitClosure());

    EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest().action());

    run_loop.Run();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> test_prefs_;
  std::unique_ptr<content::WebContents> web_contents_;
  test::MockDeviceTrustService mock_device_trust_service_;
  testing::StrictMock<device_signals::MockUserPermissionService>
      mock_user_permission_service_;
  std::unique_ptr<FakeDeviceTrustConnectorService> fake_connector_;
  base::HistogramTester histogram_tester_;
  raw_ptr<MockConsentRequester, DanglingUntriaged> mock_consent_requester_;
};

TEST_F(DeviceTrustNavigationThrottleTest, ExpectHeaderDeviceTrustOnRequest) {
  EnableDTCPolicy();
  SetCanCollectSignals();
  SetShouldCollectConsent(/*should_collect=*/false);

  content::MockNavigationHandle test_handle(GURL(kTrustedUrl), main_frame());
  EXPECT_CALL(test_handle,
              SetRequestHeader("X-Device-Trust", "VerifiedAccess"));
  auto throttle = CreateThrottle(&test_handle);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
  histogram_tester_.ExpectUniqueSample(
      kFunnelHistogramName, DTAttestationFunnelStep::kAttestationFlowStarted,
      1);
}

TEST_F(DeviceTrustNavigationThrottleTest,
       ExpectHeaderDeviceTrustOnRedirectedRequest) {
  EnableDTCPolicy();
  SetCanCollectSignals();
  SetShouldCollectConsent(/*should_collect=*/false);

  content::MockNavigationHandle test_handle(GURL(kTrustedUrl), main_frame());

  // A redirected request will have non-empty response headers.
  test_handle.set_response_headers(GetRedirectedHeader());

  EXPECT_CALL(test_handle,
              SetRequestHeader("X-Device-Trust", "VerifiedAccess"));
  auto throttle = CreateThrottle(&test_handle);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
  histogram_tester_.ExpectUniqueSample(
      kFunnelHistogramName, DTAttestationFunnelStep::kAttestationFlowStarted,
      1);
}

TEST_F(DeviceTrustNavigationThrottleTest, NullDeviceTrustService) {
  EnableDTCPolicy();
  SetShouldCollectConsent(/*should_collect=*/false);

  content::MockNavigationHandle test_handle(GURL(kTrustedUrl), main_frame());
  EXPECT_CALL(test_handle, SetRequestHeader("X-Device-Trust", "VerifiedAccess"))
      .Times(0);
  auto throttle = std::make_unique<DeviceTrustNavigationThrottle>(
      nullptr, &mock_user_permission_service_, &test_handle);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
  histogram_tester_.ExpectTotalCount(kFunnelHistogramName, 0);
}

TEST_F(DeviceTrustNavigationThrottleTest, NullUserPermissionService) {
  EnableDTCPolicy();

  content::MockNavigationHandle test_handle(GURL(kTrustedUrl), main_frame());
  EXPECT_CALL(test_handle, SetRequestHeader("X-Device-Trust", "VerifiedAccess"))
      .Times(0);
  auto throttle = std::make_unique<DeviceTrustNavigationThrottle>(
      &mock_device_trust_service_, nullptr, &test_handle);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
  histogram_tester_.ExpectTotalCount(kFunnelHistogramName, 0);
}

TEST_F(DeviceTrustNavigationThrottleTest, DTCPolicyDisabled) {
  SetShouldCollectConsent(/*should_collect=*/false);

  content::MockNavigationHandle test_handle(GURL(kTrustedUrl), main_frame());
  EXPECT_CALL(test_handle, SetRequestHeader("X-Device-Trust", "VerifiedAccess"))
      .Times(0);
  auto throttle = CreateThrottle(&test_handle);

  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
  histogram_tester_.ExpectTotalCount(kFunnelHistogramName, 0);
}

TEST_F(DeviceTrustNavigationThrottleTest,
       DTCPolicyEnabled_CannotCollectSignals) {
  EnableDTCPolicy();
  SetCanCollectSignals(/*can_collect=*/false);
  SetShouldCollectConsent(/*should_collect=*/false);

  content::MockNavigationHandle test_handle(GURL(kTrustedUrl), main_frame());
  EXPECT_CALL(test_handle, SetRequestHeader("X-Device-Trust", "VerifiedAccess"))
      .Times(0);
  auto throttle = CreateThrottle(&test_handle);

  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
  histogram_tester_.ExpectTotalCount(kFunnelHistogramName, 0);
}

TEST_F(DeviceTrustNavigationThrottleTest, NoHeaderDeviceTrustOnRequest) {
  EnableDTCPolicy();
  SetCanCollectSignals();
  SetShouldCollectConsent(/*should_collect=*/false);

  content::MockNavigationHandle test_handle(GURL("https://www.no-example.com/"),
                                            main_frame());
  EXPECT_CALL(test_handle, SetRequestHeader("X-Device-Trust", "VerifiedAccess"))
      .Times(0);
  auto throttle = CreateThrottle(&test_handle);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
  histogram_tester_.ExpectTotalCount(kFunnelHistogramName, 0);
}

TEST_F(DeviceTrustNavigationThrottleTest, InvalidURL) {
  EnableDTCPolicy();
  SetShouldCollectConsent();

  GURL invalid_url = GURL("https://www.invalid.com/", url::Parsed(), false);
  content::MockNavigationHandle test_handle(invalid_url, main_frame());
  EXPECT_CALL(test_handle, SetRequestHeader("X-Device-Trust", "VerifiedAccess"))
      .Times(0);
  auto throttle = CreateThrottle(&test_handle);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
  histogram_tester_.ExpectTotalCount(kFunnelHistogramName, 0);
}

TEST_F(DeviceTrustNavigationThrottleTest, BuildChallengeResponseFromHeader) {
  EnableDTCPolicy();
  SetCanCollectSignals();
  SetShouldCollectConsent(/*should_collect=*/false);

  content::MockNavigationHandle test_handle(GURL(kTrustedUrl), main_frame());

  test_handle.set_response_headers(GetHeaderChallenge(kChallenge));
  auto throttle = CreateThrottle(&test_handle);

  EXPECT_CALL(test_handle, RemoveRequestHeader("X-Device-Trust"));
  EXPECT_CALL(mock_device_trust_service_,
              BuildChallengeResponse(kChallenge, _, _));

  EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest().action());

  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      kFunnelHistogramName, DTAttestationFunnelStep::kChallengeReceived, 1);
}

TEST_F(DeviceTrustNavigationThrottleTest, TestReplyValidChallengeResponse) {
  EnableDTCPolicy();
  SetCanCollectSignals();

  DeviceTrustResponse test_response_valid = {kChallengeResponse, std::nullopt,
                                             std::nullopt};
  std::string valid_challenge_json = kChallengeResponse;
  TestReplyChallengeResponseAndResume(test_response_valid, valid_challenge_json,
                                      DTHandshakeResult::kSuccess);

  // Advance time and make sure that the timeout code doesn't get triggered.
  task_environment_.FastForwardBy(kTimeoutTime);
  histogram_tester_.ExpectTotalCount(
      base::StringPrintf(kLatencyHistogramName, "Failure"), 0);
}

TEST_F(DeviceTrustNavigationThrottleTest,
       TestReplyEmptyChallengeResponseUnknownError) {
  EnableDTCPolicy();
  SetCanCollectSignals();

  DeviceTrustResponse test_response_unknown = {"", std::nullopt, std::nullopt};
  std::string unknown_error_json = "{\"error\":\"unknown\"}";
  TestReplyChallengeResponseAndResume(test_response_unknown, unknown_error_json,
                                      DTHandshakeResult::kUnknown);
}

TEST_F(DeviceTrustNavigationThrottleTest,
       TestReplyChallengeResponseAttestationFailure) {
  EnableDTCPolicy();
  SetCanCollectSignals();

  DeviceTrustResponse test_response_timeout = {
      kChallengeResponse, DeviceTrustError::kTimeout,
      DTAttestationResult::kMissingSigningKey};
  std::string timeout_json =
      "{\"code\":\"missing_signing_key\",\"error\":\"timeout\"}";
  TestReplyChallengeResponseAndResume(test_response_timeout, timeout_json,
                                      DTHandshakeResult::kTimeout);
}

TEST_F(DeviceTrustNavigationThrottleTest, TestChallengeNotFromIdp) {
  EnableDTCPolicy();
  SetCanCollectSignals();
  SetShouldCollectConsent(/*should_collect=*/false);

  content::MockNavigationHandle test_handle(GURL(kTrustedUrl), main_frame());

  std::string raw_response_headers =
      "HTTP/1.1 200 OK\r\n non-idp-challenge: some challenge \r\n";
  test_handle.set_response_headers(
      base::MakeRefCounted<net::HttpResponseHeaders>(
          net::HttpUtil::AssembleRawHeaders(raw_response_headers)));
  auto throttle = CreateThrottle(&test_handle);

  EXPECT_CALL(test_handle, RemoveRequestHeader(_)).Times(0);
  EXPECT_CALL(mock_device_trust_service_, BuildChallengeResponse(_, _, _))
      .Times(0);

  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());

  base::RunLoop().RunUntilIdle();
}

TEST_F(DeviceTrustNavigationThrottleTest, TestTimeout) {
  EnableDTCPolicy();
  SetCanCollectSignals();
  SetShouldCollectConsent(/*should_collect=*/false);

  content::MockNavigationHandle test_handle(GURL(kTrustedUrl), main_frame());
  test_handle.set_response_headers(GetHeaderChallenge(kChallenge));

  auto throttle = CreateThrottle(&test_handle);

  base::RunLoop run_loop;
  throttle->set_resume_callback_for_testing(run_loop.QuitClosure());

  test::MockDeviceTrustService::DeviceTrustCallback captured_callback;
  EXPECT_CALL(mock_device_trust_service_,
              BuildChallengeResponse(kChallenge, kUserPolicyLevel, _))
      .WillOnce(
          [&captured_callback](
              const std::string& serialized_challenge,
              std::set<DTCPolicyLevel> levels,
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
  histogram_tester_.ExpectUniqueSample(kPolicyLevelsHistogramName,
                                       DTAttestationPolicyLevel::kUser, 1);
  task_environment_.FastForwardBy(kTimeoutTime);

  run_loop.Run();

  // Mimic as if the challenge response generation succeeded after the
  // timeout.
  ASSERT_TRUE(captured_callback);
  std::move(captured_callback)
      .Run({kChallengeResponse, std::nullopt, std::nullopt});

  histogram_tester_.ExpectTotalCount(
      base::StringPrintf(kLatencyHistogramName, "Failure"), 1);
  histogram_tester_.ExpectTotalCount(
      base::StringPrintf(kLatencyHistogramName, "Success"), 0);

  histogram_tester_.ExpectUniqueSample(kHandshakeResultHistogram,
                                       DTHandshakeResult::kTimeout, 1);
}

TEST_F(DeviceTrustNavigationThrottleTest,
       ExpectHeaderDeviceTrustOnRequestWithConsentDialog) {
  EnableDTCPolicy();
  SetCanCollectSignals();
  SetShouldCollectConsent();

  content::MockNavigationHandle test_handle(GURL(kTrustedUrl), main_frame());
  SetHasUserGesture(&test_handle);
  EXPECT_CALL(test_handle,
              SetRequestHeader("X-Device-Trust", "VerifiedAccess"));
  VerifyConsentDialogFlowSuccessful(CreateThrottle(&test_handle));
}

TEST_F(DeviceTrustNavigationThrottleTest, BlockedByConsentDialog) {
  EnableDTCPolicy();
  SetShouldCollectConsent();

  content::MockNavigationHandle test_handle(GURL(kTrustedUrl), main_frame());
  SetHasUserGesture(&test_handle);
  EXPECT_CALL(test_handle, SetRequestHeader(_, _)).Times(0);
  EXPECT_CALL(test_handle, RemoveRequestHeader(_)).Times(0);
  auto throttle = CreateThrottle(&test_handle);
  EXPECT_CALL(*mock_consent_requester_, RequestConsent(_))
      .Times(1)
      .WillOnce(Return());
  EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest().action());

  base::RunLoop().RunUntilIdle();
}

TEST_F(DeviceTrustNavigationThrottleTest,
       NullDeviceTrustServiceWithConsentDialog) {
  EnableDTCPolicy();
  SetShouldCollectConsent();

  content::MockNavigationHandle test_handle(GURL(kTrustedUrl), main_frame());
  SetHasUserGesture(&test_handle);
  EXPECT_CALL(test_handle, SetRequestHeader("X-Device-Trust", "VerifiedAccess"))
      .Times(0);
  CreateAndSetMockConsentRequester();
  VerifyConsentDialogFlowSuccessful(
      std::make_unique<DeviceTrustNavigationThrottle>(
          nullptr, &mock_user_permission_service_, &test_handle));
}

TEST_F(DeviceTrustNavigationThrottleTest,
       NoHeaderDeviceTrustOnRequestWithConsentDialog) {
  EnableDTCPolicy();
  SetCanCollectSignals();
  SetShouldCollectConsent();

  content::MockNavigationHandle test_handle(GURL("https://www.no-example.com/"),
                                            main_frame());
  SetHasUserGesture(&test_handle);
  EXPECT_CALL(test_handle, SetRequestHeader("X-Device-Trust", "VerifiedAccess"))
      .Times(0);

  VerifyConsentDialogFlowSuccessful(CreateThrottle(&test_handle));
}

TEST_F(DeviceTrustNavigationThrottleTest, NavigationNoUserGesture) {
  EnableDTCPolicy();
  SetCanCollectSignals();
  SetShouldCollectConsent();

  content::MockNavigationHandle test_handle(GURL(kTrustedUrl), main_frame());
  EXPECT_CALL(test_handle,
              SetRequestHeader("X-Device-Trust", "VerifiedAccess"));
  SetHasUserGesture(&test_handle, /*has_user_gesture=*/false);

  auto throttle = CreateThrottle(&test_handle);
  EXPECT_CALL(*mock_consent_requester_, RequestConsent(_)).Times(0);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
}

TEST_F(DeviceTrustNavigationThrottleTest, NavigationNotInMainFrame) {
  EnableDTCPolicy();
  SetCanCollectSignals();
  SetShouldCollectConsent();

  MockDeviceTrustNavigationHandle test_handle(GURL(kTrustedUrl), main_frame());
  test_handle.set_is_main_frame(false);
  EXPECT_CALL(test_handle,
              SetRequestHeader("X-Device-Trust", "VerifiedAccess"));
  SetHasUserGesture(&test_handle);

  auto throttle = CreateThrottle(&test_handle);
  EXPECT_CALL(*mock_consent_requester_, RequestConsent(_)).Times(0);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
}

}  // namespace enterprise_connectors

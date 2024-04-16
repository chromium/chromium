// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/test/device_trust_browsertest_base.h"

#include <utility>

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/metrics_util.h"
#include "chrome/browser/enterprise/connectors/device_trust/test/test_constants.h"
#include "chrome/browser/enterprise/connectors/test/test_constants.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors::test {

namespace {

constexpr char kRedirectPath[] =
    "/enterprise/connectors/device_trust/redirect.html";
constexpr char kRedirectLocationPath[] =
    "/enterprise/connectors/device_trust/redirect-location.html";

// Headers used in the inline flow.
constexpr char kDeviceTrustHeader[] = "X-Device-Trust";
constexpr char kDeviceTrustHeaderValue[] = "VerifiedAccess";
constexpr char kVerifiedAccessChallengeHeader[] = "X-Verified-Access-Challenge";
constexpr char kVerifiedAccessResponseHeader[] =
    "X-Verified-Access-Challenge-Response";

constexpr char kFunnelHistogramName[] =
    "Enterprise.DeviceTrust.Attestation.Funnel";
constexpr char kResultHistogramName[] =
    "Enterprise.DeviceTrust.Attestation.Result";
constexpr char kLatencySuccessHistogramName[] =
    "Enterprise.DeviceTrust.Attestation.ResponseLatency.Success";
constexpr char kLatencyFailureHistogramName[] =
    "Enterprise.DeviceTrust.Attestation.ResponseLatency.Failure";
constexpr char kAttestationPolicyLevelHistogramName[] =
    "Enterprise.DeviceTrust.Attestation.PolicyLevel";

constexpr char kKeyRotationStatusHistogramFormat[] =
    "Enterprise.DeviceTrust.RotateSigningKey.%s.Status";
constexpr char kWithNonceVariant[] = "WithNonce";
constexpr char kNoNonceVariant[] = "NoNonce";

constexpr char kChallenge[] =
    "{"
    "\"challenge\": "
    "\"CkEKFkVudGVycHJpc2VLZXlDaGFsbGVuZ2USIELlPXqh8+"
    "rZJ2VIqwPXtPFrr653QdRrIzHFwqP+"
    "b3L8GJTcufirLxKAAkindNwTfwYUcbCFDjiW3kXdmDPE0wC0J6b5ZI6X6vOVcSMXTpK7nxsAGK"
    "zFV+i80LCnfwUZn7Ne1bHzloAqBdpLOu53vQ63hKRk6MRPhc9jYVDsvqXfQ7s+"
    "FUA5r3lxdoluxwAUMFqcP4VgnMvKzKTPYbnnB+xj5h5BZqjQToXJYoP4VC3/"
    "ID+YHNsCWy5o7+G5jnq0ak3zeqWfo1+lCibMPsCM+"
    "2g7nCZIwvwWlfoKwv3aKvOVMBcJxPAIxH1w+hH+"
    "NWxqRi6qgZm84q0ylm0ybs6TFjdgLvSViAIp0Z9p/An/"
    "u3W4CMboCswxIxNYRCGrIIVPElE3Yb4QS65mKrg=\""
    "}";

}  // namespace

DeviceTrustBrowserTestBase::DeviceTrustBrowserTestBase(
    std::optional<DeviceTrustConnectorState> connector_state)
    : challenge_value_(kChallenge) {
  ResetState();

  // Enabled at both levels.
  DeviceTrustConnectorState state;
  if (connector_state) {
    state = std::move(connector_state.value());
  } else {
    // Default to fully managed, affiliated and DTC enabled at both levels.
    state.affiliated = true;
    state.cloud_user_management_level.is_managed = true;
    state.cloud_user_management_level.is_inline_policy_enabled = true;
    state.cloud_machine_management_level.is_managed = true;
    state.cloud_machine_management_level.is_inline_policy_enabled = true;
  }

  device_trust_mixin_ = std::make_unique<DeviceTrustManagementMixin>(
      &mixin_host_, this, std::move(state));
}

DeviceTrustBrowserTestBase::~DeviceTrustBrowserTestBase() = default;

void DeviceTrustBrowserTestBase::SetUpInProcessBrowserTestFixture() {
  MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &DeviceTrustBrowserTestBase::HandleRequest, base::Unretained(this)));
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server_handle_ =
                  embedded_test_server()->StartAndReturnHandle());
}

void DeviceTrustBrowserTestBase::TearDownOnMainThread() {
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  MixinBasedInProcessBrowserTest::TearDownOnMainThread();
}

void DeviceTrustBrowserTestBase::SetChallengeValue(
    const std::string& new_challenge_value) {
  challenge_value_ = new_challenge_value;
}

GURL DeviceTrustBrowserTestBase::GetRedirectUrl() {
  return embedded_test_server()->GetURL(kAllowedHost, kRedirectPath);
}

GURL DeviceTrustBrowserTestBase::GetRedirectLocationUrl() {
  return embedded_test_server()->GetURL(kAllowedHost, kRedirectLocationPath);
}

GURL DeviceTrustBrowserTestBase::GetDisallowedUrl() {
  return embedded_test_server()->GetURL(kOtherHost, "/simple.html");
}

void DeviceTrustBrowserTestBase::TriggerUrlNavigation(std::optional<GURL> url) {
  GURL redirect_url = url ? url.value() : GetRedirectUrl();
  content::TestNavigationManager first_navigation(web_contents(), redirect_url);

  web_contents()->GetController().LoadURL(redirect_url, content::Referrer(),
                                          ui::PAGE_TRANSITION_TYPED,
                                          /*extra_headers=*/std::string());

  ASSERT_TRUE(first_navigation.WaitForNavigationFinished());
}

void DeviceTrustBrowserTestBase::ExpectFunnelStep(
    DTAttestationFunnelStep step) {
  histogram_tester_->ExpectBucketCount(kFunnelHistogramName, step, 1);
}

std::string DeviceTrustBrowserTestBase::GetChallengeResponseHeader() {
  // Attestation flow should be fully done.
  EXPECT_TRUE(initial_attestation_request_);
  if (!initial_attestation_request_) {
    return std::string();
  }

  // Validate that the two requests contain expected information. URLs' paths
  // have to be used for comparison due to how the HostResolver is replacing
  // domains with '127.0.0.1' in tests.
  EXPECT_EQ(initial_attestation_request_->GetURL().path(),
            GetRedirectUrl().path());
  EXPECT_EQ(
      initial_attestation_request_->headers.find(kDeviceTrustHeader)->second,
      kDeviceTrustHeaderValue);

  // Response header should always be set, even in error cases (i.e.
  // when using v1 header).
  EXPECT_TRUE(challenge_response_request_.has_value());
  if (!challenge_response_request_.has_value()) {
    return std::string();
  }

  ExpectFunnelStep(DTAttestationFunnelStep::kChallengeReceived);

  EXPECT_EQ(challenge_response_request_->GetURL().path(),
            GetRedirectLocationUrl().path());
  return challenge_response_request_->headers
      .find(kVerifiedAccessResponseHeader)
      ->second;
}

void DeviceTrustBrowserTestBase::VerifyAttestationFlowSuccessful(
    DTAttestationResult success_result,
    std::optional<DTAttestationPolicyLevel> policy_level) {
  std::string challenge_response = GetChallengeResponseHeader();
  // TODO(crbug.com/40194842): Add challenge-response validation.
  EXPECT_TRUE(!challenge_response.empty());
  ExpectFunnelStep(DTAttestationFunnelStep::kSignalsCollected);
  ExpectFunnelStep(DTAttestationFunnelStep::kChallengeResponseSent);
  histogram_tester_->ExpectUniqueSample(kResultHistogramName, success_result,
                                        1);
  histogram_tester_->ExpectTotalCount(kLatencySuccessHistogramName, 1);
  histogram_tester_->ExpectTotalCount(kLatencyFailureHistogramName, 0);

  if (policy_level) {
    histogram_tester_->ExpectUniqueSample(kAttestationPolicyLevelHistogramName,
                                          policy_level.value(), 1);
  }
}

void DeviceTrustBrowserTestBase::VerifyAttestationFlowFailure(
    const std::string& expected_challenge_response) {
  std::string challenge_response = GetChallengeResponseHeader();
  EXPECT_EQ(challenge_response, expected_challenge_response);
  histogram_tester_->ExpectBucketCount(
      kFunnelHistogramName, DTAttestationFunnelStep::kSignalsCollected, 0);
  histogram_tester_->ExpectBucketCount(
      kFunnelHistogramName, DTAttestationFunnelStep::kChallengeResponseSent, 0);
  histogram_tester_->ExpectTotalCount(kResultHistogramName, 0);
  histogram_tester_->ExpectTotalCount(kLatencySuccessHistogramName, 0);
  histogram_tester_->ExpectTotalCount(kLatencyFailureHistogramName, 1);
}

void DeviceTrustBrowserTestBase::VerifyNoInlineFlowOccurred() {
  EXPECT_FALSE(initial_attestation_request_);
  EXPECT_FALSE(challenge_response_request_);

  histogram_tester_->ExpectTotalCount(kFunnelHistogramName, 0);
  histogram_tester_->ExpectTotalCount(kResultHistogramName, 0);
  histogram_tester_->ExpectTotalCount(kLatencySuccessHistogramName, 0);
  histogram_tester_->ExpectTotalCount(kLatencyFailureHistogramName, 0);
}

void DeviceTrustBrowserTestBase::VerifyKeyRotationSuccess(bool with_nonce) {
  const char* nonce_string = with_nonce ? kWithNonceVariant : kNoNonceVariant;
  histogram_tester_->ExpectUniqueSample(
      base::StringPrintf(kKeyRotationStatusHistogramFormat, nonce_string),
      enterprise_connectors::RotationStatus::SUCCESS, 1);
}

content::WebContents* DeviceTrustBrowserTestBase::web_contents(
    Browser* active_browser) {
  if (!active_browser) {
    active_browser = browser();
  }
  return active_browser->tab_strip_model()->GetActiveWebContents();
}

std::unique_ptr<net::test_server::HttpResponse>
DeviceTrustBrowserTestBase::HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto deviceTrustHeader = request.headers.find(kDeviceTrustHeader);
  if (deviceTrustHeader != request.headers.end()) {
    // Valid request which initiates an attestation flow. Return a response
    // which fits the flow's expectations.
    initial_attestation_request_.emplace(request);

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_FOUND);
    response->AddCustomHeader("Location", GetRedirectLocationUrl().spec());
    response->AddCustomHeader(kVerifiedAccessChallengeHeader, challenge_value_);
    return response;
  }

  auto challengeResponseHeader =
      request.headers.find(kVerifiedAccessResponseHeader);
  if (challengeResponseHeader != request.headers.end()) {
    // Valid request which returns the challenge's response.
    challenge_response_request_.emplace(request);

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    return response;
  }

  return nullptr;
}

void DeviceTrustBrowserTestBase::ResetState() {
  histogram_tester_ = std::make_unique<base::HistogramTester>();
  initial_attestation_request_.reset();
  challenge_response_request_.reset();
}

}  // namespace enterprise_connectors::test

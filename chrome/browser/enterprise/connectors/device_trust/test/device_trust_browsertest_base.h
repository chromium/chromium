// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_BROWSERTEST_BASE_H_

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/test/device_trust_management_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Browser;

namespace enterprise_connectors::test {

class DeviceTrustBrowserTestBase : public MixinBasedInProcessBrowserTest {
 protected:
  explicit DeviceTrustBrowserTestBase(absl::optional<DeviceTrustConnectorState>
                                          connector_state = absl::nullopt);

  ~DeviceTrustBrowserTestBase() override;

  // MixinBasedInProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownOnMainThread() override;

  // Can be used to force a different challenge value to be passed in subsequent
  // inline flows.
  void SetChallengeValue(const std::string& new_challenge_header);

  GURL GetRedirectUrl();
  GURL GetRedirectLocationUrl();
  GURL GetDisallowedUrl();

  // Will trigger a URL navigation to `url`. This function will block until the
  // navigation is fully finished.
  // If `url` is not set, the regular `GetRedirectUrl` will be used.
  void TriggerUrlNavigation(absl::optional<GURL> url = absl::nullopt);

  // Will parse the challenge-response header out of the captured requests.
  // This function will "expect" that the inline flow was completed
  // before returning a value. Will return an empty string if the
  // header could not be found.
  std::string GetChallengeResponseHeader();

  void ExpectFunnelStep(DTAttestationFunnelStep step);

  // Verifies that an attestation flow successfully occurred, and that a
  // challenge response has been returned. Verifies that `success_result` was
  // logged properly.
  void VerifyAttestationFlowSuccessful(
      DTAttestationResult success_result = DTAttestationResult::kSuccess);

  // Verifies that an attestation flow was started, but failed early.
  // `expected_challenge_response` will be used to verify what error challenge
  // response was sent back.
  void VerifyAttestationFlowFailure(
      const std::string& expected_challenge_response);

  // Verifies that no attestation flow has occurred.
  void VerifyNoInlineFlowOccurred();

  // Can be used to reset the general state (e.g. capured requests, logged
  // histograms).
  void ResetState();

  content::WebContents* web_contents(Browser* active_browser = nullptr);

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<DeviceTrustManagementMixin> device_trust_mixin_;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  std::string challenge_value_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  absl::optional<const net::test_server::HttpRequest>
      initial_attestation_request_;
  absl::optional<const net::test_server::HttpRequest>
      challenge_response_request_;
};

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_BROWSERTEST_BASE_H_

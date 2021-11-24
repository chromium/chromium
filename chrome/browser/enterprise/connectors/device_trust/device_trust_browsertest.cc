// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/scoped_key_rotation_command_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/signals/device_info_fetcher.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/browser/enterprise_switches.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using content::NavigationHandle;
using content::TestNavigationManager;

namespace enterprise_connectors {

namespace {

constexpr char kRedirectPath[] =
    "/enterprise/connectors/device_trust/redirect.html";
constexpr char kRedirectLocationPath[] =
    "/enterprise/connectors/device_trust/redirect-location.html";
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

constexpr char kChallengeV1[] =
    "{\"challenge\": "
    "{"
    "\"data\": "
    "\"ChZFbnRlcnByaXNlS2V5Q2hhbGxlbmdlEiABAZTXEb/mB+E3Ncja9cazVIg3frBMjxpc"
    "UfyWoC+M6xjOmrvJ0y8=\","
    "\"signature\": "
    "\"cEA1rPdSEuBaM/4cWOv8R/OicR5c8IT+anVnVd7ain6ucZuyyy/8sjWYK4JpvVu2Diy6y"
    "6a77/5mis+QRNsbjVQ1QkEf7TcQOaGitt618jwQyhc54cyGhKUiuCok8Q7jc2gwrN6POKmB"
    "3Vdx+nrhmmVjzp/QAGgamPoLQmuW5XM+Cq5hSrW/U8bg12KmrZ5OHYdiZLyGGlmgE811kpxq"
    "dKQSWWB1c2xiu5ALY0q8aa8o/Hrzqko8JJbMXcefwrr9YxcEAoVH524mjtj83Pru55WfPmDL"
    "2ZgSJhErFEQDvWjyX0cDuFX8fO2i40aAwJsFoX+Z5fHbd3kanTcK+ty56w==\""
    "}"
    "}";

constexpr char kFakeCustomerId[] = "fake-customer-id";
constexpr char kFakeBrowserDMToken[] = "fake-browser-dm-token";
constexpr char kFakeEnrollmentToken[] = "fake-enrollment-token";
constexpr char kFakeBrowserClientId[] = "fake-browser-client-id";

constexpr char kAllowedHost[] = "allowed.google.com";
constexpr char kOtherHost[] = "notallowed.google.com";

// Const headers used in the handshake flow.
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

}  // namespace

class DeviceTrustBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  DeviceTrustBrowserTest() {
    browser_dm_token_storage_ =
        std::make_unique<policy::FakeBrowserDMTokenStorage>();
    browser_dm_token_storage_->SetEnrollmentToken(kFakeEnrollmentToken);
    browser_dm_token_storage_->SetClientId(kFakeBrowserClientId);
    browser_dm_token_storage_->EnableStorage(true);
    browser_dm_token_storage_->SetDMToken(kFakeBrowserDMToken);
    policy::BrowserDMTokenStorage::SetForTesting(
        browser_dm_token_storage_.get());

    scoped_feature_list_.InitWithFeatureState(kDeviceTrustConnectorEnabled,
                                              is_enabled());
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    scoped_persistence_delegate_factory_.emplace();
    scoped_rotation_command_factory_.emplace();
    enterprise_signals::DeviceInfoFetcher::SetForceStubForTesting(true);

    auto* browser_policy_manager =
        g_browser_process->browser_policy_connector()
            ->machine_level_user_cloud_policy_manager();
    auto browser_policy_data =
        std::make_unique<enterprise_management::PolicyData>();

    browser_policy_data->set_obfuscated_customer_id(kFakeCustomerId);

    browser_policy_manager->core()->store()->set_policy_data_for_testing(
        std::move(browser_policy_data));

    // Device trust only works with VerfiedAccess v2. However make sure that v1
    // headers are just treated like "untrusted" and nothing further.
    const std::string header = use_v2_header() ? kChallenge : kChallengeV1;

    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&DeviceTrustBrowserTest::HandleRequest,
                            base::Unretained(this), header));
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(test_server_handle_ =
                    embedded_test_server()->StartAndReturnHandle());
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitch(::switches::kEnableChromeBrowserCloudManagement);
  }
#endif

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void PopulatePref(bool as_empty_list = false) {
    base::Value list_value(base::Value::Type::LIST);

    if (!as_empty_list) {
      list_value.Append(kAllowedHost);
    }

    prefs()->Set(kContextAwareAccessSignalsAllowlistPref,
                 std::move(list_value));
  }

  void NavigateToUrl(const GURL& url) {
    web_contents()->GetController().LoadURL(url, content::Referrer(),
                                            ui::PAGE_TRANSITION_TYPED,
                                            /*extra_headers=*/std::string());
  }

  GURL GetRedirectUrl() {
    return embedded_test_server()->GetURL(kAllowedHost, kRedirectPath);
  }

  GURL GetRedirectLocationUrl() {
    return embedded_test_server()->GetURL(kAllowedHost, kRedirectLocationPath);
  }

  GURL GetDisallowedUrl() {
    return embedded_test_server()->GetURL(kOtherHost, "/simple.html");
  }

  void ExpectFunnelStep(DTAttestationFunnelStep step) {
    histogram_tester_.ExpectBucketCount(kFunnelHistogramName, step, 1);
  }

  // This function needs to reflect how IdP are expected to behave.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const std::string& header,
      const net::test_server::HttpRequest& request) {
    auto deviceTrustHeader = request.headers.find(kDeviceTrustHeader);
    if (deviceTrustHeader != request.headers.end()) {
      // Valid request which initiates an attestation flow. Return a response
      // which fits the flow's expectations.
      initial_attestation_request_.emplace(request);

      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", GetRedirectLocationUrl().spec());
      response->AddCustomHeader(kVerifiedAccessChallengeHeader, header);
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

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  bool is_enabled() { return std::get<0>(GetParam()); }
  bool use_v2_header() { return std::get<1>(GetParam()); }

  PrefService* prefs() { return browser()->profile()->GetPrefs(); }

  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<policy::FakeBrowserDMTokenStorage> browser_dm_token_storage_;
  absl::optional<const net::test_server::HttpRequest>
      initial_attestation_request_;
  absl::optional<const net::test_server::HttpRequest>
      challenge_response_request_;
  absl::optional<test::ScopedKeyPersistenceDelegateFactory>
      scoped_persistence_delegate_factory_;
  absl::optional<ScopedKeyRotationCommandFactory>
      scoped_rotation_command_factory_;
};

// Tests that the whole attestation flow occurs when navigating to an allowed
// domain.
IN_PROC_BROWSER_TEST_P(DeviceTrustBrowserTest, AttestationFullFlow) {
  GURL redirect_url = GetRedirectUrl();
  TestNavigationManager first_navigation(web_contents(), redirect_url);

  // Add allowed domain to Prefs and trigger a navigation to it.
  PopulatePref();
  NavigateToUrl(redirect_url);

  first_navigation.WaitForNavigationFinished();

  if (!is_enabled()) {
    // If the feature flag is disabled, the attestation flow should not have
    // been triggered (and that is the end of the test);
    EXPECT_FALSE(initial_attestation_request_);
    EXPECT_FALSE(challenge_response_request_);

    histogram_tester_.ExpectTotalCount(kFunnelHistogramName, 0);
    histogram_tester_.ExpectTotalCount(kResultHistogramName, 0);
    histogram_tester_.ExpectTotalCount(kLatencySuccessHistogramName, 0);
    histogram_tester_.ExpectTotalCount(kLatencyFailureHistogramName, 0);
    return;
  }

  // Attestation flow should be fully done.
  EXPECT_TRUE(initial_attestation_request_);

  // Validate that the two requests contain expected information. URLs' paths
  // have to be used for comparison due to how the HostResolver is replacing
  // domains with '127.0.0.1' in tests.
  EXPECT_EQ(initial_attestation_request_->GetURL().path(),
            GetRedirectUrl().path());
  EXPECT_EQ(
      initial_attestation_request_->headers.find(kDeviceTrustHeader)->second,
      kDeviceTrustHeaderValue);

  EXPECT_EQ(use_v2_header(), challenge_response_request_.has_value());

  ExpectFunnelStep(DTAttestationFunnelStep::kAttestationFlowStarted);
  ExpectFunnelStep(DTAttestationFunnelStep::kChallengeReceived);
  ExpectFunnelStep(DTAttestationFunnelStep::kSignalsCollected);

  if (use_v2_header()) {
    EXPECT_EQ(challenge_response_request_->GetURL().path(),
              GetRedirectLocationUrl().path());

    // TODO(crbug.com/1241857): Add challenge-response validation.
    const std::string& challenge_response =
        challenge_response_request_->headers
            .find(kVerifiedAccessResponseHeader)
            ->second;
    EXPECT_TRUE(!challenge_response.empty());

    ExpectFunnelStep(DTAttestationFunnelStep::kChallengeResponseSent);
    histogram_tester_.ExpectUniqueSample(kResultHistogramName,
                                         DTAttestationResult::kSuccess, 1);
    histogram_tester_.ExpectTotalCount(kLatencySuccessHistogramName, 1);
    histogram_tester_.ExpectTotalCount(kLatencyFailureHistogramName, 0);
  } else {
    histogram_tester_.ExpectBucketCount(
        kFunnelHistogramName, DTAttestationFunnelStep::kChallengeResponseSent,
        0);
    histogram_tester_.ExpectUniqueSample(
        kResultHistogramName, DTAttestationResult::kBadChallengeFormat, 1);
    histogram_tester_.ExpectTotalCount(kLatencySuccessHistogramName, 0);
    histogram_tester_.ExpectTotalCount(kLatencyFailureHistogramName, 1);
  }
}

// Tests that the attestation flow does not get triggered when navigating to a
// domain that is not part of the allow-list.
IN_PROC_BROWSER_TEST_P(DeviceTrustBrowserTest, AttestationHostNotAllowed) {
  GURL navigation_url = GetDisallowedUrl();
  TestNavigationManager navigation_manager(web_contents(), navigation_url);

  // Add allowed domain to Prefs and trigger a navigation to another domain.
  PopulatePref();
  NavigateToUrl(navigation_url);

  navigation_manager.WaitForNavigationFinished();

  // Requests with attestation flow headers should not have been recorded.
  EXPECT_FALSE(initial_attestation_request_);
  EXPECT_FALSE(challenge_response_request_);

  histogram_tester_.ExpectTotalCount(kFunnelHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kResultHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kLatencySuccessHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kLatencyFailureHistogramName, 0);
}

// Tests that the attestation flow does not get triggered when the allow-list is
// empty.
IN_PROC_BROWSER_TEST_P(DeviceTrustBrowserTest, AttestationPrefEmptyList) {
  GURL navigation_url = GetRedirectUrl();
  TestNavigationManager navigation_manager(web_contents(), navigation_url);

  // Set the allow-list Pref to an empty list and trigger a navigation.
  PopulatePref(/*as_empty_list=*/true);
  NavigateToUrl(navigation_url);

  navigation_manager.WaitForNavigationFinished();

  // Requests with attestation flow headers should not have been recorded.
  EXPECT_FALSE(initial_attestation_request_);
  EXPECT_FALSE(challenge_response_request_);

  histogram_tester_.ExpectTotalCount(kFunnelHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kResultHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kLatencySuccessHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kLatencyFailureHistogramName, 0);
}

// Tests that the attestation flow does not get triggered when the allow-list
// pref was never populate.
IN_PROC_BROWSER_TEST_P(DeviceTrustBrowserTest, AttestationPrefNotSet) {
  GURL navigation_url = GetRedirectUrl();
  TestNavigationManager navigation_manager(web_contents(), navigation_url);

  NavigateToUrl(navigation_url);

  navigation_manager.WaitForNavigationFinished();

  // Requests with attestation flow headers should not have been recorded.
  EXPECT_FALSE(initial_attestation_request_);
  EXPECT_FALSE(challenge_response_request_);

  histogram_tester_.ExpectTotalCount(kFunnelHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kResultHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kLatencySuccessHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kLatencyFailureHistogramName, 0);
}

INSTANTIATE_TEST_SUITE_P(All,
                         DeviceTrustBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace enterprise_connectors

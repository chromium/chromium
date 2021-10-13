// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/scoped_tpm_signing_key_pair.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
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

}  // namespace

class DeviceTrustBrowserTest : public InProcessBrowserTest,
                               public ::testing::WithParamInterface<bool> {
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

    scoped_tpm_signing_key_pair_.emplace();
    enterprise_signals::DeviceInfoFetcher::SetForceStubForTesting(true);

    auto* browser_policy_manager =
        g_browser_process->browser_policy_connector()
            ->machine_level_user_cloud_policy_manager();
    auto browser_policy_data =
        std::make_unique<enterprise_management::PolicyData>();

    browser_policy_data->set_obfuscated_customer_id(kFakeCustomerId);

    browser_policy_manager->core()->store()->set_policy_data_for_testing(
        std::move(browser_policy_data));

    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &DeviceTrustBrowserTest::HandleRequest, base::Unretained(this)));
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

  // This function needs to reflect how IdP are expected to behave.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto deviceTrustHeader = request.headers.find(kDeviceTrustHeader);
    if (deviceTrustHeader != request.headers.end()) {
      // Valid request which initiates an attestation flow. Return a response
      // which fits the flow's expectations.
      initial_attestation_request_.emplace(request);

      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", GetRedirectLocationUrl().spec());
      response->AddCustomHeader(kVerifiedAccessChallengeHeader, kChallenge);
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

  bool is_enabled() { return GetParam(); }

  PrefService* prefs() { return browser()->profile()->GetPrefs(); }

  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<policy::FakeBrowserDMTokenStorage> browser_dm_token_storage_;
  absl::optional<const net::test_server::HttpRequest>
      initial_attestation_request_;
  absl::optional<const net::test_server::HttpRequest>
      challenge_response_request_;
  absl::optional<test::ScopedTpmSigningKeyPair> scoped_tpm_signing_key_pair_;
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
    return;
  }

  // Attestation flow should be fully done.
  EXPECT_TRUE(initial_attestation_request_);
  EXPECT_TRUE(challenge_response_request_);

  // Validate that the two requests contain expected information. URLs' paths
  // have to be used for comparison due to how the HostResolver is replacing
  // domains with '127.0.0.1' in tests.
  EXPECT_EQ(initial_attestation_request_->GetURL().path(),
            GetRedirectUrl().path());
  EXPECT_EQ(
      initial_attestation_request_->headers.find(kDeviceTrustHeader)->second,
      kDeviceTrustHeaderValue);

  EXPECT_EQ(challenge_response_request_->GetURL().path(),
            GetRedirectLocationUrl().path());

  // TODO(crbug.com/1241857): Add challenge-response validation.
  const std::string& challenge_response =
      challenge_response_request_->headers.find(kVerifiedAccessResponseHeader)
          ->second;
  EXPECT_TRUE(!challenge_response.empty());
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
}

INSTANTIATE_TEST_SUITE_P(All, DeviceTrustBrowserTest, testing::Bool());

}  // namespace enterprise_connectors

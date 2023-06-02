// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/navigation_throttle.h"
#include "chrome/browser/enterprise/connectors/device_trust/prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/test/device_trust_management_mixin.h"
#include "chrome/browser/enterprise/connectors/device_trust/test/test_constants.h"
#include "chrome/browser/enterprise/connectors/test/test_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/device_signals/test/signals_contract.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/device_trust/test/device_trust_test_environment_win.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#endif  // #if BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/attestation/mock_tpm_challenge_key.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key.h"
#else
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/scoped_key_rotation_command_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#endif

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

#if BUILDFLAG(IS_WIN)
constexpr char kFakeNonce[] = "fake nonce";
constexpr int kSuccessCode = 200;
constexpr int kHardFailureCode = 400;
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

class DeviceTrustBrowserTestBase : public MixinBasedInProcessBrowserTest {
 protected:
  DeviceTrustBrowserTestBase() {
    ResetState();

    // Enabled at both levels.
    test::DeviceTrustConnectorState state;
    state.affiliated = true;
    state.cloud_user_management_level.is_managed = true;
    state.cloud_user_management_level.is_inline_policy_enabled = true;
    state.cloud_machine_management_level.is_managed = true;
    state.cloud_machine_management_level.is_inline_policy_enabled = true;

    device_trust_mixin_ = std::make_unique<test::DeviceTrustManagementMixin>(
        &mixin_host_, this, std::move(state));
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &DeviceTrustBrowserTestBase::HandleRequest, base::Unretained(this)));
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(test_server_handle_ =
                    embedded_test_server()->StartAndReturnHandle());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  void SetChallengeHeader(const std::string& new_challenge_header) {
    test_header_ = new_challenge_header;
  }

  void NavigateToUrl(const GURL& url) {
    web_contents()->GetController().LoadURL(url, content::Referrer(),
                                            ui::PAGE_TRANSITION_TYPED,
                                            /*extra_headers=*/std::string());
  }

  GURL GetRedirectUrl() {
    return embedded_test_server()->GetURL(test::kAllowedHost, kRedirectPath);
  }

  GURL GetRedirectLocationUrl() {
    return embedded_test_server()->GetURL(test::kAllowedHost,
                                          kRedirectLocationPath);
  }

  GURL GetDisallowedUrl() {
    return embedded_test_server()->GetURL(test::kOtherHost, "/simple.html");
  }

  void ExpectFunnelStep(DTAttestationFunnelStep step) {
    histogram_tester_->ExpectBucketCount(kFunnelHistogramName, step, 1);
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
      response->AddCustomHeader(kVerifiedAccessChallengeHeader, test_header_);
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

  content::WebContents* web_contents(Browser* active_browser = nullptr) {
    if (!active_browser)
      active_browser = browser();
    return active_browser->tab_strip_model()->GetActiveWebContents();
  }

  PrefService* GetProfilePrefs(Browser& active_browser) {
    return active_browser.profile()->GetPrefs();
  }

  std::string GetChallengeResponseHeader() {
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

    ExpectFunnelStep(DTAttestationFunnelStep::kAttestationFlowStarted);
    ExpectFunnelStep(DTAttestationFunnelStep::kChallengeReceived);

    EXPECT_EQ(challenge_response_request_->GetURL().path(),
              GetRedirectLocationUrl().path());
    return challenge_response_request_->headers
        .find(kVerifiedAccessResponseHeader)
        ->second;
  }

  void VerifyAttestationFlowSuccessful(
      DTAttestationResult success_result = DTAttestationResult::kSuccess) {
    std::string challenge_response = GetChallengeResponseHeader();
    // TODO(crbug.com/1241857): Add challenge-response validation.
    EXPECT_TRUE(!challenge_response.empty());
    ExpectFunnelStep(DTAttestationFunnelStep::kSignalsCollected);
    ExpectFunnelStep(DTAttestationFunnelStep::kChallengeResponseSent);
    histogram_tester_->ExpectUniqueSample(kResultHistogramName, success_result,
                                          1);
    histogram_tester_->ExpectTotalCount(kLatencySuccessHistogramName, 1);
    histogram_tester_->ExpectTotalCount(kLatencyFailureHistogramName, 0);
  }

  void VerifyAttestationFlowFailure() {
    std::string challenge_response = GetChallengeResponseHeader();
    static constexpr char kFailedToParseChallengeJsonResponse[] =
        "{\"error\":\"failed_to_parse_challenge\"}";
    EXPECT_EQ(challenge_response, kFailedToParseChallengeJsonResponse);
    histogram_tester_->ExpectBucketCount(
        kFunnelHistogramName, DTAttestationFunnelStep::kSignalsCollected, 0);
    histogram_tester_->ExpectBucketCount(
        kFunnelHistogramName, DTAttestationFunnelStep::kChallengeResponseSent,
        0);
    histogram_tester_->ExpectTotalCount(kResultHistogramName, 0);
    histogram_tester_->ExpectTotalCount(kLatencySuccessHistogramName, 0);
    histogram_tester_->ExpectTotalCount(kLatencyFailureHistogramName, 1);
  }

  virtual void AttestationFullFlowTest() {
    ResetState();

    GURL redirect_url = GetRedirectUrl();
    TestNavigationManager first_navigation(web_contents(), redirect_url);

    // Add allowed domain to Prefs and trigger a navigation to it.
    NavigateToUrl(redirect_url);

    ASSERT_TRUE(first_navigation.WaitForNavigationFinished());
  }

  void ResetState() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    initial_attestation_request_.reset();
    challenge_response_request_.reset();
  }

  void VerifyNoInlineFlowOccurred() {
    // If the feature flag is disabled, the attestation flow should not have
    // been triggered (and that is the end of the test);
    EXPECT_FALSE(initial_attestation_request_);
    EXPECT_FALSE(challenge_response_request_);

    histogram_tester_->ExpectTotalCount(kFunnelHistogramName, 0);
    histogram_tester_->ExpectTotalCount(kResultHistogramName, 0);
    histogram_tester_->ExpectTotalCount(kLatencySuccessHistogramName, 0);
    histogram_tester_->ExpectTotalCount(kLatencyFailureHistogramName, 0);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::string test_header_ = kChallenge;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  absl::optional<const net::test_server::HttpRequest>
      initial_attestation_request_;
  absl::optional<const net::test_server::HttpRequest>
      challenge_response_request_;
  std::unique_ptr<test::DeviceTrustManagementMixin> device_trust_mixin_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
class DeviceTrustAshBrowserTest : public DeviceTrustBrowserTestBase {
 protected:
  DeviceTrustAshBrowserTest() {
    auto mock_challenge_key =
        std::make_unique<ash::attestation::MockTpmChallengeKey>();
    mock_challenge_key->EnableFake();
    ash::attestation::TpmChallengeKeyFactory::SetForTesting(
        std::move(mock_challenge_key));
  }

  void TearDownOnMainThread() override {
    ash::attestation::TpmChallengeKeyFactory::Create();
    DeviceTrustBrowserTestBase::TearDownOnMainThread();
  }

  void ManagementAddedAfterFirstCreationTry(bool is_enabled) {
    content::MockNavigationHandle mock_nav_handle(web_contents());

    // Make the current context unmanaged.
    management_service()->SetManagementAuthoritiesForTesting(
        static_cast<int>(policy::EnterpriseManagementAuthority::NONE));

    // Try to create the device trust navigation throttle.
    EXPECT_TRUE(enterprise_connectors::DeviceTrustNavigationThrottle::
                    MaybeCreateThrottleFor(&mock_nav_handle) == nullptr);

    // Make the current context managed again.
    management_service()->SetManagementAuthoritiesForTesting(
        static_cast<int>(policy::EnterpriseManagementAuthority::CLOUD_DOMAIN));

    // Try to create the device trust navigation throttle.
    EXPECT_EQ(enterprise_connectors::DeviceTrustNavigationThrottle::
                      MaybeCreateThrottleFor(&mock_nav_handle) != nullptr,
              is_enabled);
  }

  policy::ManagementService* management_service() {
    return policy::ManagementServiceFactory::GetForProfile(
        browser()->profile());
  }
};

using DeviceTrustBrowserTest = DeviceTrustAshBrowserTest;
#else
class DeviceTrustDesktopBrowserTest : public DeviceTrustBrowserTestBase {
 protected:
  explicit DeviceTrustDesktopBrowserTest(bool create_preexisting_key = true)
      : create_preexisting_key_(create_preexisting_key) {}

  void SetUpInProcessBrowserTestFixture() override {
    DeviceTrustBrowserTestBase::SetUpInProcessBrowserTestFixture();
#if BUILDFLAG(IS_WIN)
    device_trust_test_environment_win_.emplace();
    device_trust_test_environment_win_->SetExpectedDMToken(
        test::kBrowserDmToken);
    device_trust_test_environment_win_->SetExpectedClientID(
        test::kBrowserClientId);

    if (create_preexisting_key_) {
      device_trust_test_environment_win_->SetUpExistingKey();
    }
#else  // BUILDFLAG(IS_WIN)
    scoped_persistence_delegate_factory_.emplace();
    scoped_rotation_command_factory_.emplace();
#endif
  }

  // If set to true, will fake as if a key was already persisted on the device
  // before the browser starts.
  const bool create_preexisting_key_;

#if BUILDFLAG(IS_WIN)
  absl::optional<DeviceTrustTestEnvironmentWin>
      device_trust_test_environment_win_;
#else  // BUILDFLAG(IS_WIN)
  absl::optional<test::ScopedKeyPersistenceDelegateFactory>
      scoped_persistence_delegate_factory_;
  absl::optional<ScopedKeyRotationCommandFactory>
      scoped_rotation_command_factory_;
#endif
};

using DeviceTrustBrowserTest = DeviceTrustDesktopBrowserTest;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Tests that the whole attestation flow occurs when navigating to an
// allowed domain.
IN_PROC_BROWSER_TEST_F(DeviceTrustBrowserTest, AttestationFullFlowKeyExists) {
  AttestationFullFlowTest();
  VerifyAttestationFlowSuccessful();
}

IN_PROC_BROWSER_TEST_F(DeviceTrustBrowserTest, AttestationFullFlowKeyExistsV1) {
  SetChallengeHeader(kChallengeV1);
  AttestationFullFlowTest();
  VerifyAttestationFlowFailure();
}

class DeviceTrustDisabledBrowserTest : public DeviceTrustBrowserTest {
 protected:
  DeviceTrustDisabledBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(kDeviceTrustConnectorEnabled,
                                              false);
  }
};

IN_PROC_BROWSER_TEST_F(DeviceTrustDisabledBrowserTest,
                       AttestationFullFlowKeyExists) {
  AttestationFullFlowTest();
  VerifyNoInlineFlowOccurred();
}

// Tests that the attestation flow does not get triggered when navigating to a
// domain that is not part of the allow-list.
IN_PROC_BROWSER_TEST_F(DeviceTrustBrowserTest, AttestationHostNotAllowed) {
  GURL navigation_url = GetDisallowedUrl();
  TestNavigationManager navigation_manager(web_contents(), navigation_url);

  // Add allowed domain to Prefs and trigger a navigation to another domain.
  NavigateToUrl(navigation_url);

  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  // Requests with attestation flow headers should not have been recorded.
  VerifyNoInlineFlowOccurred();
}

// Tests that the attestation flow does not get triggered when the allow-list is
// empty.
IN_PROC_BROWSER_TEST_F(DeviceTrustBrowserTest, AttestationPrefEmptyList) {
  GURL navigation_url = GetRedirectUrl();
  TestNavigationManager navigation_manager(web_contents(), navigation_url);

  // Clear the allow-list Pref and trigger a navigation.
  device_trust_mixin_->DisableAllInlinePolicies();
  NavigateToUrl(navigation_url);

  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  // Requests with attestation flow headers should not have been recorded.
  VerifyNoInlineFlowOccurred();
}

// Tests that the device trust navigation throttle does not get created for a
// navigation handle in incognito mode.
IN_PROC_BROWSER_TEST_F(DeviceTrustBrowserTest,
                       CreateNavigationThrottleIncognitoMode) {
  // Add incognito browser for the mock navigation handle.
  auto* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  content::MockNavigationHandle mock_nav_handle(
      web_contents(incognito_browser));

  // Try to create the device trust navigation throttle.
  EXPECT_FALSE(enterprise_connectors::DeviceTrustNavigationThrottle::
                   MaybeCreateThrottleFor(&mock_nav_handle));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Tests that the device trust navigation throttle does not get created when
// there is no management and later gets created when management is added to the
// same context.
IN_PROC_BROWSER_TEST_F(DeviceTrustBrowserTest,
                       ManagementAddedAfterFirstCreationTry) {
  ManagementAddedAfterFirstCreationTry(/*is_enabled=*/true);
}

IN_PROC_BROWSER_TEST_F(DeviceTrustDisabledBrowserTest,
                       ManagementAddedAfterFirstCreationTry) {
  ManagementAddedAfterFirstCreationTry(/*is_enabled=*/false);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Tests that signal values respect the expected format and is filled-out as
// expect per platform.
IN_PROC_BROWSER_TEST_F(DeviceTrustBrowserTest, SignalsContract) {
  auto* device_trust_service =
      DeviceTrustServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(device_trust_service);

  base::test::TestFuture<base::Value::Dict> future;
  device_trust_service->GetSignals(future.GetCallback());

  // This error most likely indicates that one of the signals decorators did
  // not invoke its done_closure in time.
  ASSERT_TRUE(future.Wait()) << "Timed out while collecting signals.";

  const base::Value::Dict& signals_dict = future.Get();

  const auto signals_contract_map = device_signals::test::GetSignalsContract();
  ASSERT_FALSE(signals_contract_map.empty());
  for (const auto& signals_contract_entry : signals_contract_map) {
    // First is the signal name.
    // Second is the contract evaluation predicate.
    EXPECT_TRUE(signals_contract_entry.second.Run(signals_dict))
        << "Signals contract validation failed for: "
        << signals_contract_entry.first;
  }
}

#if BUILDFLAG(IS_WIN)

using KeyRotationResult = DeviceTrustKeyManager::KeyRotationResult;

// To test "create key" flows, there should be no pre-existing persisted key.
class DeviceTrustCreateKeyBrowserTest : public DeviceTrustDesktopBrowserTest {
 protected:
  DeviceTrustCreateKeyBrowserTest()
      : DeviceTrustDesktopBrowserTest(/*create_preexisting_key=*/false) {}
};

// Windows DT test environment mocks the registry and DT key does not exist by
// default, in this test case a key will be created by DeviceTrustKeyManager.
IN_PROC_BROWSER_TEST_F(DeviceTrustCreateKeyBrowserTest,
                       AttestationFullFlowKeyCreation) {
  AttestationFullFlowTest();
  VerifyAttestationFlowSuccessful();
}

IN_PROC_BROWSER_TEST_F(DeviceTrustCreateKeyBrowserTest,
                       AttestationFullFlowKeyCreationV1) {
  SetChallengeHeader(kChallengeV1);
  AttestationFullFlowTest();
  VerifyAttestationFlowFailure();
}

// To test "create key" flows where the initial upload fails, the response code
// needs to be mocked before the browser starts.
class DeviceTrustCreateKeyUploadFailedBrowserTest
    : public DeviceTrustCreateKeyBrowserTest {
 protected:
  DeviceTrustCreateKeyUploadFailedBrowserTest()
      : DeviceTrustCreateKeyBrowserTest() {}

  void SetUpOnMainThread() override {
    DeviceTrustCreateKeyBrowserTest::SetUpOnMainThread();

    // First attestation flow attempt fails when a DT attestation key does not
    // exist, and KeyRotationCommand fails to upload the newly created key.
    device_trust_test_environment_win_->SetUploadResult(kHardFailureCode);
  }
};

IN_PROC_BROWSER_TEST_F(DeviceTrustCreateKeyUploadFailedBrowserTest,
                       AttestationFullFlowSucceedOnThirdAttempt) {
  AttestationFullFlowTest();
  VerifyAttestationFlowSuccessful(DTAttestationResult::kSuccessNoSignature);
  // DT attestation key should not be created if attestation fails.
  ASSERT_FALSE(device_trust_test_environment_win_->KeyExists());

  // Second attestation flow attempt fails when key upload fails again, this is
  // for testing that consecutive failures does not break anything
  AttestationFullFlowTest();
  VerifyAttestationFlowSuccessful(DTAttestationResult::kSuccessNoSignature);
  ASSERT_FALSE(device_trust_test_environment_win_->KeyExists());

  // Third attestation flow attempt succeeds after two failed attempts, this is
  // for testing that previous failed attempts does not affect new attempts from
  // succeeding AND that metrics is working at the same time.
  device_trust_test_environment_win_->SetUploadResult(kSuccessCode);
  AttestationFullFlowTest();
  VerifyAttestationFlowSuccessful();
  ASSERT_TRUE(device_trust_test_environment_win_->KeyExists());
}

IN_PROC_BROWSER_TEST_F(DeviceTrustDesktopBrowserTest,
                       RemoteCommandKeyRotationSuccess) {
  // Make sure the key is present and store its current value.
  std::vector<uint8_t> current_key_pair =
      device_trust_test_environment_win_->GetWrappedKey();
  ASSERT_FALSE(current_key_pair.empty());

  auto* key_manager = g_browser_process->browser_policy_connector()
                          ->chrome_browser_cloud_management_controller()
                          ->GetDeviceTrustKeyManager();

  base::test::TestFuture<KeyRotationResult> future_result;
  key_manager->RotateKey(kFakeNonce, future_result.GetCallback());
  ASSERT_EQ(future_result.Get(), KeyRotationResult::SUCCESS);

  // Check that key still exists & is replaced with new value.
  ASSERT_TRUE(device_trust_test_environment_win_->KeyExists());
  EXPECT_NE(device_trust_test_environment_win_->GetWrappedKey(),
            current_key_pair);
}

IN_PROC_BROWSER_TEST_F(DeviceTrustDesktopBrowserTest,
                       RemoteCommandKeyRotationFailure) {
  // Make sure key presents and stores its current value.
  std::vector<uint8_t> current_key_pair =
      device_trust_test_environment_win_->GetWrappedKey();
  ASSERT_FALSE(current_key_pair.empty());

  // Force key upload to fail, in turn failing the key rotation
  device_trust_test_environment_win_->SetUploadResult(kHardFailureCode);

  auto* key_manager = g_browser_process->browser_policy_connector()
                          ->chrome_browser_cloud_management_controller()
                          ->GetDeviceTrustKeyManager();

  base::test::TestFuture<KeyRotationResult> future_result;
  key_manager->RotateKey(kFakeNonce, future_result.GetCallback());
  ASSERT_EQ(future_result.Get(), KeyRotationResult::FAILURE);

  // Check that key still exists & has the same value since rotation failed.
  ASSERT_TRUE(device_trust_test_environment_win_->KeyExists());
  EXPECT_EQ(device_trust_test_environment_win_->GetWrappedKey(),
            current_key_pair);
}

class DeviceTrustDisabledCreateKeyBrowserTest
    : public DeviceTrustCreateKeyBrowserTest {
 protected:
  DeviceTrustDisabledCreateKeyBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(kDeviceTrustConnectorEnabled,
                                              false);
  }
};
IN_PROC_BROWSER_TEST_F(DeviceTrustDisabledCreateKeyBrowserTest,
                       AttestationFullFlowKeyCreation) {
  AttestationFullFlowTest();
  VerifyNoInlineFlowOccurred();
  ASSERT_FALSE(device_trust_test_environment_win_->KeyExists());
}

#endif

}  // namespace enterprise_connectors

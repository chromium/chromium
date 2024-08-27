// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/ssl_error_handler.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/test/browser_test.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/dns/mock_host_resolver.h"
#include "net/net_buildflags.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/ssl_config.mojom.h"
#include "third_party/blink/public/common/features.h"

namespace AuthState = ssl_test_util::AuthState;

namespace {

// The test EV policy OID used for generated certs.
static const char kOCSPTestCertPolicy[] = "1.3.6.1.4.1.11129.2.4.1";

}  // namespace

class OCSPBrowserTest : public InProcessBrowserTest,
                        public network::mojom::SSLConfigClient {
 public:
  OCSPBrowserTest() = default;

  void SetUp() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        false);

    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    InProcessBrowserTest::SetUp();
    SSLErrorHandler::ResetConfigForTesting();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();

    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        std::nullopt);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    network::mojom::NetworkContextParamsPtr context_params =
        g_browser_process->system_network_context_manager()
            ->CreateDefaultNetworkContextParams();
    last_ssl_config_ = *context_params->initial_ssl_config;
    receiver_.Bind(std::move(context_params->ssl_config_client_receiver));
  }

  // Sets the policy identified by |policy_name| to be true, ensuring
  // that the corresponding boolean pref |pref_name| is updated to match.
  void EnablePolicy(PrefService* pref_service,
                    const char* policy_name,
                    const char* pref_name) {
    policy::PolicyMap policy_map;
    policy_map.Set(policy_name, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                   base::Value(true), nullptr);

    EXPECT_NO_FATAL_FAILURE(UpdateChromePolicy(policy_map));

    EXPECT_TRUE(pref_service->GetBoolean(pref_name));
    EXPECT_TRUE(pref_service->IsManagedPreference(pref_name));

    // Wait for the updated SSL configuration to be sent to the network service,
    // to avoid a race.
    g_browser_process->system_network_context_manager()
        ->FlushSSLConfigManagerForTesting();
  }

  void EnableRevocationChecking() {
    // OCSP checking is disabled by default.
    EXPECT_FALSE(last_ssl_config_.rev_checking_enabled);
    EXPECT_FALSE(g_browser_process->system_network_context_manager()
                     ->CreateDefaultNetworkContextParams()
                     ->initial_ssl_config->rev_checking_enabled);

    // Enable, and make sure the default network context params reflect the
    // change.
    base::RunLoop run_loop;
    set_ssl_config_updated_callback(run_loop.QuitClosure());
    ASSERT_NO_FATAL_FAILURE(
        EnablePolicy(g_browser_process->local_state(),
                     policy::key::kEnableOnlineRevocationChecks,
                     prefs::kCertRevocationCheckingEnabled));
    run_loop.Run();
    EXPECT_TRUE(last_ssl_config_.rev_checking_enabled);
    EXPECT_TRUE(g_browser_process->system_network_context_manager()
                    ->CreateDefaultNetworkContextParams()
                    ->initial_ssl_config->rev_checking_enabled);
  }

  void DoConnection(
      std::string_view hostname,
      const net::EmbeddedTestServer::ServerCertificateConfig& config) {
    net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);

    server.SetSSLConfig(config);
    server.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(server.Start());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), server.GetURL(hostname, "/ssl/google.html")));
  }

  void DoConnection(
      const net::EmbeddedTestServer::ServerCertificateConfig& config) {
    DoConnection("127.0.0.1", config);
  }

  net::CertStatus GetCurrentCertStatus() {
    content::NavigationEntry* entry =
        chrome_test_utils::GetActiveWebContents(this)
            ->GetController()
            .GetVisibleEntry();
    return entry->GetSSL().cert_status;
  }

  void set_ssl_config_updated_callback(
      const base::RepeatingClosure& ssl_config_updated_callback) {
    ssl_config_updated_callback_ = std::move(ssl_config_updated_callback);
  }

  // network::mojom::SSLConfigClient implementation.
  void OnSSLConfigUpdated(network::mojom::SSLConfigPtr ssl_config) override {
    last_ssl_config_ = *ssl_config;
    if (ssl_config_updated_callback_)
      ssl_config_updated_callback_.Run();
  }

  const network::mojom::SSLConfig& last_ssl_config() const {
    return last_ssl_config_;
  }

 private:
  void UpdateChromePolicy(const policy::PolicyMap& policies) {
    policy_provider_.UpdateChromePolicy(policies);
    ASSERT_TRUE(base::CurrentThread::Get());

    base::RunLoop().RunUntilIdle();

    content::FlushNetworkServiceInstanceForTesting();
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;

  base::RepeatingClosure ssl_config_updated_callback_;
  network::mojom::SSLConfig last_ssl_config_;
  mojo::Receiver<network::mojom::SSLConfigClient> receiver_{this};
};

// Visits a page with revocation checking set to the default value (disabled)
// and a revoked OCSP response.
IN_PROC_BROWSER_TEST_F(OCSPBrowserTest, TestHTTPSOCSPRevokedButNotChecked) {
  // OCSP checking is disabled by default.
  EXPECT_FALSE(last_ssl_config().rev_checking_enabled);
  EXPECT_FALSE(g_browser_process->system_network_context_manager()
                   ->CreateDefaultNetworkContextParams()
                   ->initial_ssl_config->rev_checking_enabled);

  net::EmbeddedTestServer::ServerCertificateConfig revoked_cert_config;
  revoked_cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});
  DoConnection(revoked_cert_config);

  ssl_test_util::CheckAuthenticatedState(
      chrome_test_utils::GetActiveWebContents(this), AuthState::NONE);

  net::CertStatus cert_status = GetCurrentCertStatus();
  EXPECT_FALSE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

// Visits a page with revocation checking enabled and a valid OCSP response.
IN_PROC_BROWSER_TEST_F(OCSPBrowserTest, TestHTTPSOCSPOk) {
  EnableRevocationChecking();

  net::EmbeddedTestServer::ServerCertificateConfig ok_cert_config;
  ok_cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  DoConnection(ok_cert_config);

  ssl_test_util::CheckAuthenticatedState(
      chrome_test_utils::GetActiveWebContents(this), AuthState::NONE);

  net::CertStatus cert_status = GetCurrentCertStatus();
  EXPECT_TRUE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

// Visits a page with revocation checking enabled and a revoked OCSP response.
IN_PROC_BROWSER_TEST_F(OCSPBrowserTest, TestHTTPSOCSPRevoked) {
  EnableRevocationChecking();

  net::EmbeddedTestServer::ServerCertificateConfig revoked_cert_config;
  revoked_cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  DoConnection(revoked_cert_config);

  ssl_test_util::CheckAuthenticationBrokenState(
      chrome_test_utils::GetActiveWebContents(this), net::CERT_STATUS_REVOKED,
      AuthState::SHOWING_INTERSTITIAL);

  net::CertStatus cert_status = GetCurrentCertStatus();
  EXPECT_TRUE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

IN_PROC_BROWSER_TEST_F(OCSPBrowserTest, TestHTTPSOCSPInvalid) {
  EnableRevocationChecking();

  net::EmbeddedTestServer::ServerCertificateConfig invalid_cert_config;
  invalid_cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      net::EmbeddedTestServer::OCSPConfig::ResponseType::kInvalidResponse);

  DoConnection(invalid_cert_config);

  ssl_test_util::CheckAuthenticatedState(
      chrome_test_utils::GetActiveWebContents(this), AuthState::NONE);

  net::CertStatus cert_status = GetCurrentCertStatus();
  EXPECT_TRUE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

IN_PROC_BROWSER_TEST_F(OCSPBrowserTest, TestHTTPSOCSPIntermediateValid) {
  EnableRevocationChecking();

  net::EmbeddedTestServer::ServerCertificateConfig
      intermediate_invalid_cert_config;
  intermediate_invalid_cert_config.intermediate =
      net::EmbeddedTestServer::IntermediateType::kInHandshake;
  intermediate_invalid_cert_config
      .ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});
  intermediate_invalid_cert_config
      .intermediate_ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  DoConnection(intermediate_invalid_cert_config);

  ssl_test_util::CheckAuthenticatedState(
      chrome_test_utils::GetActiveWebContents(this), AuthState::NONE);

  net::CertStatus cert_status = GetCurrentCertStatus();
  EXPECT_TRUE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

IN_PROC_BROWSER_TEST_F(OCSPBrowserTest,
                       TestHTTPSOCSPIntermediateResponseOldButStillValid) {
  EnableRevocationChecking();

  net::EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.intermediate =
      net::EmbeddedTestServer::IntermediateType::kInHandshake;
  cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});
  // Use an OCSP response for the intermediate that would be too old for a leaf
  // cert, but is still valid for an intermediate.
  cert_config.intermediate_ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kLong}});

  DoConnection(cert_config);

  net::CertStatus cert_status = GetCurrentCertStatus();
  ssl_test_util::CheckAuthenticationBrokenState(
      chrome_test_utils::GetActiveWebContents(this), net::CERT_STATUS_REVOKED,
      AuthState::SHOWING_INTERSTITIAL);
  EXPECT_TRUE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

IN_PROC_BROWSER_TEST_F(OCSPBrowserTest,
                       TestHTTPSOCSPIntermediateResponseTooOldKnownRoot) {
  EnableRevocationChecking();

  scoped_refptr<net::X509Certificate> root_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(root_cert);
  net::ScopedTestKnownRoot scoped_known_root(root_cert.get());

  net::EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.intermediate =
      net::EmbeddedTestServer::IntermediateType::kInHandshake;
  cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});
  // OCSP Response is too old and so should be ignored.
  cert_config.intermediate_ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kLonger}});
  cert_config.dns_names = {"example.com"};

  DoConnection("example.com", cert_config);
  net::CertStatus cert_status = GetCurrentCertStatus();

  if (ssl_test_util::UsingBuiltinCertVerifier()) {
    // The builtin verifier enforces the baseline requirements for max age
    // of an intermediate's OCSP response.
    ssl_test_util::CheckAuthenticatedState(
        chrome_test_utils::GetActiveWebContents(this), AuthState::NONE);
  } else {
    // The platform verifiers are more lenient.
    ssl_test_util::CheckAuthenticationBrokenState(
        chrome_test_utils::GetActiveWebContents(this), net::CERT_STATUS_REVOKED,
        AuthState::SHOWING_INTERSTITIAL);
  }

  EXPECT_TRUE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

IN_PROC_BROWSER_TEST_F(OCSPBrowserTest,
                       TestHTTPSOCSPIntermediateResponseTooOld) {
  EnableRevocationChecking();

  net::EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.intermediate =
      net::EmbeddedTestServer::IntermediateType::kInHandshake;
  cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});
  cert_config.intermediate_ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kLonger}});

  DoConnection(cert_config);
  net::CertStatus cert_status = GetCurrentCertStatus();

  // No limitation on response age for locally trusted roots.
  ssl_test_util::CheckAuthenticationBrokenState(
      chrome_test_utils::GetActiveWebContents(this), net::CERT_STATUS_REVOKED,
      AuthState::SHOWING_INTERSTITIAL);

  EXPECT_TRUE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

IN_PROC_BROWSER_TEST_F(OCSPBrowserTest, TestHTTPSOCSPIntermediateRevoked) {
  EnableRevocationChecking();

  net::EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.intermediate =
      net::EmbeddedTestServer::IntermediateType::kInHandshake;
  cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});
  cert_config.intermediate_ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  DoConnection(cert_config);

  net::CertStatus cert_status = GetCurrentCertStatus();
  ssl_test_util::CheckAuthenticationBrokenState(
      chrome_test_utils::GetActiveWebContents(this), net::CERT_STATUS_REVOKED,
      AuthState::SHOWING_INTERSTITIAL);
  EXPECT_TRUE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

IN_PROC_BROWSER_TEST_F(OCSPBrowserTest, TestHTTPSOCSPValidStapled) {
  if (!ssl_test_util::SystemSupportsOCSPStapling()) {
    LOG(WARNING)
        << "Skipping test because system doesn't support OCSP stapling";
    return;
  }

  EnableRevocationChecking();

  net::EmbeddedTestServer::ServerCertificateConfig cert_config;

  // AIA OCSP url is included, but does not return a successful ocsp response.
  cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      net::EmbeddedTestServer::OCSPConfig::ResponseType::kTryLater);

  cert_config.stapled_ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  DoConnection(cert_config);

  ssl_test_util::CheckAuthenticatedState(
      chrome_test_utils::GetActiveWebContents(this), AuthState::NONE);

  net::CertStatus cert_status = GetCurrentCertStatus();
  EXPECT_TRUE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

IN_PROC_BROWSER_TEST_F(OCSPBrowserTest, TestHTTPSOCSPRevokedStapled) {
  if (!ssl_test_util::SystemSupportsOCSPStapling()) {
    LOG(WARNING)
        << "Skipping test because system doesn't support OCSP stapling";
    return;
  }

  EnableRevocationChecking();

  net::EmbeddedTestServer::ServerCertificateConfig cert_config;

  // AIA OCSP url is included, but does not return a successful ocsp response.
  cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      net::EmbeddedTestServer::OCSPConfig::ResponseType::kTryLater);

  cert_config.stapled_ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  DoConnection(cert_config);

  ssl_test_util::CheckAuthenticationBrokenState(
      chrome_test_utils::GetActiveWebContents(this), net::CERT_STATUS_REVOKED,
      AuthState::SHOWING_INTERSTITIAL);

  net::CertStatus cert_status = GetCurrentCertStatus();
  EXPECT_TRUE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

IN_PROC_BROWSER_TEST_F(OCSPBrowserTest, TestHTTPSOCSPOldStapledAndInvalidAIA) {
  if (!ssl_test_util::SystemSupportsOCSPStapling()) {
    LOG(WARNING)
        << "Skipping test because system doesn't support OCSP stapling";
    return;
  }

  EnableRevocationChecking();

  net::EmbeddedTestServer::ServerCertificateConfig cert_config;
  // Stapled response indicates good, but is too old.
  cert_config.stapled_ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kOld}});

  // AIA OCSP url is included, but does not return a successful ocsp response.
  cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      net::EmbeddedTestServer::OCSPConfig::ResponseType::kTryLater);

  DoConnection(cert_config);
  ssl_test_util::CheckAuthenticatedState(
      chrome_test_utils::GetActiveWebContents(this), AuthState::NONE);

  net::CertStatus cert_status = GetCurrentCertStatus();
  EXPECT_TRUE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

IN_PROC_BROWSER_TEST_F(OCSPBrowserTest, TestHTTPSOCSPOldStapledButValidAIA) {
  if (!ssl_test_util::SystemSupportsOCSPStapling()) {
    LOG(WARNING)
        << "Skipping test because system doesn't support OCSP stapling";
    return;
  }

  EnableRevocationChecking();

  net::EmbeddedTestServer::ServerCertificateConfig cert_config;

  // Stapled response indicates good, but response is too old.
  cert_config.stapled_ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kOld}});

  // AIA OCSP url is included, and returns a successful ocsp response.
  cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  DoConnection(cert_config);
  ssl_test_util::CheckAuthenticatedState(
      chrome_test_utils::GetActiveWebContents(this), AuthState::NONE);

  net::CertStatus cert_status = GetCurrentCertStatus();
  EXPECT_TRUE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
IN_PROC_BROWSER_TEST_F(OCSPBrowserTest, HardFailOnOCSPInvalid) {
  if (!ssl_test_util::SystemSupportsHardFailRevocationChecking()) {
    LOG(WARNING) << "Skipping test because system doesn't support hard fail "
                 << "revocation checking";
    return;
  }

  // OCSP checking is disabled by default.
  EXPECT_FALSE(last_ssl_config().rev_checking_required_local_anchors);
  EXPECT_FALSE(g_browser_process->system_network_context_manager()
                   ->CreateDefaultNetworkContextParams()
                   ->initial_ssl_config->rev_checking_required_local_anchors);

  // Enable, and make sure the default network context params reflect the
  // change.
  base::RunLoop run_loop;
  set_ssl_config_updated_callback(run_loop.QuitClosure());
  ASSERT_NO_FATAL_FAILURE(
      EnablePolicy(g_browser_process->local_state(),
                   policy::key::kRequireOnlineRevocationChecksForLocalAnchors,
                   prefs::kCertRevocationCheckingRequiredLocalAnchors));
  run_loop.Run();
  EXPECT_TRUE(last_ssl_config().rev_checking_required_local_anchors);
  EXPECT_TRUE(g_browser_process->system_network_context_manager()
                  ->CreateDefaultNetworkContextParams()
                  ->initial_ssl_config->rev_checking_required_local_anchors);

  net::EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      net::EmbeddedTestServer::OCSPConfig::ResponseType::kInvalidResponse);

  DoConnection(cert_config);

  ssl_test_util::CheckAuthenticationBrokenState(
      chrome_test_utils::GetActiveWebContents(this),
      net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION,
      AuthState::SHOWING_INTERSTITIAL);

  net::CertStatus cert_status = GetCurrentCertStatus();
  EXPECT_TRUE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

IN_PROC_BROWSER_TEST_F(OCSPBrowserTest, HardFailOCSPInvalidUseStapled) {
  if (!ssl_test_util::SystemSupportsHardFailRevocationChecking()) {
    LOG(WARNING) << "Skipping test because system doesn't support hard fail "
                 << "revocation checking";
    return;
  }

  if (!ssl_test_util::SystemSupportsOCSPStapling()) {
    LOG(WARNING)
        << "Skipping test because system doesn't support OCSP stapling";
    return;
  }

  // OCSP checking is disabled by default.
  EXPECT_FALSE(last_ssl_config().rev_checking_required_local_anchors);
  EXPECT_FALSE(g_browser_process->system_network_context_manager()
                   ->CreateDefaultNetworkContextParams()
                   ->initial_ssl_config->rev_checking_required_local_anchors);

  // Enable hard-fail, and make sure the default network context params reflect
  // the change.
  base::RunLoop run_loop;
  set_ssl_config_updated_callback(run_loop.QuitClosure());
  ASSERT_NO_FATAL_FAILURE(
      EnablePolicy(g_browser_process->local_state(),
                   policy::key::kRequireOnlineRevocationChecksForLocalAnchors,
                   prefs::kCertRevocationCheckingRequiredLocalAnchors));
  run_loop.Run();
  EXPECT_TRUE(last_ssl_config().rev_checking_required_local_anchors);
  EXPECT_TRUE(g_browser_process->system_network_context_manager()
                  ->CreateDefaultNetworkContextParams()
                  ->initial_ssl_config->rev_checking_required_local_anchors);

  net::EmbeddedTestServer::ServerCertificateConfig cert_config;

  // AIA OCSP url is included, but does not return a successful ocsp response.
  cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      net::EmbeddedTestServer::OCSPConfig::ResponseType::kTryLater);

  cert_config.stapled_ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  DoConnection(cert_config);

  ssl_test_util::CheckAuthenticatedState(
      chrome_test_utils::GetActiveWebContents(this), AuthState::NONE);

  net::CertStatus cert_status = GetCurrentCertStatus();
  EXPECT_TRUE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

IN_PROC_BROWSER_TEST_F(OCSPBrowserTest,
                       HardFailTestHTTPSOCSPOldStapledAndInvalidAIA) {
  if (!ssl_test_util::SystemSupportsHardFailRevocationChecking()) {
    LOG(WARNING) << "Skipping test because system doesn't support hard fail "
                 << "revocation checking";
    return;
  }

  if (!ssl_test_util::SystemSupportsOCSPStapling()) {
    LOG(WARNING)
        << "Skipping test because system doesn't support OCSP stapling";
    return;
  }

  // OCSP checking is disabled by default.
  EXPECT_FALSE(last_ssl_config().rev_checking_required_local_anchors);
  EXPECT_FALSE(g_browser_process->system_network_context_manager()
                   ->CreateDefaultNetworkContextParams()
                   ->initial_ssl_config->rev_checking_required_local_anchors);

  // Enable hard-fail, and make sure the default network context params reflect
  // the change.
  base::RunLoop run_loop;
  set_ssl_config_updated_callback(run_loop.QuitClosure());
  ASSERT_NO_FATAL_FAILURE(
      EnablePolicy(g_browser_process->local_state(),
                   policy::key::kRequireOnlineRevocationChecksForLocalAnchors,
                   prefs::kCertRevocationCheckingRequiredLocalAnchors));
  run_loop.Run();
  EXPECT_TRUE(last_ssl_config().rev_checking_required_local_anchors);
  EXPECT_TRUE(g_browser_process->system_network_context_manager()
                  ->CreateDefaultNetworkContextParams()
                  ->initial_ssl_config->rev_checking_required_local_anchors);

  net::EmbeddedTestServer::ServerCertificateConfig cert_config;
  // Stapled response indicates good, but is too old.
  cert_config.stapled_ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kOld}});

  // AIA OCSP url is included, but does not return a successful ocsp response.
  cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      net::EmbeddedTestServer::OCSPConfig::ResponseType::kTryLater);

  DoConnection(cert_config);
  ssl_test_util::CheckAuthenticationBrokenState(
      chrome_test_utils::GetActiveWebContents(this),
      net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION,
      AuthState::SHOWING_INTERSTITIAL);

  net::CertStatus cert_status = GetCurrentCertStatus();
  EXPECT_TRUE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

IN_PROC_BROWSER_TEST_F(OCSPBrowserTest,
                       HardFailTestHTTPSOCSPOldStapledButValidAIA) {
  if (!ssl_test_util::SystemSupportsHardFailRevocationChecking()) {
    LOG(WARNING) << "Skipping test because system doesn't support hard fail "
                 << "revocation checking";
    return;
  }

  if (!ssl_test_util::SystemSupportsOCSPStapling()) {
    LOG(WARNING)
        << "Skipping test because system doesn't support OCSP stapling";
    return;
  }

  // OCSP checking is disabled by default.
  EXPECT_FALSE(last_ssl_config().rev_checking_required_local_anchors);
  EXPECT_FALSE(g_browser_process->system_network_context_manager()
                   ->CreateDefaultNetworkContextParams()
                   ->initial_ssl_config->rev_checking_required_local_anchors);

  // Enable hard-fail, and make sure the default network context params reflect
  // the change.
  base::RunLoop run_loop;
  set_ssl_config_updated_callback(run_loop.QuitClosure());
  ASSERT_NO_FATAL_FAILURE(
      EnablePolicy(g_browser_process->local_state(),
                   policy::key::kRequireOnlineRevocationChecksForLocalAnchors,
                   prefs::kCertRevocationCheckingRequiredLocalAnchors));
  run_loop.Run();
  EXPECT_TRUE(last_ssl_config().rev_checking_required_local_anchors);
  EXPECT_TRUE(g_browser_process->system_network_context_manager()
                  ->CreateDefaultNetworkContextParams()
                  ->initial_ssl_config->rev_checking_required_local_anchors);

  net::EmbeddedTestServer::ServerCertificateConfig cert_config;

  // Stapled response indicates good, but response is too old.
  cert_config.stapled_ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kOld}});

  // AIA OCSP url is included, and returns a successful ocsp response.
  cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  DoConnection(cert_config);
  ssl_test_util::CheckAuthenticatedState(
      chrome_test_utils::GetActiveWebContents(this), AuthState::NONE);

  net::CertStatus cert_status = GetCurrentCertStatus();
  EXPECT_TRUE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || \
        // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

class EVBrowserTest : public OCSPBrowserTest {
 public:
  void SetUpOnMainThread() override {
    OCSPBrowserTest::SetUpOnMainThread();

    // TODO(crbug.com/40693524): when the CertVerifierService is moved
    // out of process, the ScopedTestEVPolicy needs to be instantiated in
    // that process.
    scoped_refptr<net::X509Certificate> root_cert = net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "root_ca_cert.pem");
    ASSERT_TRUE(root_cert);

    ev_test_policy_ = std::make_unique<net::ScopedTestEVPolicy>(
        net::EVRootCAMetadata::GetInstance(),
        net::X509Certificate::CalculateFingerprint256(root_cert->cert_buffer()),
        kOCSPTestCertPolicy);
  }

 private:
  std::unique_ptr<net::ScopedTestEVPolicy> ev_test_policy_;
};

IN_PROC_BROWSER_TEST_F(EVBrowserTest, TestHTTPSEVNoPolicySet) {
  // OCSP checking is disabled by default.
  EXPECT_FALSE(last_ssl_config().rev_checking_enabled);
  EXPECT_FALSE(g_browser_process->system_network_context_manager()
                   ->CreateDefaultNetworkContextParams()
                   ->initial_ssl_config->rev_checking_enabled);

  net::EmbeddedTestServer::ServerCertificateConfig ok_cert_config;
  DoConnection(ok_cert_config);

  ssl_test_util::CheckAuthenticatedState(
      chrome_test_utils::GetActiveWebContents(this), AuthState::NONE);

  net::CertStatus cert_status = GetCurrentCertStatus();
  EXPECT_FALSE(cert_status & net::CERT_STATUS_IS_EV);
  EXPECT_FALSE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

IN_PROC_BROWSER_TEST_F(EVBrowserTest, TestHTTPSEVNoOCSPCheck) {
  // OCSP checking is disabled by default.
  EXPECT_FALSE(last_ssl_config().rev_checking_enabled);
  EXPECT_FALSE(g_browser_process->system_network_context_manager()
                   ->CreateDefaultNetworkContextParams()
                   ->initial_ssl_config->rev_checking_enabled);

  net::EmbeddedTestServer::ServerCertificateConfig ok_cert_config;
  ok_cert_config.policy_oids = {kOCSPTestCertPolicy};
  DoConnection(ok_cert_config);

  ssl_test_util::CheckAuthenticatedState(
      chrome_test_utils::GetActiveWebContents(this), AuthState::NONE);

  net::CertStatus cert_status = GetCurrentCertStatus();
  EXPECT_EQ(ssl_test_util::SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & net::CERT_STATUS_IS_EV));
  EXPECT_FALSE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

// Test EV checking when revocation checking is explicitly enabled and we have a
// revoked OCSP response.
IN_PROC_BROWSER_TEST_F(EVBrowserTest, TestHTTPSOCSPRevoked) {
  EnableRevocationChecking();

  net::EmbeddedTestServer::ServerCertificateConfig revoked_cert_config;
  revoked_cert_config.policy_oids = {kOCSPTestCertPolicy};
  revoked_cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  DoConnection(revoked_cert_config);

  ssl_test_util::CheckAuthenticationBrokenState(
      chrome_test_utils::GetActiveWebContents(this), net::CERT_STATUS_REVOKED,
      AuthState::SHOWING_INTERSTITIAL);

  net::CertStatus cert_status = GetCurrentCertStatus();
  EXPECT_FALSE(cert_status & net::CERT_STATUS_IS_EV);
  EXPECT_TRUE(cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED);
}

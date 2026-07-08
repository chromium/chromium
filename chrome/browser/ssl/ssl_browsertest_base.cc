// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_browsertest_base.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/content/ssl_error_handler.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/install_default_websocket_handlers.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "third_party/blink/public/common/features.h"

SSLUITestBase::SSLUITestBase()
    : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
      https_server_expired_(net::EmbeddedTestServer::TYPE_HTTPS),
      https_server_mismatched_(net::EmbeddedTestServer::TYPE_HTTPS),
      https_server_sha1_(net::EmbeddedTestServer::TYPE_HTTPS),
      https_server_common_name_only_(net::EmbeddedTestServer::TYPE_HTTPS),
      wss_server_expired_(net::EmbeddedTestServer::TYPE_HTTPS),
      wss_server_mismatched_(net::EmbeddedTestServer::TYPE_HTTPS) {
  https_server_.AddDefaultHandlers(GetChromeTestDataDir());

  https_server_expired_.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server_expired_.AddDefaultHandlers(GetChromeTestDataDir());

  https_server_mismatched_.SetSSLConfig(
      net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server_mismatched_.AddDefaultHandlers(GetChromeTestDataDir());

  https_server_sha1_.SetSSLConfig(net::EmbeddedTestServer::CERT_SHA1_LEAF);
  https_server_sha1_.AddDefaultHandlers(GetChromeTestDataDir());

  https_server_common_name_only_.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_ONLY);
  https_server_common_name_only_.AddDefaultHandlers(GetChromeTestDataDir());

  wss_server_expired_.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  net::test_server::InstallDefaultWebSocketHandlers(&wss_server_expired_);
  wss_server_expired_.AddDefaultHandlers(GetChromeTestDataDir());

  wss_server_mismatched_.SetSSLConfig(
      net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  net::test_server::InstallDefaultWebSocketHandlers(&wss_server_mismatched_);
  wss_server_mismatched_.AddDefaultHandlers(GetChromeTestDataDir());
}

SSLUITestBase::~SSLUITestBase() = default;

void SSLUITestBase::SetUp() {
  policy_provider_.SetDefaultReturns(
      /*is_initialization_complete_return=*/true,
      /*is_first_policy_load_complete_return=*/true);
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
      &policy_provider_);

  InProcessBrowserTest::SetUp();
  SSLErrorHandler::ResetConfigForTesting();
}

void SSLUITestBase::TearDown() {
  SSLErrorHandler::ResetConfigForTesting();
  InProcessBrowserTest::TearDown();
}

void SSLUITestBase::SetUpCommandLine(base::CommandLine* command_line) {
  // Browser will both run and display insecure content.
  command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
  // Use process-per-site so that navigating to a same-site page in a
  // new tab will use the same process.
  command_line->AppendSwitch(switches::kProcessPerSite);
}

void SSLUITestBase::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
  network::mojom::NetworkContextParamsPtr context_params =
      CreateDefaultNetworkContextParams();
  last_ssl_config_ = *context_params->initial_ssl_config;
  receiver_.Bind(std::move(context_params->ssl_config_client_receiver));
}

void SSLUITestBase::TearDownOnMainThread() {
  receiver_.reset();
}

void SSLUITestBase::OnSSLConfigUpdated(
    network::mojom::SSLConfigPtr ssl_config) {
  last_ssl_config_ = *ssl_config;
}

// static
std::string SSLUITestBase::GetFilePathWithHostAndPortReplacement(
    const std::string& original_file_path,
    const net::HostPortPair& host_port_pair) {
  base::StringPairs replacement_text;
  replacement_text.emplace_back(
      std::make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
  return net::test_server::GetFilePathWithReplacements(original_file_path,
                                                       replacement_text);
}

// static
std::vector<base::test::FeatureRef> SSLUITestBase::GetDisabledFeatures() {
  return {blink::features::kMixedContentAutoupgrade};
}

// static
std::string SSLUITestBase::GetTopFramePath(
    const net::EmbeddedTestServer& http_server,
    const net::EmbeddedTestServer& good_https_server,
    const net::EmbeddedTestServer& bad_https_server) {
  // The "frame_left.html" page contained in the top_frame.html page contains
  // <a href>'s to three different servers. This sets up all of the
  // replacement text to work with test servers which listen on ephemeral
  // ports.
  GURL http_url = http_server.GetURL("/ssl/google.html");
  GURL good_https_url = good_https_server.GetURL("/ssl/google.html");
  GURL bad_https_url = bad_https_server.GetURL("/ssl/bad_iframe.html");

  base::StringPairs replacement_text_frame_left;
  replacement_text_frame_left.emplace_back(
      std::make_pair("REPLACE_WITH_HTTP_PORT", http_url.GetPort()));
  replacement_text_frame_left.emplace_back(
      std::make_pair("REPLACE_WITH_GOOD_HTTPS_PAGE", good_https_url.spec()));
  replacement_text_frame_left.emplace_back(
      std::make_pair("REPLACE_WITH_BAD_HTTPS_PAGE", bad_https_url.spec()));
  std::string frame_left_path = net::test_server::GetFilePathWithReplacements(
      "frame_left.html", replacement_text_frame_left);

  // Substitute the generated frame_left URL into the top_frame page.
  base::StringPairs replacement_text_top_frame;
  replacement_text_top_frame.emplace_back(
      std::make_pair("REPLACE_WITH_FRAME_LEFT_PATH", frame_left_path));
  return net::test_server::GetFilePathWithReplacements(
      "/ssl/top_frame.html", replacement_text_top_frame);
}

void SSLUITestBase::ProceedThroughInterstitial(content::WebContents* tab) {
  content::TestNavigationObserver nav_observer(tab, 1);
  SendInterstitialCommand(tab, security_interstitials::CMD_PROCEED);
  nav_observer.Wait();
}

void SSLUITestBase::DontProceedThroughInterstitial(content::WebContents* tab) {
  SendInterstitialCommand(tab, security_interstitials::CMD_DONT_PROCEED);
}

void SSLUITestBase::SendInterstitialCommand(
    content::WebContents* tab,
    security_interstitials::SecurityInterstitialCommand command) {
  std::string javascript;
  switch (command) {
    case security_interstitials::CMD_DONT_PROCEED: {
      javascript = "window.certificateErrorPageController.dontProceed();";
      break;
    }
    case security_interstitials::CMD_PROCEED: {
      javascript = "window.certificateErrorPageController.proceed();";
      break;
    }
    case security_interstitials::CMD_SHOW_MORE_SECTION: {
      javascript = "window.certificateErrorPageController.showMoreSection();";
      break;
    }
    case security_interstitials::CMD_OPEN_HELP_CENTER: {
      javascript = "window.certificateErrorPageController.openHelpCenter();";
      break;
    }
    case security_interstitials::CMD_OPEN_DIAGNOSTIC: {
      javascript = "window.certificateErrorPageController.openDiagnostic();";
      break;
    }
    case security_interstitials::CMD_RELOAD: {
      javascript = "window.certificateErrorPageController.reload();";
      break;
    }
    case security_interstitials::CMD_OPEN_DATE_SETTINGS: {
      javascript = "window.certificateErrorPageController.openDateSettings();";
      break;
    }
    case security_interstitials::CMD_OPEN_LOGIN: {
      javascript = "window.certificateErrorPageController.openLogin();";
      break;
    }
    case security_interstitials::CMD_DO_REPORT: {
      javascript = "window.certificateErrorPageController.doReport();";
      break;
    }
    case security_interstitials::CMD_DONT_REPORT: {
      javascript = "window.certificateErrorPageController.dontReport();";
      break;
    }
    case security_interstitials::CMD_OPEN_REPORTING_PRIVACY: {
      javascript =
          "window.certificateErrorPageController.openReportingPrivacy();";
      break;
    }
    case security_interstitials::CMD_OPEN_WHITEPAPER: {
      javascript = "window.certificateErrorPageController.openWhitepaper();";
      break;
    }
    case security_interstitials::CMD_REPORT_PHISHING_ERROR: {
      javascript =
          "window.certificateErrorPageController.reportPhishingError();";
      break;
    }
    case security_interstitials::CMD_OPEN_HELP_CENTER_IN_NEW_TAB: {
      javascript =
          "window.certificateErrorPageController.openHelpCenterInNewTab();";
      break;
    }
    case security_interstitials::CMD_OPEN_REPORTING_PRIVACY_IN_NEW_TAB: {
      javascript =
          "window.certificateErrorPageController."
          "openReportingPrivacyInNewTab();";
      break;
    }
    case security_interstitials::CMD_OPEN_WHITEPAPER_IN_NEW_TAB: {
      javascript =
          "window.certificateErrorPageController.openWhitepaperInNewTab();";
      break;
    }
    default: {
      // Other values in the enum are not used by these tests, and don't
      // have a Javascript equivalent that can be called here.
      NOTREACHED();
    }
  }
  ASSERT_TRUE(content::ExecJs(tab, javascript));
}

void SSLUITestBase::SetUpUnsafeContentsWithUserException(
    const std::string& path) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_mismatched_.Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html")));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID,
      ssl_test_util::AuthState::SHOWING_INTERSTITIAL);
  ProceedThroughInterstitial(tab);
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID,
      ssl_test_util::AuthState::NONE);

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      path, https_server_mismatched_.host_port_pair());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
}

void SSLUITestBase::UpdateChromePolicy(const policy::PolicyMap& policies) {
  policy_provider_.UpdateChromePolicy(policies);
  ASSERT_TRUE(base::CurrentThread::Get());

  base::RunLoop().RunUntilIdle();

  content::FlushNetworkServiceInstanceForTesting();
}

void SSLUITestBase::RunOnIOThreadBlocking(base::OnceClosure task) {
  base::RunLoop run_loop;
  content::GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE, std::move(task), run_loop.QuitClosure());
  run_loop.Run();
}

security_interstitials::SecurityInterstitialPage*
SSLUITestBase::GetInterstitialPage(content::WebContents* tab) {
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  if (!helper) {
    return nullptr;
  }
  return helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();
}

security_interstitials::SecurityInterstitialControllerClient*
SSLUITestBase::GetControllerClientFromSSLBlockingPage(
    SSLBlockingPage* ssl_interstitial) {
  return ssl_interstitial->controller();
}

network::mojom::NetworkContextParamsPtr
SSLUITestBase::CreateDefaultNetworkContextParams() {
  return g_browser_process->system_network_context_manager()
      ->CreateDefaultNetworkContextParams();
}

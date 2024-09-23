// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/interstitials/security_interstitial_idn_test.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "chrome/browser/ssl/ssl_error_controller_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/common/content_settings_agent.mojom.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/embedder_support/switches.h"
#include "components/error_page/content/browser/net_error_auto_reloader.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/network_time/network_time_test_utils.h"
#include "components/network_time/network_time_tracker.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/bad_clock_blocking_page.h"
#include "components/security_interstitials/content/captive_portal_blocking_page.h"
#include "components/security_interstitials/content/common_name_mismatch_handler.h"
#include "components/security_interstitials/content/insecure_form_blocking_page.h"
#include "components/security_interstitials/content/insecure_form_navigation_throttle.h"
#include "components/security_interstitials/content/mitm_software_blocking_page.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/content/ssl_error_assistant.h"
#include "components/security_interstitials/content/ssl_error_assistant.pb.h"
#include "components/security_interstitials/content/ssl_error_handler.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_params_manager.h"
#include "components/variations/variations_switches.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_entry_restore_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "crypto/sha2.h"
#include "extensions/browser/event_router.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_database.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_info.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(USE_NSS_CERTS)
#include "chrome/browser/certificate_manager_model.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_util_nss.h"
#endif  // BUILDFLAG(USE_NSS_CERTS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "base/path_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/session_manager/core/session_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using content::WebContents;
namespace AuthState = ssl_test_util::AuthState;
namespace CertError = ssl_test_util::CertError;

namespace {

const int kLargeVersionId = 0xFFFFFF;

const char kHstsTestHostName[] = "hsts-example.test";

enum ProceedDecision {
  SSL_INTERSTITIAL_PROCEED,
  SSL_INTERSTITIAL_DO_NOT_PROCEED
};

// A WebContentsObserver that allows observing when the page has set a
// particular SSL content status flag. Assumes that the flag is not set when the
// observer is created.
class SSLContentStatusObserver : public content::WebContentsObserver {
 public:
  explicit SSLContentStatusObserver(content::WebContents* web_contents,
                                    content::SSLStatus::ContentStatusFlags flag)
      : content::WebContentsObserver(web_contents), flag_(flag) {
    content::NavigationEntry* entry =
        web_contents->GetController().GetVisibleEntry();
    if (entry) {
      DCHECK(!(entry->GetSSL().content_status & flag_));
    }
  }

  SSLContentStatusObserver(const SSLContentStatusObserver&) = delete;
  SSLContentStatusObserver& operator=(const SSLContentStatusObserver&) = delete;

  ~SSLContentStatusObserver() override = default;

  void DidChangeVisibleSecurityState() override {
    content::NavigationEntry* entry =
        web_contents()->GetController().GetVisibleEntry();
    if (entry && (entry->GetSSL().content_status & flag_)) {
      run_loop_.Quit();
    }
  }

  void WaitForSSLContentStatusFlag() { run_loop_.Run(); }

 private:
  // The content status flag of interest
  content::SSLStatus::ContentStatusFlags flag_;
  base::RunLoop run_loop_;
};

// This observer waits for the SSLErrorHandler to start an interstitial timer
// for the given web contents.
class SSLInterstitialTimerObserver {
 public:
  explicit SSLInterstitialTimerObserver(WebContents* web_contents)
      : web_contents_(web_contents), message_loop_runner_(new base::RunLoop) {
    callback_ = base::BindRepeating(
        &SSLInterstitialTimerObserver::OnTimerStarted, base::Unretained(this));
    SSLErrorHandler::SetInterstitialTimerStartedCallbackForTesting(&callback_);
  }

  SSLInterstitialTimerObserver(const SSLInterstitialTimerObserver&) = delete;
  SSLInterstitialTimerObserver& operator=(const SSLInterstitialTimerObserver&) =
      delete;

  ~SSLInterstitialTimerObserver() {
    SSLErrorHandler::SetInterstitialTimerStartedCallbackForTesting(nullptr);
  }

  // Waits until the interstitial delay timer in SSLErrorHandler is started.
  void WaitForTimerStarted() { message_loop_runner_->Run(); }

  // Returns true if the interstitial delay timer has been started.
  bool timer_started() const { return timer_started_; }

 private:
  void OnTimerStarted(WebContents* web_contents) {
    timer_started_ = true;
    if (web_contents_ == web_contents)
      message_loop_runner_->Quit();
  }

  bool timer_started_ = false;
  raw_ptr<const WebContents> web_contents_;
  SSLErrorHandler::TimerStartedCallback callback_;

  std::unique_ptr<base::RunLoop> message_loop_runner_;
};

class ChromeContentBrowserClientForMixedContentTest
    : public ChromeContentBrowserClient {
 public:
  ChromeContentBrowserClientForMixedContentTest() {}

  ChromeContentBrowserClientForMixedContentTest(
      const ChromeContentBrowserClientForMixedContentTest&) = delete;
  ChromeContentBrowserClientForMixedContentTest& operator=(
      const ChromeContentBrowserClientForMixedContentTest&) = delete;

  void OverrideWebkitPrefs(
      content::WebContents* web_contents,
      blink::web_pref::WebPreferences* web_prefs) override {
    web_prefs->allow_running_insecure_content = allow_running_insecure_content_;
    web_prefs->strict_mixed_content_checking = strict_mixed_content_checking_;
    web_prefs->strictly_block_blockable_mixed_content =
        strictly_block_blockable_mixed_content_;
  }
  void SetMixedContentSettings(bool allow_running_insecure_content,
                               bool strict_mixed_content_checking,
                               bool strictly_block_blockable_mixed_content) {
    allow_running_insecure_content_ = allow_running_insecure_content;
    strict_mixed_content_checking_ = strict_mixed_content_checking;
    strictly_block_blockable_mixed_content_ =
        strictly_block_blockable_mixed_content;
  }

 private:
  bool allow_running_insecure_content_ = false;
  bool strict_mixed_content_checking_ = false;
  bool strictly_block_blockable_mixed_content_ = false;
};

std::string EncodeQuery(const std::string& query) {
  url::RawCanonOutputT<char> buffer;
  url::EncodeURIComponent(query, &buffer);
  return std::string(buffer.view());
}

// Returns the Sha256 hash of the SPKI of |cert|.
net::HashValue GetSPKIHash(const CRYPTO_BUFFER* cert) {
  std::string_view spki_bytes;
  EXPECT_TRUE(net::asn1::ExtractSPKIFromDERCert(
      net::x509_util::CryptoBufferAsStringPiece(cert), &spki_bytes));
  net::HashValue sha256(net::HASH_VALUE_SHA256);
  crypto::SHA256HashString(spki_bytes, sha256.data(), crypto::kSHA256Length);
  return sha256;
}

// Compares two SSLStatuses to check if they match up before and after an
// interstitial. To match up, they should have the same connection information
// properties, such as certificate, connection status, connection security,
// etc. Content status and user data are not compared. Returns true if the
// statuses match and false otherwise.
bool ComparePreAndPostInterstitialSSLStatuses(const content::SSLStatus& one,
                                              const content::SSLStatus& two) {
  // TODO(mattm): It feels like this should use
  // certificate->EqualsIncludingChain, but that fails on some platforms. Find
  // out why and document or fix.
  return one.initialized == two.initialized &&
         !!one.certificate == !!two.certificate &&
         (one.certificate
              ? one.certificate->EqualsExcludingChain(two.certificate.get())
              : true) &&
         one.cert_status == two.cert_status &&
         one.key_exchange_group == two.key_exchange_group &&
         // Skip comparing the peer_signature_algorithm, because it is not
         // filled in by the time of an interstitial.
         one.connection_status == two.connection_status &&
         one.pkp_bypassed == two.pkp_bypassed;
}

void ExpectInterstitialElementHidden(WebContents* tab,
                                     const std::string& element_id,
                                     bool expect_hidden) {
  content::RenderFrameHost* frame = tab->GetPrimaryMainFrame();
  // Send CMD_TEXT_FOUND to indicate that the 'hidden' class is found, and
  // CMD_TEXT_NOT_FOUND if not.
  std::string command = base::StringPrintf(
      "document.querySelector('#%s')"
      "    .classList.contains('hidden')"
      " ? %d : %d;",
      element_id.c_str(), security_interstitials::CMD_TEXT_FOUND,
      security_interstitials::CMD_TEXT_NOT_FOUND);
  int result = content::EvalJs(frame, command).ExtractInt();
  EXPECT_EQ(expect_hidden ? security_interstitials::CMD_TEXT_FOUND
                          : security_interstitials::CMD_TEXT_NOT_FOUND,
            result);
}

}  // namespace

class SSLUITestBase : public InProcessBrowserTest,
                      public network::mojom::SSLConfigClient {
 public:
  SSLUITestBase()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_expired_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_mismatched_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_sha1_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_common_name_only_(net::EmbeddedTestServer::TYPE_HTTPS),
        wss_server_expired_(net::SpawnedTestServer::TYPE_WSS,
                            SSLOptions(SSLOptions::CERT_EXPIRED),
                            net::GetWebSocketTestDataDirectory()),
        wss_server_mismatched_(net::SpawnedTestServer::TYPE_WSS,
                               SSLOptions(SSLOptions::CERT_MISMATCHED_NAME),
                               net::GetWebSocketTestDataDirectory()) {
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
  }

  SSLUITestBase(const SSLUITestBase&) = delete;
  SSLUITestBase& operator=(const SSLUITestBase&) = delete;

  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    InProcessBrowserTest::SetUp();
    SSLErrorHandler::ResetConfigForTesting();
  }

  void TearDown() override {
    SSLErrorHandler::ResetConfigForTesting();
    InProcessBrowserTest::TearDown();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Browser will both run and display insecure content.
    command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
    // Use process-per-site so that navigating to a same-site page in a
    // new tab will use the same process.
    command_line->AppendSwitch(switches::kProcessPerSite);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    network::mojom::NetworkContextParamsPtr context_params =
        CreateDefaultNetworkContextParams();
    last_ssl_config_ = *context_params->initial_ssl_config;
    receiver_.Bind(std::move(context_params->ssl_config_client_receiver));
  }

  void TearDownOnMainThread() override { receiver_.reset(); }

  void ProceedThroughInterstitial(WebContents* tab) {
    content::TestNavigationObserver nav_observer(tab, 1);
    SendInterstitialCommand(tab, security_interstitials::CMD_PROCEED);
    nav_observer.Wait();
  }

  virtual void DontProceedThroughInterstitial(WebContents* tab) {
    SendInterstitialCommand(tab, security_interstitials::CMD_DONT_PROCEED);
  }

  void SendInterstitialCommand(
      WebContents* tab,
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
        javascript =
            "window.certificateErrorPageController.openDateSettings();";
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
      default: {
        // Other values in the enum are not used by these tests, and don't
        // have a Javascript equivalent that can be called here.
        NOTREACHED_IN_MIGRATION();
      }
    }
    ASSERT_TRUE(content::ExecJs(tab, javascript));
    return;
  }

  network::mojom::NetworkContextParamsPtr CreateDefaultNetworkContextParams() {
    return g_browser_process->system_network_context_manager()
        ->CreateDefaultNetworkContextParams();
  }

  static std::string GetFilePathWithHostAndPortReplacement(
      const std::string& original_file_path,
      const net::HostPortPair& host_port_pair) {
    base::StringPairs replacement_text;
    replacement_text.push_back(
        make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
    return net::test_server::GetFilePathWithReplacements(original_file_path,
                                                         replacement_text);
  }

  static std::string GetTopFramePath(
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
    replacement_text_frame_left.push_back(
        make_pair("REPLACE_WITH_HTTP_PORT", http_url.port()));
    replacement_text_frame_left.push_back(
        make_pair("REPLACE_WITH_GOOD_HTTPS_PAGE", good_https_url.spec()));
    replacement_text_frame_left.push_back(
        make_pair("REPLACE_WITH_BAD_HTTPS_PAGE", bad_https_url.spec()));
    std::string frame_left_path = net::test_server::GetFilePathWithReplacements(
        "frame_left.html", replacement_text_frame_left);

    // Substitute the generated frame_left URL into the top_frame page.
    base::StringPairs replacement_text_top_frame;
    replacement_text_top_frame.push_back(
        make_pair("REPLACE_WITH_FRAME_LEFT_PATH", frame_left_path));
    return net::test_server::GetFilePathWithReplacements(
        "/ssl/top_frame.html", replacement_text_top_frame);
  }

  security_interstitials::SecurityInterstitialPage* GetInterstitialPage(
      WebContents* tab) {
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            tab);
    if (!helper)
      return nullptr;
    return helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();
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

  // Sets the policy identified by |policy_name| to |policy_value|.
  void SetPolicy(const char* policy_name, base::Value policy_value) {
    policy::PolicyMap policy_map;
    policy_map.Set(policy_name, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                   std::move(policy_value), nullptr);

    EXPECT_NO_FATAL_FAILURE(UpdateChromePolicy(policy_map));

    // Wait for the updated SSL configuration to be sent to the network service,
    // to avoid a race.
    g_browser_process->system_network_context_manager()
        ->FlushSSLConfigManagerForTesting();
  }

  // Helper function for TestInterstitialLinksOpenInNewTab. Implemented as a
  // test fixture method because the whole test fixture class is friended by
  // SSLBlockingPage.
  security_interstitials::SecurityInterstitialControllerClient*
  GetControllerClientFromSSLBlockingPage(SSLBlockingPage* ssl_interstitial) {
    return ssl_interstitial->controller();
  }

  // Helper function that checks that after proceeding through an interstitial,
  // the app window is closed, a new tab with the app URL is opened, and there
  // is no interstitial.
  void ProceedThroughInterstitialInAppAndCheckNewTabOpened(
      Browser* app_browser,
      const GURL& app_url) {
    Profile* profile = browser()->profile();

    size_t num_browsers = chrome::GetBrowserCount(profile);
    EXPECT_EQ(app_browser, chrome::FindLastActive());
    int num_tabs = browser()->tab_strip_model()->count();

    ProceedThroughInterstitial(
        app_browser->tab_strip_model()->GetActiveWebContents());

    EXPECT_EQ(--num_browsers, chrome::GetBrowserCount(profile));
    EXPECT_EQ(browser(), chrome::FindLastActive());
    EXPECT_EQ(++num_tabs, browser()->tab_strip_model()->count());

    WebContents* new_tab = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(new_tab));

    ssl_test_util::CheckAuthenticationBrokenState(
        new_tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
    EXPECT_EQ(app_url, new_tab->GetVisibleURL());
  }

  // network::mojom::SSLConfigClient implementation.
  void OnSSLConfigUpdated(network::mojom::SSLConfigPtr ssl_config) override {
    last_ssl_config_ = *ssl_config;
  }

 protected:
  typedef net::SpawnedTestServer::SSLOptions SSLOptions;

  // Navigates to an interstitial and clicks through the certificate
  // error; then navigates to a page at |path| that loads unsafe content.
  void SetUpUnsafeContentsWithUserException(const std::string& path) {
    ASSERT_TRUE(https_server_.Start());
    // Note that it is necessary to user https_server_mismatched_ here over the
    // other invalid cert servers. This is because the test relies on the two
    // servers having different hosts since SSL exceptions are per-host, not per
    // origin, and https_server_mismatched_ uses 'localhost' rather than
    // '127.0.0.1'.
    ASSERT_TRUE(https_server_mismatched_.Start());

    // Navigate to an unsafe site. Proceed with interstitial page to indicate
    // the user approves the bad certificate.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html")));
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    ssl_test_util::CheckAuthenticationBrokenState(
        tab, net::CERT_STATUS_COMMON_NAME_INVALID,
        AuthState::SHOWING_INTERSTITIAL);
    ProceedThroughInterstitial(tab);
    ssl_test_util::CheckAuthenticationBrokenState(
        tab, net::CERT_STATUS_COMMON_NAME_INVALID, AuthState::NONE);

    std::string replacement_path = GetFilePathWithHostAndPortReplacement(
        path, https_server_mismatched_.host_port_pair());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL(replacement_path)));
  }

  void UpdateChromePolicy(const policy::PolicyMap& policies) {
    policy_provider_.UpdateChromePolicy(policies);
    ASSERT_TRUE(base::CurrentThread::Get());

    base::RunLoop().RunUntilIdle();

    content::FlushNetworkServiceInstanceForTesting();
  }

  void RunOnIOThreadBlocking(base::OnceClosure task) {
    base::RunLoop run_loop;
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE, std::move(task), run_loop.QuitClosure());
    run_loop.Run();
  }

  net::EmbeddedTestServer https_server_;
  net::EmbeddedTestServer https_server_expired_;
  net::EmbeddedTestServer https_server_mismatched_;
  net::EmbeddedTestServer https_server_sha1_;
  net::EmbeddedTestServer https_server_common_name_only_;

  net::SpawnedTestServer wss_server_expired_;
  net::SpawnedTestServer wss_server_mismatched_;

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;

  network::mojom::SSLConfig last_ssl_config_;
  mojo::Receiver<network::mojom::SSLConfigClient> receiver_{this};
};

class SSLUITest : public SSLUITestBase {
 public:
  SSLUITest() : SSLUITestBase() {
    scoped_feature_list_.InitWithFeatures(
        /* enabled_features */ {},
        /* disabled_features */ {blink::features::kMixedContentAutoupgrade});
  }

  SSLUITest(const SSLUITest&) = delete;
  SSLUITest& operator=(const SSLUITest&) = delete;

 protected:
  void DontProceedThroughInterstitial(WebContents* tab) override {
    content::TestNavigationObserver nav_observer(tab, 1);
    SendInterstitialCommand(tab, security_interstitials::CMD_DONT_PROCEED);
    nav_observer.Wait();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class SSLUITestBlock : public SSLUITest {
 public:
  SSLUITestBlock() : SSLUITest() {}

  // Browser will not run insecure content.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // By overriding SSLUITest, we won't apply the flag that allows running
    // insecure content.
  }
};

class SSLUITestIgnoreCertErrors : public SSLUITest {
 public:
  SSLUITestIgnoreCertErrors() : SSLUITest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SSLUITest::SetUpCommandLine(command_line);
    // Browser will ignore certificate errors.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }
};

static std::string MakeCertSPKIFingerprint(net::X509Certificate* cert) {
  return base::Base64Encode(GetSPKIHash(cert->cert_buffer()));
}

class SSLUITestIgnoreCertErrorsBySPKIHTTPS : public SSLUITest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SSLUITest::SetUpCommandLine(command_line);
    std::string whitelist_flag = MakeCertSPKIFingerprint(
        https_server_mismatched_.GetCertificate().get());
    // Browser will ignore certificate errors for chains matching one of the
    // public keys from the list.
    command_line->AppendSwitchASCII(
        network::switches::kIgnoreCertificateErrorsSPKIList, whitelist_flag);
  }
};

class SSLUITestIgnoreCertErrorsBySPKIWSS : public SSLUITest {
 public:
  SSLUITestIgnoreCertErrorsBySPKIWSS() : SSLUITest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SSLUITest::SetUpCommandLine(command_line);
    std::string whitelist_flag =
        MakeCertSPKIFingerprint(wss_server_expired_.GetCertificate().get());
    // Browser will ignore certificate errors for chains matching one of the
    // public keys from the list.
    command_line->AppendSwitchASCII(
        network::switches::kIgnoreCertificateErrorsSPKIList, whitelist_flag);
  }
};

class SSLUITestIgnoreLocalhostCertErrors : public SSLUITest {
 public:
  SSLUITestIgnoreLocalhostCertErrors() : SSLUITest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SSLUITest::SetUpCommandLine(command_line);
    // Browser will ignore certificate errors on localhost.
    command_line->AppendSwitch(switches::kAllowInsecureLocalhost);
  }
};

class SSLUITestWithExtendedReporting : public SSLUITest {
 public:
  SSLUITestWithExtendedReporting() : SSLUITest() {
    // CertReportHelper::ShouldReportCertificateError checks the value of this
    // variation. Ensure reporting is enabled.
    variations::testing::VariationParamsManager::SetVariationParams(
        "ReportCertificateErrors", "ShowAndPossiblySend",
        {{"sendingThreshold", "1.0"}});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SSLUITest::SetUpCommandLine(command_line);
  }
};

class SSLUITestHSTS : public SSLUITest {
 public:
  void SetUpOnMainThread() override {
    SSLUITest::SetUpOnMainThread();
    ssl_test_util::SetHSTSForHostName(browser()->profile(), kHstsTestHostName);
  }
};

class SSLUITestReduceSubresourceNotifications : public SSLUITestBase {
 public:
  SSLUITestReduceSubresourceNotifications() {
    scoped_feature_list_.InitWithFeatures(
        /* enabled_features */ {features::kReduceSubresourceResponseStartedIPC},
        /* disabled_features */ {blink::features::kMixedContentAutoupgrade});
  }

  SSLUITestReduceSubresourceNotifications(
      const SSLUITestReduceSubresourceNotifications&) = delete;
  SSLUITestReduceSubresourceNotifications& operator=(
      const SSLUITestReduceSubresourceNotifications&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Visits a regular page over http.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTP) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html")));

  ssl_test_util::CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
}

// Visits a page over http which includes broken https resources (status should
// be OK).
// TODO(jcampan): test that bad HTTPS content is blocked (otherwise we'll give
//                the secure cookies away!).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPWithBrokenHTTPSResource) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_unsafe_contents.html",
      https_server_expired_.host_port_pair());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(replacement_path)));

  ssl_test_util::CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
}

// Tests that after loading mixed content and then making a same-document
// navigation, the mixed content security indicator remains. See
// https://crbug.com/959571.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestMixedContentWithSamePageNavigation) {
  ASSERT_TRUE(https_server_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a secure page (no mixed content).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // Add a mixed form after a same-document navigation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html#foo")));
  ssl_test_util::SecurityStateWebContentsObserver observer(tab);
  ASSERT_NE(false, content::EvalJs(tab,
                                   "var f = document.createElement('form');"
                                   "f.action = 'http://foo.test';"
                                   "document.body.appendChild(f)"));
  observer.WaitForDidChangeVisibleSecurityState();

  // Since mixed forms trigger their own warning, we display the lock icon on
  // otherwise secure sites with an insecure form.
  security_state::SecurityLevel expected_level = security_state::SECURE;

  ssl_test_util::CheckSecurityState(
      tab, CertError::NONE, expected_level,
      AuthState::DISPLAYED_FORM_WITH_INSECURE_ACTION);

  // Go back (which should also be a same-document navigation) and test that the
  // security indicator is still downgraded because of the mixed form.
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WaitForLoadStop(tab));
  ssl_test_util::CheckSecurityState(
      tab, CertError::NONE, expected_level,
      AuthState::DISPLAYED_FORM_WITH_INSECURE_ACTION);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBrokenHTTPSWithInsecureContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL(replacement_path)));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID,
      AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Tests that the NavigationEntry gets marked as active mixed content,
// even if there is a certificate error. Regression test for
// https://crbug.com/593950.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestBrokenHTTPSWithActiveInsecureContent) {
  ASSERT_TRUE(https_server_expired_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Navigate to a page with a certificate error and click through the
  // interstitial.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/title1.html")));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);
  ProceedThroughInterstitial(tab);

  // Load an insecure (http://) script. When it is loaded, check that the page
  // is marked as having run insecure content.
  SSLContentStatusObserver observer(tab,
                                    content::SSLStatus::RAN_INSECURE_CONTENT);
  ASSERT_NE(false,
            content::EvalJs(tab,
                            "var s = document.createElement('script');"
                            "s.src = 'http://does-not-exist.test/foo.js';"
                            "document.body.appendChild(s)"));
  observer.WaitForSSLContentStatusFlag();

  // Now check that the page is marked as both having a cert error and having
  // run insecure content.
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::RAN_INSECURE_CONTENT);
}

// Tests that when a subframe commits a main resource with a certificate error,
// the navigation entry is marked as insecure.
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreCertErrors, SubframeHasCertError) {
  ASSERT_TRUE(https_server_mismatched_.Start());
  // Load a page with a data: favicon URL to suppress a favicon request. A
  // favicon request can cause the navigation entry to get marked as having run
  // insecure content (favicons are treated as active content), which would
  // interfere with the test expectation below.
  GURL main_frame_url =
      https_server_mismatched_.GetURL("a.test", "/data_favicon.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_FALSE(
      tab->GetController().GetLastCommittedEntry()->GetSSL().content_status &
      content::SSLStatus::RAN_CONTENT_WITH_CERT_ERRORS);

  GURL subframe_url =
      https_server_mismatched_.GetURL("b.test", "/data_favicon.html");
  content::TestNavigationObserver iframe_observer(tab);
  EXPECT_TRUE(content::ExecJs(
      tab, content::JsReplace("var i = document.createElement('iframe');"
                              "i.src = $1;"
                              "document.body.appendChild(i);",
                              subframe_url.spec())));
  iframe_observer.Wait();
  EXPECT_TRUE(
      tab->GetController().GetLastCommittedEntry()->GetSSL().content_status &
      content::SSLStatus::RAN_CONTENT_WITH_CERT_ERRORS);
}

namespace {

// A WebContentsObserver that allows the user to wait for a same-document
// navigation. Tests using this observer will fail if a non-same-document
// navigation completes after calling WaitForSameDocumentNavigation.
class SameDocumentNavigationObserver : public content::WebContentsObserver {
 public:
  explicit SameDocumentNavigationObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~SameDocumentNavigationObserver() override {}

  void WaitForSameDocumentNavigation() { run_loop_.Run(); }

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    ASSERT_TRUE(navigation_handle->IsSameDocument());
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
};

}  // namespace

// Tests that the mixed content flags are reset when going back to an existing
// navigation entry that had mixed content. Regression test for
// https://crbug.com/750649.
IN_PROC_BROWSER_TEST_F(SSLUITest, GoBackToMixedContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  // Navigate to a URL and dynamically load mixed content.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
  ssl_test_util::SecurityStateWebContentsObserver observer(tab);
  ASSERT_TRUE(content::ExecJs(tab,
                              "var i = document.createElement('img');"
                              "i.src = 'http://example.test';"
                              "document.body.appendChild(i);"));
  observer.WaitForDidChangeVisibleSecurityState();
  ssl_test_util::CheckSecurityState(tab, CertError::NONE,
                                    security_state::WARNING,
                                    AuthState::DISPLAYED_INSECURE_CONTENT);

  // Now navigate somewhere else, and then back to the page that dynamically
  // loaded mixed content.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html")));
  ssl_test_util::CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WaitForLoadStop(tab));
  // After going back, the mixed content indicator should no longer be present.
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Tests that the mixed content flags are not reset for an in-page navigation.
IN_PROC_BROWSER_TEST_F(SSLUITest, MixedContentWithSameDocumentNavigation) {
  ASSERT_TRUE(https_server_.Start());

  // Navigate to a URL and dynamically load mixed content.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
  ssl_test_util::SecurityStateWebContentsObserver security_state_observer(tab);
  ASSERT_TRUE(content::ExecJs(tab,
                              "var i = document.createElement('img');"
                              "i.src = 'http://example.test';"
                              "document.body.appendChild(i);"));
  security_state_observer.WaitForDidChangeVisibleSecurityState();
  ssl_test_util::CheckSecurityState(tab, CertError::NONE,
                                    security_state::WARNING,
                                    AuthState::DISPLAYED_INSECURE_CONTENT);

  // Initiate a same-document navigation and check that the page is still
  // marked as having displayed mixed content.
  SameDocumentNavigationObserver navigation_observer(tab);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html#foo")));
  navigation_observer.WaitForSameDocumentNavigation();
  ssl_test_util::CheckSecurityState(tab, CertError::NONE,
                                    security_state::WARNING,
                                    AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Tests that the WebContents's flag for displaying content with cert
// errors get cleared upon navigation.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       DisplayedContentWithCertErrorsClearedOnNavigation) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Navigate to a page with a certificate error and click through the
  // interstitial.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/title1.html")));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);
  ProceedThroughInterstitial(tab);

  // Add a subresource with a certificate error and check that it's recorded
  // correctly. We wait specifically for the DidDisplayContentWithCertErrors
  // event (rather than a DidChangeVisibleSecurityState event) because the
  // page's favicon is loaded as active content and the notification about that
  // can interfere with the visible security state change that we're observing
  // here.
  SSLContentStatusObserver observer(
      tab, content::SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS);
  ASSERT_NE(false, content::EvalJs(tab,
                                   "var i = document.createElement('img');"
                                   "i.src = 'ssl/google_files/logo.gif';"
                                   "document.body.appendChild(i)"));
  observer.WaitForSSLContentStatusFlag();

  // Navigate away to a different page, and check that the flag gets cleared.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_FALSE(entry->GetSSL().content_status &
               content::SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBrokenHTTPSMetricsReporting_Proceed) {
  ASSERT_TRUE(https_server_expired_.Start());
  base::HistogramTester histograms;
  const std::string decision_histogram =
      "interstitial.ssl_overridable.decision";
  const std::string interaction_histogram =
      "interstitial.ssl_overridable.interaction";

  // Histograms should start off empty.
  histograms.ExpectTotalCount(decision_histogram, 0);
  histograms.ExpectTotalCount(interaction_histogram, 0);

  // After navigating to the page, the totals should be set.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/title1.html")));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
  histograms.ExpectTotalCount(decision_histogram, 1);
  histograms.ExpectBucketCount(decision_histogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectTotalCount(interaction_histogram, 2);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::SHOW_ENHANCED_PROTECTION, 1);

  // Decision should be recorded.
  SendInterstitialCommand(browser()->tab_strip_model()->GetActiveWebContents(),
                          security_interstitials::CMD_PROCEED);
  histograms.ExpectTotalCount(decision_histogram, 2);
  histograms.ExpectBucketCount(
      decision_histogram, security_interstitials::MetricsHelper::PROCEED, 1);
  histograms.ExpectTotalCount(interaction_histogram, 2);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBrokenHTTPSMetricsReporting_DontProceed) {
  ASSERT_TRUE(https_server_expired_.Start());
  base::HistogramTester histograms;
  const std::string decision_histogram =
      "interstitial.ssl_overridable.decision";
  const std::string interaction_histogram =
      "interstitial.ssl_overridable.interaction";

  // Histograms should start off empty.
  histograms.ExpectTotalCount(decision_histogram, 0);
  histograms.ExpectTotalCount(interaction_histogram, 0);

  // After navigating to the page, the totals should be set.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/title1.html")));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
  histograms.ExpectTotalCount(decision_histogram, 1);
  histograms.ExpectBucketCount(decision_histogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectTotalCount(interaction_histogram, 2);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::SHOW_ENHANCED_PROTECTION, 1);

  // Decision should be recorded.
  SendInterstitialCommand(browser()->tab_strip_model()->GetActiveWebContents(),
                          security_interstitials::CMD_DONT_PROCEED);
  histograms.ExpectTotalCount(decision_histogram, 2);
  histograms.ExpectBucketCount(
      decision_histogram, security_interstitials::MetricsHelper::DONT_PROCEED,
      1);
  histograms.ExpectTotalCount(interaction_histogram, 2);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
}

// Visits a page over OK https:
IN_PROC_BROWSER_TEST_F(SSLUITest, TestOKHTTPS) {
  ASSERT_TRUE(https_server_.Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));

  ssl_test_util::CheckAuthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
}

// Visits a page with https error and proceed:
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSExpiredCertAndProceed) {
  ASSERT_TRUE(https_server_expired_.Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
  EXPECT_EQ(https_server_expired_.GetURL("/ssl/google.html"),
            tab->GetVisibleURL());
}

// Visits a page with https error and checks favicon is not displayed:
IN_PROC_BROWSER_TEST_F(SSLUITest, TestNoFaviconOnInterstitial) {
  ASSERT_TRUE(https_server_expired_.Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));
  EXPECT_FALSE(
      browser()->tab_strip_model()->delegate()->ShouldDisplayFavicon(tab));
}

class SSLUITestWithWebApps : public SSLUITest {
 public:
  Browser* InstallAndOpenTestWebApp(const GURL& start_url) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->title = u"Test app";
    web_app_info->description = u"Test description";

    Profile* profile = browser()->profile();

    webapps::AppId app_id =
        web_app::test::InstallWebApp(profile, std::move(web_app_info));

    Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(profile, app_id);
    return app_browser;
  }

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

// Visits a page in an app window with https error and proceed:
// Disabled due to flaky failures; see https://crbug.com/1156046.
IN_PROC_BROWSER_TEST_F(SSLUITestWithWebApps,
                       DISABLED_InAppTestHTTPSExpiredCertAndProceed) {
  ASSERT_TRUE(https_server_expired_.Start());

  const GURL app_url = https_server_expired_.GetURL("/ssl/google.html");
  Browser* app_browser = InstallAndOpenTestWebApp(app_url);

  WebContents* app_tab = app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(app_tab));
  ssl_test_util::CheckAuthenticationBrokenState(
      app_tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitialInAppAndCheckNewTabOpened(app_browser, app_url);
}

// Visits a page with https error and proceed. Then open the app and proceed.
IN_PROC_BROWSER_TEST_F(SSLUITestWithWebApps,
                       InAppTestHTTPSExpiredCertAndPreviouslyProceeded) {
  ASSERT_TRUE(https_server_expired_.Start());

  const GURL app_url = https_server_expired_.GetURL("/ssl/google.html");

  // Go through the interstitial in a regular browser tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(initial_tab));
  ssl_test_util::CheckAuthenticationBrokenState(
      initial_tab, net::CERT_STATUS_DATE_INVALID,
      AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(initial_tab);
  ssl_test_util::CheckAuthenticationBrokenState(
      initial_tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);

  Browser* app_browser = InstallAndOpenTestWebApp(app_url);

  // Apps are not allowed to have SSL errors, so the interstitial should be
  // showing even though the user proceeded through it in a regular tab.
  WebContents* app_tab = app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(app_tab));
  // TODO(crbug.com/40735115): Apps are not setting the right security state in
  // this case, so we only check the presence of the interstitial (inside Wait
  // ForInterstitial) and the behavior after clicking through.
  // After the bug is fixed, add a call to CheckAuthenticationBrokenState
  // with net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL
  // parameters.

  ProceedThroughInterstitialInAppAndCheckNewTabOpened(app_browser, app_url);
}

// Visits a page with https error and don't proceed (and ensure we can still
// navigate at that point):
IN_PROC_BROWSER_TEST_F(SSLUITest, TestInterstitialCrossSiteNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_mismatched_.Start());

  // First navigate to an OK page.
  GURL initial_url = https_server_.GetURL("/ssl/google.html");
  ASSERT_EQ("127.0.0.1", initial_url.host());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate from 127.0.0.1 to localhost so it triggers a
  // cross-site navigation to make sure http://crbug.com/5800 is gone.
  GURL cross_site_url = https_server_mismatched_.GetURL("/ssl/google.html");
  ASSERT_EQ("localhost", cross_site_url.host());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), cross_site_url));
  // An SSL interstitial should be showing.
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID,
      AuthState::SHOWING_INTERSTITIAL);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));

  // Simulate user clicking "Take me back".
  DontProceedThroughInterstitial(tab);

  // We should be back to the original good page.
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // Navigate to a new page to make sure bug 5800 is fixed.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html")));
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Test that localhost pages don't show an interstitial.
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreLocalhostCertErrors,
                       TestNoInterstitialOnLocalhost) {
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a localhost page.
  GURL url = https_server_.GetURL("/ssl/page_with_subresource.html");
  GURL::Replacements replacements;
  std::string new_host("localhost");
  replacements.SetHostStr(new_host);
  url = url.ReplaceComponents(replacements);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // We should see no interstitial, but we should have an error
  // (red-crossed-out-https) in the URL bar.
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID, AuthState::NONE);

  // We should see that the script tag in the page loaded and ran (and
  // wasn't blocked by the certificate error).
  std::u16string title;
  std::u16string expected_title = u"This script has loaded";
  ui_test_utils::GetCurrentTabTitle(browser(), &title);
  EXPECT_EQ(title, expected_title);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSErrorCausedByClockUsingBuildTime) {
  ASSERT_TRUE(https_server_expired_.Start());

  // Set up the build and current clock times to be more than a year apart.
  std::unique_ptr<base::SimpleTestClock> mock_clock(
      new base::SimpleTestClock());
  mock_clock->SetNow(base::Time::NowFromSystemTime());
  mock_clock->Advance(base::Days(367));
  SSLErrorHandler::SetClockForTesting(mock_clock.get());
  ssl_errors::SetBuildTimeForTesting(base::Time::NowFromSystemTime());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/title1.html")));
  WebContents* clock_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingBadClockInterstitial(clock_tab));
  ssl_test_util::CheckSecurityState(clock_tab, net::CERT_STATUS_DATE_INVALID,
                                    security_state::DANGEROUS,
                                    AuthState::SHOWING_INTERSTITIAL);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSErrorCausedByClockUsingNetwork) {
  ASSERT_TRUE(https_server_expired_.Start());

  // Set network forward ten minutes, which is sufficient to trigger
  // the interstitial.
  g_browser_process->network_time_tracker()->UpdateNetworkTime(
      base::Time::Now() + base::Minutes(10),
      base::Milliseconds(1),   /* resolution */
      base::Milliseconds(500), /* latency */
      base::TimeTicks::Now() /* posting time of this update */);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/title1.html")));
  WebContents* clock_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingBadClockInterstitial(clock_tab));
  ssl_test_util::CheckSecurityState(clock_tab, net::CERT_STATUS_DATE_INVALID,
                                    security_state::DANGEROUS,
                                    AuthState::SHOWING_INTERSTITIAL);
}

// Visits a page with https error and then goes back using Browser::GoBack.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSExpiredCertAndGoBackViaButton) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to an HTTP page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html")));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Now go to a bad HTTPS page that shows an interstitial.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Simulate user clicking on back button (crbug.com/39248).
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // We should be back at the original good page.
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(tab));
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Visits a page with https error and then goes back using GoToOffset.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSExpiredCertAndGoBackViaMenu) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to an HTTP page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html")));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Now go to a bad HTTPS page that shows an interstitial.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Simulate user clicking and holding on back button (crbug.com/37215). With
  // committed interstitials enabled, this triggers a navigation.
  content::TestNavigationObserver nav_observer(tab);
  tab->GetController().GoToOffset(-1);
  nav_observer.Wait();

  // We should be back at the original good page.
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(tab));
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Visits a page with https error and then goes back using the DONT_PROCEED
// interstitial command.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSExpiredCertGoBackUsingCommand) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to an HTTP page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html")));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Now go to a bad HTTPS page that shows an interstitial.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  content::LoadStopObserver observer(tab);
  SendInterstitialCommand(tab, security_interstitials::CMD_DONT_PROCEED);
  observer.Wait();

  // We should be back at the original good page.
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Visits a page that uses a SHA-1 leaf certificate, which should be rejected
// by default.
IN_PROC_BROWSER_TEST_F(SSLUITest, SHA1IsDefaultDisabled) {
  EXPECT_FALSE(last_ssl_config_.sha1_local_anchors_enabled);
  EXPECT_FALSE(CreateDefaultNetworkContextParams()
                   ->initial_ssl_config->sha1_local_anchors_enabled);

  ASSERT_TRUE(https_server_sha1_.Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_sha1_.GetURL("/ssl/google.html")));

  ssl_test_util::CheckAuthenticationBrokenState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM,
      AuthState::SHOWING_INTERSTITIAL);
}

// By default, trust in Symantec's Legacy PKI should be disabled. Unfortunately,
// there is currently no way to simulate navigation to a page that will
// meaningfully test that Symantec enforcement is actually applied to the
// request.
IN_PROC_BROWSER_TEST_F(SSLUITest, SymantecEnforcementIsNotDisabled) {
  EXPECT_FALSE(last_ssl_config_.symantec_enforcement_disabled);
  EXPECT_FALSE(CreateDefaultNetworkContextParams()
                   ->initial_ssl_config->symantec_enforcement_disabled);
}

// Visit a HTTP page which request WSS connection to a server providing invalid
// certificate. Close the page while WSS connection waits for SSLManager's
// response from UI thread.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestWSSInvalidCertAndClose) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(wss_server_expired_.Start());

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, u"PASS");
  watcher.AlsoWaitForTitle(u"FAIL");

  // Create GURLs to test pages.
  std::string wss_close_url_path = base::StringPrintf(
      "%s?%d",
      embedded_test_server()->GetURL("/ssl/wss_close.html").spec().c_str(),
      wss_server_expired_.host_port_pair().port());
  GURL wss_close_url(wss_close_url_path);
  std::string wss_loop_url_path = base::StringPrintf(
      "%s?%d",
      embedded_test_server()->GetURL("/ssl/wss_close_loop.html").spec().c_str(),
      wss_server_expired_.host_port_pair().port());
  GURL wss_loop_url(wss_loop_url_path);

  // Create tabs and visit pages which keep on creating wss connections.
  WebContents* tabs[16];
  for (int i = 0; i < 16; ++i) {
    tabs[i] = chrome::AddSelectedTabWithURL(browser(), wss_loop_url,
                                            ui::PAGE_TRANSITION_LINK);
  }
  chrome::SelectNextTab(browser());

  // Visit a page which waits for one TLS handshake failure.
  // The title will be changed to 'PASS'.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), wss_close_url));
  const std::u16string result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::EqualsCaseInsensitiveASCII(result, "pass"));

  // Close tabs which contains the test page.
  for (int i = 0; i < 16; ++i)
    chrome::CloseWebContents(browser(), tabs[i], false);
  chrome::CloseWebContents(browser(), tab, false);
}

// Visit a HTTPS page and proceeds despite an invalid certificate. The page
// requests WSS connection to the same origin host to check if WSS connection
// share certificates policy with HTTPS correcly.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestWSSInvalidCert) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(wss_server_expired_.Start());

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, u"PASS");
  watcher.AlsoWaitForTitle(u"FAIL");

  // Visit bad HTTPS page.
  GURL::Replacements replacements;
  replacements.SetSchemeStr("https");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), wss_server_expired_.GetURL("connect_check.html")
                     .ReplaceComponents(replacements)));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Proceed anyway.
  ProceedThroughInterstitial(tab);

  // Test page run a WebSocket wss connection test. The result will be shown
  // as page title.
  const std::u16string result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::EqualsCaseInsensitiveASCII(result, "pass"));
}

// Data URLs should always be marked as non-secure.
IN_PROC_BROWSER_TEST_F(SSLUITest, MarkDataAsNonSecure) {
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("data:text/plain,hello")));
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());
}

// TODO(crbug.com/40156980): This class directly calls
// `UnsafelyGetNSSCertDatabaseForTesting()` that causes crash at the moment
// and is never called from Lacros-Chrome. This should be revisited when there
// is a solution for the client certificates settings page on Lacros-Chrome.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(USE_NSS_CERTS)
class SSLUITestWithClientCert : public SSLUITestBase {
 public:
  SSLUITestWithClientCert() : cert_db_(nullptr) {}

  void SetUpOnMainThread() override {
    SSLUITestBase::SetUpOnMainThread();

    base::RunLoop loop;
    NssServiceFactory::GetForContext(browser()->profile())
        ->UnsafelyGetNSSCertDatabaseForTesting(
            base::BindOnce(&SSLUITestWithClientCert::DidGetCertDatabase,
                           base::Unretained(this), &loop));
    loop.Run();
  }

 protected:
  void DidGetCertDatabase(base::RunLoop* loop, net::NSSCertDatabase* cert_db) {
    cert_db_ = cert_db;
    loop->Quit();
  }

  raw_ptr<net::NSSCertDatabase> cert_db_;
};

// SSL client certificate tests are only enabled when using NSS for private key
// storage, as only NSS can avoid modifying global machine state when testing.
// See http://crbug.com/51132

// Visit a HTTPS page which requires client cert authentication. The client
// cert will be selected automatically, then a test which uses WebSocket runs.
//
// TODO(crbug.com/40811167): disabled because of race in when certs
// are incorporated.
IN_PROC_BROWSER_TEST_F(SSLUITestWithClientCert, DISABLED_TestWSSClientCert) {
  // Import a client cert for test.
  crypto::ScopedPK11Slot public_slot = cert_db_->GetPublicSlot();
  std::string pkcs12_data;
  base::FilePath cert_path = net::GetTestCertsDirectory().Append(
      FILE_PATH_LITERAL("websocket_client_cert.p12"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::ReadFileToString(cert_path, &pkcs12_data));
  }
  EXPECT_EQ(net::OK,
            cert_db_->ImportFromPKCS12(public_slot.get(), pkcs12_data,
                                       std::u16string(), true, nullptr));

  // Start WebSocket test server with TLS and client cert authentication.
  net::SpawnedTestServer::SSLOptions options(
      net::SpawnedTestServer::SSLOptions::CERT_OK);
  options.request_client_certificate = true;
  base::FilePath ca_path = net::GetTestCertsDirectory().Append(
      FILE_PATH_LITERAL("websocket_cacert.pem"));
  options.client_authorities.push_back(ca_path);
  net::SpawnedTestServer wss_server(net::SpawnedTestServer::TYPE_WSS, options,
                                    net::GetWebSocketTestDataDirectory());
  ASSERT_TRUE(wss_server.Start());
  GURL::Replacements replacements;
  replacements.SetSchemeStr("https");
  GURL url =
      wss_server.GetURL("connect_check.html").ReplaceComponents(replacements);

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, u"PASS");
  watcher.AlsoWaitForTitle(u"FAIL");

  // Add an entry into AutoSelectCertificateForUrls policy for automatic client
  // cert selection.
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  DCHECK(profile);
  base::Value::Dict filter;
  filter.SetByDottedPath("ISSUER.CN", "pywebsocket");
  base::Value::List filters;
  filters.Append(std::move(filter));
  base::Value::Dict setting;
  setting.Set("filters", std::move(filters));
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetWebsiteSettingDefaultScope(
          url, GURL(), ContentSettingsType::AUTO_SELECT_CERTIFICATE,
          base::Value(std::move(setting)));

  // Visit a HTTPS page which requires client certs.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // Test page runs a WebSocket wss connection test. The result will be shown
  // as page title.
  const std::u16string result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::EqualsCaseInsensitiveASCII(result, "pass"));
}
#endif  // BUILDFLAG(USE_NSS_CERTS)
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// A stub ClientCertStore that returns a FakeClientCertIdentity.
class ClientCertStoreStub : public net::ClientCertStore {
 public:
  explicit ClientCertStoreStub(net::ClientCertIdentityList list)
      : list_(std::move(list)) {}

  ~ClientCertStoreStub() override {}

  // net::ClientCertStore:
  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override {
    std::move(callback).Run(std::move(list_));
  }

 private:
  net::ClientCertIdentityList list_;
};

std::unique_ptr<net::ClientCertStore> CreateCertStore() {
  base::FilePath certs_dir = net::GetTestCertsDirectory();

  net::ClientCertIdentityList cert_identity_list;

  {
    base::ScopedAllowBlockingForTesting allow_blocking;

    std::unique_ptr<net::FakeClientCertIdentity> cert_identity =
        net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
            certs_dir, "client_1.pem", "client_1.pk8");
    EXPECT_TRUE(cert_identity.get());
    if (cert_identity)
      cert_identity_list.push_back(std::move(cert_identity));
  }

  return std::unique_ptr<net::ClientCertStore>(
      new ClientCertStoreStub(std::move(cert_identity_list)));
}

std::unique_ptr<net::ClientCertStore> CreateFailSigningCertStore() {
  base::FilePath certs_dir = net::GetTestCertsDirectory();

  net::ClientCertIdentityList cert_identity_list;

  {
    base::ScopedAllowBlockingForTesting allow_blocking;

    std::unique_ptr<net::FakeClientCertIdentity> cert_identity =
        net::FakeClientCertIdentity::CreateFromCertAndFailSigning(
            certs_dir, "client_1.pem");
    EXPECT_TRUE(cert_identity.get());
    if (cert_identity)
      cert_identity_list.push_back(std::move(cert_identity));
  }

  return std::unique_ptr<net::ClientCertStore>(
      new ClientCertStoreStub(std::move(cert_identity_list)));
}

std::unique_ptr<net::ClientCertStore> CreateEmptyCertStore() {
  return std::unique_ptr<net::ClientCertStore>(new ClientCertStoreStub({}));
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBrowserUseClientCertStore) {
  // Make the browser use the ClientCertStoreStub instead of the regular one.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->set_client_cert_store_factory_for_testing(
          base::BindRepeating(&CreateCertStore));

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL https_url =
      https_server.GetURL("/ssl/browser_use_client_cert_store.html");

  // Add an entry into AutoSelectCertificateForUrls policy for automatic client
  // cert selection.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  DCHECK(profile);
  base::Value::List filters;
  filters.Append(base::Value::Dict());
  base::Value::Dict setting;
  setting.Set("filters", std::move(filters));
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetWebsiteSettingDefaultScope(
          https_url, GURL(), ContentSettingsType::AUTO_SELECT_CERTIFICATE,
          base::Value(std::move(setting)));

  // Visit a HTTPS page which requires client certs.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                            https_url, 1);
  EXPECT_EQ("pass", tab->GetLastCommittedURL().ref());
}

// Tests that requests from service workers can also use certificates
// auto-selected by policy.
// https://crbug.com/1417601.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestServiceWorkerRequestsUseClientCertStore) {
  // Make the browser use the ClientCertStoreStub instead of the regular one.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->set_client_cert_store_factory_for_testing(
          base::BindRepeating(&CreateCertStore));

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  {
    net::SSLServerConfig ssl_config;
    ssl_config.client_cert_type =
        net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
    https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES,
                              ssl_config);
  }
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");

  ASSERT_TRUE(https_server.Start());

  // We set up a page that installs a service worker to perform a fetch to
  // a separate, cross-origin resource. We need this to be a cross-origin
  // because visiting the first site (to install the service worker) requires
  // a cert to be present, so subsequent fetches to that site will succeed
  // without a separate certificate prompt.

  // Note: These domain names need to match those in
  // //net/data/ssl/certificates/test_names.pem.
  GURL requestor_url =
      https_server.GetURL("a.test", "/ssl/service_worker_fetch/page.html");
  GURL target_url =
      https_server.GetURL("b.test", "/ssl/service_worker_fetch/target.txt");

  // Add an entry into AutoSelectCertificateForUrls policy for automatic client
  // cert selection for both the requestor and target URLs.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  DCHECK(profile);
  base::Value::List filters;
  filters.Append(base::Value::Dict());
  base::Value::Dict setting;
  setting.Set("filters", std::move(filters));
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetWebsiteSettingDefaultScope(
          requestor_url, GURL(), ContentSettingsType::AUTO_SELECT_CERTIFICATE,
          base::Value(setting.Clone()));
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetWebsiteSettingDefaultScope(
          target_url, GURL(), ContentSettingsType::AUTO_SELECT_CERTIFICATE,
          base::Value(std::move(setting)));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), requestor_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Check the navigation succeeded. The title check verifies we didn't e.g.
  // get a privacy error.
  EXPECT_EQ(requestor_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(u"My Title", web_contents->GetTitle());

  // Perform a fetch from a worker and validate that it succeeds.
  EXPECT_EQ(
      "text content\n",
      content::EvalJs(web_contents,
                      content::JsReplace("doFetchInWorker($1);", target_url)));
}

// Tests that if an extension service worker requests a resource where a
// client cert is optional (not required) and there are no client certs, the
// request will continue without a certificate (as opposed to abort).
#if BUILDFLAG(ENABLE_EXTENSIONS) && !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(
    SSLUITest,
    TestExtensionServiceWorkerCanContinueWithoutACertificate) {
  // Make the browser use the ClientCertStoreStub instead of the regular one.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->set_client_cert_store_factory_for_testing(
          base::BindRepeating(&CreateEmptyCertStore));

  // Set up an HTTPS server with optional client certs.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  {
    net::SSLServerConfig ssl_config;
    ssl_config.client_cert_type =
        net::SSLServerConfig::ClientCertType::OPTIONAL_CLIENT_CERT;
    https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES,
                              ssl_config);
  }
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());

  // Load a test extension that will try to fetch the cross-origin resource.
  static constexpr char kManifest[] =
      R"({
           "name": "Fetching Extension",
           "manifest_version": 2,
           "version": "0.1",
           "background": {"service_worker": "background.js"}
         })";
  static constexpr char kBackgroundJs[] =
      R"(async function doFetchAndReply(url) {
           try {
             let response = await fetch(url);
             result = await response.text();
           } catch (e) {
             result = `Fetch error: ${e.toString()}`;
           }

           chrome.test.sendScriptResult(result);
         })";

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);

  Profile* const profile = browser()->profile();
  extensions::ChromeTestExtensionLoader extension_loader(profile);
  scoped_refptr<const extensions::Extension> extension =
      extension_loader.LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Path to the cross-origin resource to fetch.
  // Note: This domain name matches one in
  // //net/data/ssl/certificates/test_names.pem.
  GURL target_url =
      https_server.GetURL("b.test", "/ssl/service_worker_fetch/target.txt");

  // Try to fetch the resource from the extension. We have no client certs
  // (we're using an empty cert store), so no certificates will be selected.
  // Even so, the fetch should succeed. It continues without a certificate, and
  // the certificate is optional.
  base::Value fetch_result =
      extensions::BackgroundScriptExecutor::ExecuteScript(
          profile, extension->id(),
          base::StringPrintf("doFetchAndReply('%s');",
                             target_url.spec().c_str()),
          extensions::BackgroundScriptExecutor::ResultCapture::
              kSendScriptResult);

  EXPECT_EQ(fetch_result, "text content\n");
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

IN_PROC_BROWSER_TEST_F(SSLUITest, TestClientAuthSigningFails) {
  // Make the browser use the ClientCertStoreStub instead of the regular one.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->set_client_cert_store_factory_for_testing(
          base::BindRepeating(&CreateFailSigningCertStore));

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL https_url =
      https_server.GetURL("/ssl/browser_use_client_cert_store.html");

  // Add an entry into AutoSelectCertificateForUrls policy for automatic client
  // cert selection.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  DCHECK(profile);
  base::Value::List filters;
  filters.Append(base::Value::Dict());
  base::Value::Dict setting;
  setting.Set("filters", std::move(filters));
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetWebsiteSettingDefaultScope(
          https_url, GURL(), ContentSettingsType::AUTO_SELECT_CERTIFICATE,
          base::Value(std::move(setting)));

  // Visit a HTTPS page which requires client certs.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                            https_url, 1);
  // Page should not load successfully.
  EXPECT_EQ("", tab->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestClientAuthContinueWithoutCert) {
  // Make the browser use a ClientCertStoreStub that returns no certs.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->set_client_cert_store_factory_for_testing(
          base::BindRepeating(&CreateEmptyCertStore));

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL https_url =
      https_server.GetURL("/ssl/browser_use_client_cert_store.html");

  // Visit a HTTPS page which requires client certs.
  // The browser should automatically continue to the site without a client
  // cert, since the ClientCertStore returns no certs.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                            https_url, 1);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  // Page should not load successfully.
  EXPECT_EQ("", tab->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestCertDBChangedFlushesClientAuthCache) {
  // Make the browser use the ClientCertStoreStub instead of the regular one.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->set_client_cert_store_factory_for_testing(
          base::BindRepeating(&CreateCertStore));

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL https_url =
      https_server.GetURL("/ssl/browser_use_client_cert_store.html");

  // Add an entry into AutoSelectCertificateForUrls policy for automatic client
  // cert selection.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  DCHECK(profile);
  base::Value::List filters;
  filters.Append(base::Value::Dict());
  base::Value::Dict setting;
  setting.Set("filters", std::move(filters));
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetWebsiteSettingDefaultScope(
          https_url, GURL(), ContentSettingsType::AUTO_SELECT_CERTIFICATE,
          base::Value(std::move(setting)));

  // Visit a HTTPS page which requires client certs.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                            https_url, 1);
  EXPECT_EQ("pass", tab->GetLastCommittedURL().ref());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("about:blank"), 1);
  EXPECT_EQ("", tab->GetLastCommittedURL().ref());

  // Now use a ClientCertStoreStub that always returns an empty list.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->set_client_cert_store_factory_for_testing(
          base::BindRepeating(&CreateEmptyCertStore));

  // Visiting the page which requires client certs should still work (either
  // due to the socket still being open, or due to the SSL client auth cache).
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                            https_url, 1);
  EXPECT_EQ("pass", tab->GetLastCommittedURL().ref());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("about:blank"), 1);
  EXPECT_EQ("", tab->GetLastCommittedURL().ref());

  // Send an OnClientCertStoreChanged notification.
  net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
  content::FlushNetworkServiceInstanceForTesting();

  // Visiting the page which requires client certs should fail, as the socket
  // pool has been flushed and SSL client auth cache has been cleared due to
  // the CertDBChanged observer.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                            https_url, 1);
  EXPECT_EQ("", tab->GetLastCommittedURL().ref());
}

// Open a page with a HTTPS error in a tab with no prior navigation (through a
// link with a blank target).  This is to test that the lack of navigation entry
// does not cause any problems (it was causing a crasher, see
// http://crbug.com/19941).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSErrorWithNoNavEntry) {
  ASSERT_TRUE(https_server_expired_.Start());

  const GURL url = https_server_expired_.GetURL("/ssl/google.htm");
  WebContents* tab2 =
      chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);
  content::WaitForLoadStop(tab2);

  // Verify our assumption that there was no prior navigation.
  EXPECT_FALSE(chrome::CanGoBack(browser()));

  // We should have an interstitial page showing.
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab2));
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBadHTTPSDownload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());
  GURL url_non_dangerous = embedded_test_server()->GetURL("/title1.html");
  GURL url_dangerous =
      https_server_expired_.GetURL("/downloads/dangerous/dangerous.exe");

  // Visit a non-dangerous page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_non_dangerous));

  // Now, start a transition to dangerous download.
  {
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    content::LoadStopObserver observer(tab);
    NavigateParams navigate_params(browser(), url_dangerous,
                                   ui::PAGE_TRANSITION_TYPED);
    Navigate(&navigate_params);
    observer.Wait();
  }

  // Proceed through the SSL interstitial. This doesn't use
  // ProceedThroughInterstitial() since no page load will commit.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  {
    // Wait for the download to complete after proceeding with the download.
    // This serves to verify the download was initiated, and to let the
    // test successfully shut down and cleanup. Exiting the browser with a
    // download still in-progress can lead to test failues.
    content::DownloadTestObserverTerminal dangerous_download_observer(
        browser()->profile()->GetDownloadManager(), 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT);
    SendInterstitialCommand(tab, security_interstitials::CMD_PROCEED);
    dangerous_download_observer.WaitForFinished();
  }

  // There should still be an interstitial at this point. Press the
  // back button on the browser. Note that this doesn't wait for a
  // NAV_ENTRY_COMMITTED notification because going back with an
  // active interstitial simply hides the interstitial.
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  EXPECT_TRUE(chrome::CanGoBack(browser()));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
}

//
// Insecure content
//

// Visits a page that displays insecure content.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysInsecureContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair());

  // Load a page that displays insecure content.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));

  ssl_test_util::CheckSecurityState(
      browser()->tab_strip_model()->GetActiveWebContents(), CertError::NONE,
      security_state::WARNING, AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Visits a page that displays an insecure form.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysInsecureForm) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_form.html",
      embedded_test_server()->host_port_pair());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));

  // Since mixed forms trigger their own warning, we display the lock icon on
  // otherwise secure sites with an insecure form.
  security_state::SecurityLevel expected_level = security_state::SECURE;

  ssl_test_util::CheckSecurityState(
      browser()->tab_strip_model()->GetActiveWebContents(), CertError::NONE,
      expected_level, AuthState::DISPLAYED_FORM_WITH_INSECURE_ACTION);
}

// Verifies that an SSL interstitial generates SafeBrowsing extension api
// events.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestExtensionEvents) {
  class ExtensionEventObserver : public extensions::EventRouter::TestObserver {
   public:
    ExtensionEventObserver() = default;

    ExtensionEventObserver(const ExtensionEventObserver&) = delete;
    ExtensionEventObserver& operator=(const ExtensionEventObserver&) = delete;

    ~ExtensionEventObserver() override = default;

    // extensions::EventRouter::TestObserver:
    void OnWillDispatchEvent(const extensions::Event& event) override {
      event_names_.push_back(event.event_name);
    }

    void OnDidDispatchEventToProcess(const extensions::Event& event,
                                     int process_id) override {}

    const std::vector<std::string>& event_names() const { return event_names_; }

   private:
    std::vector<std::string> event_names_;
  };

  ExtensionEventObserver observer;
  extensions::EventRouter::Get(browser()->profile())
      ->AddObserverForTesting(&observer);

  ASSERT_TRUE(https_server_expired_.Start());

  GURL request_url = https_server_expired_.GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), request_url));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab != nullptr);
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Verifies that security interstitial shown event is observed.
  EXPECT_TRUE(base::Contains(observer.event_names(),
                             extensions::api::safe_browsing_private::
                                 OnSecurityInterstitialShown::kEventName));

  ProceedThroughInterstitial(tab);

  // Verifies that security interstitial proceeded event is observed.
  EXPECT_TRUE(base::Contains(observer.event_names(),
                             extensions::api::safe_browsing_private::
                                 OnSecurityInterstitialProceeded::kEventName));

  extensions::EventRouter::Get(browser()->profile())
      ->RemoveObserverForTesting(&observer);
}

// Visits a page that runs insecure content and tries to suppress the insecure
// content warnings by randomizing location.hash.
// Based on http://crbug.com/8706
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRunsInsecuredContentRandomizeHash) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/page_runs_insecure_content.html")));

  ssl_test_util::CheckAuthenticationBrokenState(
      browser()->tab_strip_model()->GetActiveWebContents(), CertError::NONE,
      AuthState::RAN_INSECURE_CONTENT);
}

// Visits an SSL page twice, once with subresources served over good SSL and
// once over bad SSL.
// - For the good SSL case, the iframe and images should be properly displayed.
// - For the bad SSL case, the iframe contents shouldn't be displayed and images
//   and scripts should be filtered out entirely.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeContents) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());
  // Enable popups without user gesture.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::POPUPS,
                                 CONTENT_SETTING_ALLOW);
  {
    // First visit the page with its iframe and subresources served over good
    // SSL. This is a sanity check to make sure these resources aren't already
    // broken in the good case.
    std::string replacement_path = GetFilePathWithHostAndPortReplacement(
        "/ssl/page_with_unsafe_contents.html", https_server_.host_port_pair());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL(replacement_path)));
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    // The state is expected to be authenticated.
    ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
    // The iframe should be able to open a popup.
    EXPECT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
    // In order to check that the image was loaded, check its width.
    // The actual image (Google logo) is 276 pixels wide.
    EXPECT_EQ(276, content::EvalJs(tab, "ImageWidth();"));
    // Check that variable |foo| is set.
    EXPECT_EQ(true, content::EvalJs(tab, "IsFooSet();"));
  }
  {
    // Now visit the page with its iframe and subresources served over bad
    // SSL. Iframes, images, and scripts should all be blocked.
    std::string replacement_path = GetFilePathWithHostAndPortReplacement(
        "/ssl/page_with_unsafe_contents.html",
        https_server_expired_.host_port_pair());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL(replacement_path)));
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    // When the bad content is filtered, the state is expected to be
    // authenticated.
    ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
    // The iframe attempts to open a popup window, but it shouldn't be able to.
    // Previous popup is still open.
    EXPECT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
    // The broken image width is zero.
    EXPECT_EQ(16, content::EvalJs(tab, "ImageWidth();"));
    // Check that variable |foo| is not set.
    EXPECT_EQ(false, content::EvalJs(tab, "IsFooSet();"));
  }
}

// Visits a page with insecure content loaded by JS (after the initial page
// load).
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// flaky http://crbug.com/396462
#define MAYBE_TestDisplaysInsecureContentLoadedFromJS \
  DISABLED_TestDisplaysInsecureContentLoadedFromJS
#else
#define MAYBE_TestDisplaysInsecureContentLoadedFromJS \
  TestDisplaysInsecureContentLoadedFromJS
#endif
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       MAYBE_TestDisplaysInsecureContentLoadedFromJS) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  net::HostPortPair replacement_pair = embedded_test_server()->host_port_pair();
  replacement_pair.set_host("example.test");

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html", replacement_pair);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // Load the insecure image.
  EXPECT_EQ(true, content::EvalJs(tab, "loadBadImage();"));

  // We should now have insecure content.
  ssl_test_util::CheckSecurityState(tab, CertError::NONE,
                                    security_state::WARNING,
                                    AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Visits two pages from the same origin: one that displays insecure content and
// one that doesn't.  The test checks that we do not propagate the insecure
// content state from one to the other.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysInsecureContentTwoTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/blank_page.html")));

  WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();

  // This tab should be fine.
  ssl_test_util::CheckAuthenticatedState(tab1, AuthState::NONE);

  // Create a new tab.
  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair());

  GURL url = https_server_.GetURL(replacement_path);
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.tabstrip_index = 0;
  params.source_contents = tab1;
  Navigate(&params);
  WebContents* tab2 = params.navigated_or_inserted_contents;
  EXPECT_TRUE(content::WaitForLoadStop(tab2));

  // The new tab has insecure content.
  ssl_test_util::CheckSecurityState(tab2, CertError::NONE,
                                    security_state::WARNING,
                                    AuthState::DISPLAYED_INSECURE_CONTENT);

  // The original tab should not be contaminated.
  ssl_test_util::CheckAuthenticatedState(tab1, AuthState::NONE);
}

// Visits two pages from the same origin: one that runs insecure content and one
// that doesn't.  The test checks that we propagate the insecure content state
// from one to the other.
// TODO(crbug.com/40709634): Flaky
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_TestRunsInsecureContentTwoTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/blank_page.html")));

  WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();

  // This tab should be fine.
  ssl_test_util::CheckAuthenticatedState(tab1, AuthState::NONE);

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html",
      embedded_test_server()->host_port_pair());

  // Create a new tab in the same process.  Using a NEW_FOREGROUND_TAB
  // disposition won't usually stay in the same process, but this works
  // because we are using process-per-site in SetUpCommandLine.
  GURL url = https_server_.GetURL(replacement_path);
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.source_contents = tab1;
  Navigate(&params);
  WebContents* tab2 = params.navigated_or_inserted_contents;
  EXPECT_TRUE(content::WaitForLoadStop(tab2));

  // Both tabs should have the same process.
  EXPECT_EQ(tab1->GetPrimaryMainFrame()->GetProcess(),
            tab2->GetPrimaryMainFrame()->GetProcess());

  // The new tab has insecure content.
  ssl_test_util::CheckAuthenticationBrokenState(
      tab2, CertError::NONE, AuthState::RAN_INSECURE_CONTENT);

  // Which means the origin for the first tab has also been contaminated with
  // insecure content.
  ssl_test_util::CheckAuthenticationBrokenState(
      tab1, CertError::NONE, AuthState::RAN_INSECURE_CONTENT);
}

// Visits a page with an image over http.  Visits another page over https
// referencing that same image over http (hoping it is coming from the webcore
// memory cache).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysCachedInsecureContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair());

  // Load original page over HTTP.
  const GURL url_http = embedded_test_server()->GetURL(replacement_path);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_http));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);

  // Load again but over SSL.  It should be marked as displaying insecure
  // content (even though the image comes from the WebCore memory cache).
  const GURL url_https = https_server_.GetURL(replacement_path);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_https));
  ssl_test_util::CheckSecurityState(tab, CertError::NONE,
                                    security_state::WARNING,
                                    AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Visits a page with script over http.  Visits another page over https
// referencing that same script over http (hoping it is coming from the webcore
// memory cache).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRunsCachedInsecureContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html",
      embedded_test_server()->host_port_pair());

  // Load original page over HTTP.
  const GURL url_http = embedded_test_server()->GetURL(replacement_path);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_http));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);

  // Load again but over SSL.  It should be marked as displaying insecure
  // content (even though the image comes from the WebCore memory cache).
  const GURL url_https = https_server_.GetURL(replacement_path);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_https));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, CertError::NONE, AuthState::RAN_INSECURE_CONTENT);
}

// This test ensures the CN invalid status does not 'stick' to a certificate
// (see bug #1044942) and that it depends on the host-name.
// Test if disabled due to flakiness http://crbug.com/368280 .
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_TestCNInvalidStickiness) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_mismatched_.Start());

  // First we hit the server with hostname, this generates an invalid policy
  // error.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/google.html")));

  // We get an interstitial page as a result.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID,
      AuthState::SHOWING_INTERSTITIAL);
  ProceedThroughInterstitial(tab);
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID, AuthState::NONE);

  // Now we try again with the right host name this time.
  GURL url(https_server_.GetURL("/ssl/google.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Security state should be OK.
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // Now try again the broken one to make sure it is still broken.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/google.html")));

  // Since we OKed the interstitial last time, we get right to the page.
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID, AuthState::NONE);
}

// Test that navigating to a #ref does not change a bad security state.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRefNavigation) {
  ASSERT_TRUE(https_server_expired_.Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/page_with_refs.html")));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
  // Now navigate to a ref in the page, the security state should not have
  // changed.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/page_with_refs.html#jp")));

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// Tests that closing a page that opened a pop-up with an interstitial does not
// crash the browser (crbug.com/1966).
// TODO(crbug.com/1119359, crbug.com/1338068): Test is flaky on Linux and Chrome
// OS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_TestCloseTabWithUnsafePopup DISABLED_TestCloseTabWithUnsafePopup
#else
#define MAYBE_TestCloseTabWithUnsafePopup TestCloseTabWithUnsafePopup
#endif
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestCloseTabWithUnsafePopup) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // Enable popups without user gesture.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::POPUPS,
                                 CONTENT_SETTING_ALLOW);

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_unsafe_popup.html",
      https_server_expired_.host_port_pair());
  WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(
      https_server_expired_.GetURL("/ssl/bad_iframe.html"));
  nav_observer.StartWatchingNewWebContents();
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(replacement_path)));
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

  // Last activated browser should be the popup.
  Browser* popup_browser = chrome::FindBrowserWithProfile(browser()->profile());
  WebContents* popup = popup_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(popup, tab1);
  nav_observer.Wait();
  ASSERT_TRUE(popup->GetController().GetVisibleEntry());
  EXPECT_EQ(https_server_expired_.GetURL("/ssl/bad_iframe.html"),
            popup->GetController().GetVisibleEntry()->GetURL());
  // The interstitial showing is posted to the message loop and this happens
  // after the navigation, so we need to additionally wait for that to be
  // processed.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(popup));

  // Add another tab to make sure the browser does not exit when we close
  // the first tab.
  GURL url = embedded_test_server()->GetURL("/ssl/google.html");
  auto* contents =
      chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  // Close the first tab.
  chrome::CloseWebContents(browser(), tab1, false);
}

// Visit a page over bad https that is a redirect to a page with good https.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectBadToGoodHTTPS) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  GURL url1 = https_server_expired_.GetURL("/server-redirect?");
  GURL url2 = https_server_.GetURL("/ssl/google.html");

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url1.spec() + url2.spec())));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  // We have been redirected to the good page.
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Visit a page over good https that is a redirect to a page with bad https.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectGoodToBadHTTPS) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  GURL url1 = https_server_.GetURL("/server-redirect?");
  GURL url2 = https_server_expired_.GetURL("/ssl/google.html");
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url1.spec() + url2.spec())));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// Visit a page over http that is a redirect to a page with good HTTPS.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectHTTPToGoodHTTPS) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // HTTP redirects to good HTTPS.
  GURL http_url = embedded_test_server()->GetURL("/server-redirect?");
  GURL good_https_url = https_server_.GetURL("/ssl/google.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(http_url.spec() + good_https_url.spec())));
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Visit a page over http that is a redirect to a page with bad HTTPS.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectHTTPToBadHTTPS) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  const GURL http_url = embedded_test_server()->GetURL("/server-redirect?");
  const GURL bad_https_url = https_server_expired_.GetURL("/ssl/google.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(http_url.spec() + bad_https_url.spec())));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// Visit a page over https that is a redirect to a page with http (to make sure
// we don't keep the secure state).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectHTTPSToHTTP) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  GURL https_url = https_server_.GetURL("/server-redirect?");
  GURL http_url = embedded_test_server()->GetURL("/ssl/google.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(https_url.spec() + http_url.spec())));
  ssl_test_util::CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
}

// Visit a page over https that is a redirect to a non-existent page with http
// (to make sure we don't keep the secure state when redirecting to an error).
// Regression test for crbug.com/1154754.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectHTTPSToInvalidHTTP) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  GURL https_url = https_server_.GetURL("/server-redirect?");
  // Test runners might have servers listening in localhost, and the test
  // constructor routes all URLs to localhost, so use close-socket to make
  // sure we always get an error page.
  GURL invalid_url = embedded_test_server()->GetURL("/close-socket");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(https_url.spec() + invalid_url.spec())));
  auto* helper = SecurityStateTabHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  // Check we don't keep the previous certificate state around.
  EXPECT_FALSE(helper->GetVisibleSecurityState()->certificate);
  EXPECT_EQ(helper->GetSecurityLevel(), security_state::SecurityLevel::NONE);
}

class SSLUITestWaitForDOMNotification : public SSLUITestIgnoreCertErrors,
                                        public content::WebContentsObserver {
 public:
  SSLUITestWaitForDOMNotification()
      : SSLUITestIgnoreCertErrors(), run_loop_(nullptr) {}

  SSLUITestWaitForDOMNotification(const SSLUITestWaitForDOMNotification&) =
      delete;
  SSLUITestWaitForDOMNotification& operator=(
      const SSLUITestWaitForDOMNotification&) = delete;
  ~SSLUITestWaitForDOMNotification() override = default;

  void SetUpOnMainThread() override {
    SSLUITestIgnoreCertErrors::SetUpOnMainThread();
  }

  void set_expected_notification(const std::string& expected_notification) {
    expected_notification_ = expected_notification;
  }

  void set_run_loop(base::RunLoop* run_loop) { run_loop_ = run_loop; }
  void observe(content::WebContents* web_contents) {
    content::WebContentsObserver::Observe(web_contents);
  }

  // content::WebContentsObserver
  void DomOperationResponse(content::RenderFrameHost* render_frame_host,
                            const std::string& json_string) override {
    DCHECK(run_loop_);
    if (json_string == expected_notification_) {
      run_loop_->Quit();
    }
  }

 private:
  std::string expected_notification_;
  raw_ptr<base::RunLoop> run_loop_;
};

// Tests that a mixed resource which includes HTTP in the redirect chain
// is marked as mixed content, even if the end result is HTTPS.
IN_PROC_BROWSER_TEST_F(SSLUITestWaitForDOMNotification,
                       TestMixedContentWithHTTPInRedirectChain) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/blank_page.html")));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // Construct a URL which will be dynamically added to the page as an
  // image. The URL redirects through HTTP, though it ends up at an
  // HTTPS resource.
  GURL http_url = embedded_test_server()->GetURL("/server-redirect?");
  GURL::Replacements http_url_replacements;
  // Be sure to use a non-localhost name for the mixed content request,
  // since local hostnames are not considered mixed content.
  http_url_replacements.SetHostStr("example.test");
  std::string http_url_query =
      EncodeQuery(https_server_.GetURL("/ssl/google_files/logo.gif").spec());
  http_url_replacements.SetQueryStr(http_url_query);
  http_url = http_url.ReplaceComponents(http_url_replacements);

  GURL https_url = https_server_.GetURL("/server-redirect?");
  GURL::Replacements https_url_replacements;
  std::string https_url_query = EncodeQuery(http_url.spec());
  https_url_replacements.SetQueryStr(https_url_query);
  https_url = https_url.ReplaceComponents(https_url_replacements);

  base::RunLoop run_loop;

  // Load the image. It starts at |https_server_|, which redirects to an
  // embedded_test_server() HTTP URL, which redirects back to
  // |https_server_| for the final HTTPS image. Because the redirect
  // chain passes through HTTP, the page should be marked as mixed
  // content.
  set_expected_notification("\"mixed-image-loaded\"");
  observe(tab);
  set_run_loop(&run_loop);
  ASSERT_TRUE(content::ExecJs(
      tab,
      "var loaded = function () {"
      "  window.domAutomationController.send('mixed-image-loaded');"
      "};"
      "var img = document.createElement('img');"
      "img.onload = loaded;"
      "img.src = '" +
          https_url.spec() +
          "';"
          "document.body.appendChild(img);"));

  run_loop.Run();
  ssl_test_util::CheckSecurityState(tab, CertError::NONE,
                                    security_state::WARNING,
                                    AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Visits a page to which we could not connect (bad port) over http and https
// and make sure the security style is correct.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestConnectToBadPort) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("http://localhost:17")));
  ssl_test_util::CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      AuthState::SHOWING_ERROR);

  // Same thing over HTTPS.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://localhost:17")));
  ssl_test_util::CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      AuthState::SHOWING_ERROR);
}

//
// Frame navigation
//

// From a good HTTPS top frame:
// - navigate to an OK HTTPS frame
// - navigate to a bad HTTPS (expect unsafe content and filtered frame), then
//   back
// - navigate to HTTP (expect insecure content), then back
IN_PROC_BROWSER_TEST_F(SSLUITest, TestGoodFrameNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // SetUpOnMainThread adds this hostname to the resolver so that it's not
  // blocked (browser_test_base.cc has a resolver that blocks all non-local
  // hostnames by default to ensure tests don't hit the network). This is
  // critical to do because the request would otherwise get cancelled in the
  // browser before the renderer sees it.

  std::string top_frame_path = GetTopFramePath(
      *embedded_test_server(), https_server_, https_server_expired_);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(top_frame_path)));

  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // Now navigate inside the frame.
  {
    content::LoadStopObserver observer(tab);
    ASSERT_EQ(true, content::EvalJs(tab, "clickLink('goodHTTPSLink');"));
    observer.Wait();
  }

  // We should still be fine.
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // Now let's hit a bad page.
  {
    content::LoadStopObserver observer(tab);
    ASSERT_EQ(true, content::EvalJs(tab, "clickLink('badHTTPSLink');"));
    observer.Wait();
  }

  // The security style should still be secure.
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // And the frame should be blocked.
  content::RenderFrameHost* content_frame = content::FrameMatchingPredicate(
      tab->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, "contentFrame"));
  std::string is_evil_js("document.getElementById('evilDiv') != null;");
  EXPECT_EQ(false, content::EvalJs(content_frame, is_evil_js));

  // Now go back, our state should still be OK.
  {
    content::LoadStopObserver observer(tab);
    tab->GetController().GoBack();
    observer.Wait();
  }
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // Navigate to a page served over HTTP.
  {
    content::LoadStopObserver observer(tab);
    ASSERT_EQ(true, content::EvalJs(tab, "clickLink('HTTPLink');"));
    observer.Wait();
  }

  // Our state should be unauthenticated (in the ran mixed script sense). Note
  // this also displays images from the http page (google.com).
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, CertError::NONE,
      AuthState::RAN_INSECURE_CONTENT | AuthState::DISPLAYED_INSECURE_CONTENT |
          AuthState::DISPLAYED_FORM_WITH_INSECURE_ACTION);

  // Go back, our state should be unchanged.
  {
    content::LoadStopObserver observer(tab);
    tab->GetController().GoBack();
    observer.Wait();
  }

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, CertError::NONE,
      AuthState::RAN_INSECURE_CONTENT | AuthState::DISPLAYED_INSECURE_CONTENT |
          AuthState::DISPLAYED_FORM_WITH_INSECURE_ACTION);
}

// From a bad HTTPS top frame:
// - navigate to an OK HTTPS frame (expected to be still authentication broken).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestBadFrameNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string top_frame_path = GetTopFramePath(
      *embedded_test_server(), https_server_, https_server_expired_);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL(top_frame_path)));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  // Navigate to a good frame.
  content::LoadStopObserver observer(tab);
  ASSERT_EQ(true, content::EvalJs(tab, "clickLink('goodHTTPSLink');"));
  observer.Wait();

  // We should still be authentication broken.
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// From an HTTP top frame, navigate to good and bad HTTPS (security state should
// stay unauthenticated).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnauthenticatedFrameNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string top_frame_path = GetTopFramePath(
      *embedded_test_server(), https_server_, https_server_expired_);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(top_frame_path)));
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);

  // Now navigate inside the frame to a secure HTTPS frame.
  {
    content::LoadStopObserver observer(tab);
    ASSERT_EQ(true, content::EvalJs(tab, "clickLink('goodHTTPSLink');"));
    observer.Wait();
  }

  // We should still be unauthenticated.
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);

  // Now navigate to a bad HTTPS frame.
  {
    content::LoadStopObserver observer(tab);
    ASSERT_EQ(true, content::EvalJs(tab, "clickLink('badHTTPSLink');"));
    observer.Wait();
  }

  // State should not have changed.
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);

  // And the frame should have been blocked (see bug #2316).
  content::RenderFrameHost* content_frame = content::FrameMatchingPredicate(
      tab->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, "contentFrame"));
  std::string is_evil_js("document.getElementById('evilDiv') != null;");
  EXPECT_EQ(false, content::EvalJs(content_frame, is_evil_js));
}

enum class OffMainThreadFetchMode { kEnabled, kDisabled };
enum class SSLUIWorkerFetchTestType { kUseFetch, kUseImportScripts };

class SSLUIWorkerFetchTest
    : public testing::WithParamInterface<SSLUIWorkerFetchTestType>,
      public SSLUITestBase {
 public:
  SSLUIWorkerFetchTest() { EXPECT_TRUE(tmp_dir_.CreateUniqueTempDir()); }

  SSLUIWorkerFetchTest(const SSLUIWorkerFetchTest&) = delete;
  SSLUIWorkerFetchTest& operator=(const SSLUIWorkerFetchTest&) = delete;

  ~SSLUIWorkerFetchTest() override {}

  void SetUpOnMainThread() override { SSLUITestBase::SetUpOnMainThread(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SSLUITestBase::SetUpCommandLine(command_line);
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
  }

 protected:
  void WriteFile(const base::FilePath::StringType& filename,
                 std::string_view contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::WriteFile(tmp_dir_.GetPath().Append(filename), contents));
  }

  void WriteTestFiles(const net::EmbeddedTestServer& remote_server,
                      const std::string& hostname) {
    WriteFile(FILE_PATH_LITERAL("worker_test.html"),
              "<script>"
              "var worker = new Worker('worker.js');"
              "worker.addEventListener("
              "    'message',"
              "    event => { document.title = event.data; });"
              "</script>");
    switch (GetParam()) {
      case SSLUIWorkerFetchTestType::kUseFetch:
        WriteFile(FILE_PATH_LITERAL("worker_test_data.txt.mock-http-headers"),
                  "HTTP/1.1 200 OK\n"
                  "Content-Type: text/plain\n"
                  "Access-Control-Allow-Origin: *");
        WriteFile(FILE_PATH_LITERAL("worker_test_data.txt"), "LOADED");
        WriteFile(FILE_PATH_LITERAL("worker.js"),
                  base::StringPrintf(
                      "fetch('%s')"
                      "  .then(res => res.text())"
                      "  .then(text => postMessage(text))"
                      "  .catch(_ => postMessage('FAILED'))",
                      remote_server.GetURL(hostname, "/worker_test_data.txt")
                          .spec()
                          .c_str()));
        break;
      case SSLUIWorkerFetchTestType::kUseImportScripts: {
        WriteFile(FILE_PATH_LITERAL("imported.js"), "data = 'LOADED';");
        WriteFile(
            FILE_PATH_LITERAL("worker.js"),
            base::StringPrintf(
                "var data = 'FAILED';"
                "try {"
                "  importScripts('%s')"
                "} catch(e) {}"
                "postMessage(data);",
                remote_server.GetURL(hostname, "/imported.js").spec().c_str()));
      } break;
    }
  }

  void RunMixedContentSettingsTest(
      ChromeContentBrowserClientForMixedContentTest* browser_client,
      bool allow_running_insecure_content,
      bool strict_mixed_content_checking,
      bool strictly_block_blockable_mixed_content,
      bool expected_load,
      bool expected_show_blocked,
      bool expected_show_dangerous,
      bool expected_load_after_allow,
      bool expected_show_blocked_after_allow,
      bool expected_show_dangerous_after_allow) {
    SCOPED_TRACE(
        ::testing::Message()
        << "RunMixedContentSettingsTest :"
        << "allow_running_insecure_content="
        << (allow_running_insecure_content ? "true " : "false ")
        << "strict_mixed_content_checking="
        << (strict_mixed_content_checking ? "true " : "false ")
        << "strictly_block_blockable_mixed_content="
        << (strictly_block_blockable_mixed_content ? "true " : "false "));

    // Run the tests in a new tab. This forces each call of
    // RunMixedContentSettingsTest in a single test case to use different tabs
    // and thus different processes, bypassing a subtle race condition where
    // processes can get re-used under Site Isolation and retain their mixed
    // content status (see crbug.com/890372). This ensures all error state is
    // cleared.
    chrome::NewTab(browser());
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(tab));

    CheckErrorStateIsCleared();

    browser_client->SetMixedContentSettings(
        allow_running_insecure_content, strict_mixed_content_checking,
        strictly_block_blockable_mixed_content);
    tab->OnWebPreferencesChanged();
    CheckMixedContentSettings(allow_running_insecure_content,
                              strict_mixed_content_checking,
                              strictly_block_blockable_mixed_content);

    const std::u16string loaded_title = u"LOADED";
    const std::u16string failed_title = u"FAILED";

    {
      // First load.
      content::TitleWatcher watcher(tab, loaded_title);
      watcher.AlsoWaitForTitle(failed_title);
      ASSERT_TRUE(ui_test_utils::NavigateToURL(
          browser(), https_server_.GetURL("/worker_test.html")));
      EXPECT_EQ(expected_load ? loaded_title : failed_title,
                watcher.WaitAndGetTitle());
    }

    EXPECT_EQ(expected_show_blocked,
              content_settings::PageSpecificContentSettings::GetForFrame(
                  tab->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::MIXEDSCRIPT));
    ssl_test_util::CheckSecurityState(
        tab, CertError::NONE,
        expected_show_dangerous ? security_state::DANGEROUS
                                : security_state::SECURE,
        expected_show_dangerous ? AuthState::RAN_INSECURE_CONTENT
                                : AuthState::NONE);
    // Clears title.
    ASSERT_TRUE(
        content::ExecJs(tab->GetPrimaryMainFrame(), "document.title = \"\";"));

    {
      // SetAllowRunningInsecureContent will reload the page.
      content::TitleWatcher watcher(tab, loaded_title);
      watcher.AlsoWaitForTitle(failed_title);
      SetAllowRunningInsecureContent();
      tab->OnWebPreferencesChanged();
      EXPECT_EQ(expected_load_after_allow ? loaded_title : failed_title,
                watcher.WaitAndGetTitle());
    }

    EXPECT_EQ(expected_show_blocked_after_allow,
              content_settings::PageSpecificContentSettings::GetForFrame(
                  tab->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::MIXEDSCRIPT));
    ssl_test_util::CheckSecurityState(
        tab, CertError::NONE,
        expected_show_dangerous_after_allow ? security_state::DANGEROUS
                                            : security_state::SECURE,
        expected_show_dangerous_after_allow ? AuthState::RAN_INSECURE_CONTENT
                                            : AuthState::NONE);

    chrome::CloseTab(browser());
  }

  base::ScopedTempDir tmp_dir_;

 private:
  void SetAllowRunningInsecureContent() {
    content::RenderFrameHost* render_frame_host = browser()
                                                      ->tab_strip_model()
                                                      ->GetActiveWebContents()
                                                      ->GetPrimaryMainFrame();
    mojo::AssociatedRemote<content_settings::mojom::ContentSettingsAgent> agent;
    render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&agent);
    agent->SetAllowRunningInsecureContent();
  }

  void CheckErrorStateIsCleared() {
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_FALSE(content_settings::PageSpecificContentSettings::GetForFrame(
                     tab->GetPrimaryMainFrame())
                     ->IsContentBlocked(ContentSettingsType::MIXEDSCRIPT));
    ssl_test_util::CheckSecurityState(tab, CertError::NONE,
                                      security_state::NONE, AuthState::NONE);
    EXPECT_FALSE(SecurityStateTabHelper::FromWebContents(tab)
                     ->GetVisibleSecurityState()
                     ->ran_mixed_content);
  }

  void CheckMixedContentSettings(bool allow_running_insecure_content,
                                 bool strict_mixed_content_checking,
                                 bool strictly_block_blockable_mixed_content) {
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    const blink::web_pref::WebPreferences& prefs =
        tab->GetOrCreateWebPreferences();
    ASSERT_EQ(prefs.strictly_block_blockable_mixed_content,
              strictly_block_blockable_mixed_content);
    ASSERT_EQ(prefs.allow_running_insecure_content,
              allow_running_insecure_content);
    ASSERT_EQ(prefs.strict_mixed_content_checking,
              strict_mixed_content_checking);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SSLUIWorkerFetchTest,
                       TestUnsafeContentsInWorkerFiltered) {
  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  https_server_expired_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());
  WriteTestFiles(https_server_expired_, "localhost");

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  const std::u16string loaded_title = u"LOADED";
  const std::u16string failed_title = u"FAILED";
  content::TitleWatcher watcher(tab, loaded_title);
  watcher.AlsoWaitForTitle(failed_title);

  // This page will spawn a Worker which will try to load content from
  // BadCertServer.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/worker_test.html")));
  // Expect Worker not to load insecure content.
  EXPECT_EQ(failed_title, watcher.WaitAndGetTitle());
  // The bad content is filtered, expect the state to be authenticated.
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// This test, and the related test TestUnsafeContentsWithUserException, verify
// that if unsafe content is loaded but the host of that unsafe content has a
// user exception, the content runs and the security style is downgraded.
// TODO(crbug.com/40707016): Disabled due to flakiness.
IN_PROC_BROWSER_TEST_P(SSLUIWorkerFetchTest,
                       DISABLED_TestUnsafeContentsInWorkerWithUserException) {
  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  https_server_mismatched_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  // Note that it is necessary to user https_server_mismatched_ here over the
  // other invalid cert servers. This is because the test relies on the two
  // servers having different hosts since SSL exceptions are per-host, not per
  // origin, and https_server_mismatched_ uses 'localhost' rather than
  // '127.0.0.1'.
  ASSERT_TRUE(https_server_mismatched_.Start());

  WriteTestFiles(https_server_mismatched_, "localhost");

  // Navigate to an unsafe site. Proceed with interstitial page to indicate
  // the user approves the bad certificate.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html")));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID,
      AuthState::SHOWING_INTERSTITIAL);
  content::TestNavigationObserver nav_observer(tab, 1);
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
      ->CommandReceived(
          base::NumberToString(security_interstitials::CMD_PROCEED));
  nav_observer.Wait();

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID, AuthState::NONE);

  SecurityStateTabHelper* tab_helper =
      SecurityStateTabHelper::FromWebContents(tab);
  ASSERT_TRUE(tab_helper);
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      tab_helper->GetVisibleSecurityState();
  EXPECT_FALSE(visible_security_state->ran_mixed_content);
  EXPECT_FALSE(visible_security_state->displayed_mixed_content);
  EXPECT_FALSE(visible_security_state->ran_content_with_cert_errors);
  EXPECT_FALSE(visible_security_state->displayed_content_with_cert_errors);

  const std::u16string loaded_title = u"LOADED";
  const std::u16string failed_title = u"FAILED";
  content::TitleWatcher watcher(tab, loaded_title);
  watcher.AlsoWaitForTitle(failed_title);

  // Navigate to safe page that has Worker loading unsafe content.
  // Expect content to load but be marked as auth broken due to running insecure
  // content.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/worker_test.html")));
  // Worker loads insecure content
  EXPECT_EQ(loaded_title, watcher.WaitAndGetTitle());
  ssl_test_util::CheckAuthenticationBrokenState(tab, CertError::NONE,
                                                AuthState::NONE);

  visible_security_state = tab_helper->GetVisibleSecurityState();
  EXPECT_FALSE(visible_security_state->ran_mixed_content);
  EXPECT_FALSE(visible_security_state->displayed_mixed_content);
  EXPECT_TRUE(visible_security_state->ran_content_with_cert_errors);
  EXPECT_FALSE(visible_security_state->displayed_content_with_cert_errors);
}

// This test checks the behavior of mixed content blocking for the requests
// from a dedicated worker by changing the settings in WebPreferences
// with allow_running_insecure_content = true.
// Flaky. See https://crbug.com/1145674.
IN_PROC_BROWSER_TEST_P(
    SSLUIWorkerFetchTest,
    DISABLED_MixedContentSettings_AllowRunningInsecureContent) {
  ChromeContentBrowserClientForMixedContentTest browser_client;
  content::ScopedContentBrowserClientSetting setting(&browser_client);

  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  embedded_test_server()->ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  WriteTestFiles(*embedded_test_server(), "example.com");
  for (bool strict_mixed_content_checking : {true, false}) {
    for (bool strictly_block_blockable_mixed_content : {true, false}) {
      if (strict_mixed_content_checking) {
        RunMixedContentSettingsTest(
            &browser_client, true /* allow_running_insecure_content */,
            strict_mixed_content_checking,
            strictly_block_blockable_mixed_content, false /* expected_load */,
            false /* expected_show_blocked */,
            false /* expected_show_dangerous */,
            false /* expected_load_after_allow */,
            false /* expected_show_blocked_after_allow */,
            false /* expected_show_dangerous_after_allow */);
      } else {
        RunMixedContentSettingsTest(
            &browser_client, true /* allow_running_insecure_content */,
            strict_mixed_content_checking,
            strictly_block_blockable_mixed_content, true /* expected_load */,
            false /* expected_show_blocked */,
            true /* expected_show_dangerous */,
            true /* expected_load_after_allow */,
            false /* expected_show_blocked_after_allow */,
            true /* expected_show_dangerous_after_allow */);
      }
    }
  }
}

// This test checks the behavior of mixed content blocking for the requests
// from a dedicated worker by changing the settings in WebPreferences
// with allow_running_insecure_content = false.
// Disabled due to being flaky. crbug.com/1116670
IN_PROC_BROWSER_TEST_P(
    SSLUIWorkerFetchTest,
    DISABLED_MixedContentSettings_DisallowRunningInsecureContent) {
  ChromeContentBrowserClientForMixedContentTest browser_client;
  content::ScopedContentBrowserClientSetting setting(&browser_client);

  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  embedded_test_server()->ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  WriteTestFiles(*embedded_test_server(), "example.com");
  for (bool strict_mixed_content_checking : {true, false}) {
    for (bool strictly_block_blockable_mixed_content : {true, false}) {
      if (strict_mixed_content_checking) {
        RunMixedContentSettingsTest(
            &browser_client, false /* allow_running_insecure_content */,
            strict_mixed_content_checking,
            strictly_block_blockable_mixed_content, false /* expected_load */,
            false /* expected_show_blocked */,
            false /* expected_show_dangerous */,
            false /* expected_load_after_allow */,
            false /* expected_show_blocked_after_allow */,
            false /* expected_show_dangerous_after_allow */);
      } else if (strictly_block_blockable_mixed_content) {
        RunMixedContentSettingsTest(
            &browser_client, false /* allow_running_insecure_content */,
            strict_mixed_content_checking,
            strictly_block_blockable_mixed_content, false /* expected_load */,
            false /* expected_show_blocked */,
            false /* expected_show_dangerous */,
            false /* expected_load_after_allow */,
            false /* expected_show_blocked_after_allow */,
            false /* expected_show_dangerous_after_allow */);
      } else {
        RunMixedContentSettingsTest(
            &browser_client, false /* allow_running_insecure_content */,
            strict_mixed_content_checking,
            strictly_block_blockable_mixed_content, false /* expected_load */,
            true /* expected_show_blocked */,
            false /* expected_show_dangerous */,
            true /* expected_load_after_allow */,
            false /* expected_show_blocked_after_allow */,
            true /* expected_show_dangerous_after_allow */);
      }
    }
  }
}

// This test checks that all mixed content requests from a dedicated worker are
// blocked regardless of the settings in WebPreferences when
// block-all-mixed-content CSP is set with allow_running_insecure_content=true.
IN_PROC_BROWSER_TEST_P(
    SSLUIWorkerFetchTest,
    MixedContentSettingsWithBlockingCSP_AllowRunningInsecureContent) {
  ChromeContentBrowserClientForMixedContentTest browser_client;
  content::ScopedContentBrowserClientSetting setting(&browser_client);

  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  embedded_test_server()->ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  WriteTestFiles(*embedded_test_server(), "example.com");
  WriteFile(FILE_PATH_LITERAL("worker.js.mock-http-headers"),
            "HTTP/1.1 200 OK\n"
            "Content-Type: application/javascript\n"
            "Content-Security-Policy: block-all-mixed-content;");
  for (bool strict_mixed_content_checking : {true, false}) {
    for (bool strictly_block_blockable_mixed_content : {true, false}) {
      RunMixedContentSettingsTest(
          &browser_client, true /* allow_running_insecure_content */,
          strict_mixed_content_checking, strictly_block_blockable_mixed_content,
          false /* expected_load */, false /* expected_show_blocked */,
          false /* expected_show_dangerous */,
          false /* expected_load_after_allow */,
          false /* expected_show_blocked_after_allow */,
          false /* expected_show_dangerous_after_allow */);
    }
  }
}

// This test checks that all mixed content requests from a dedicated worker are
// blocked regardless of the settings in WebPreferences when
// block-all-mixed-content CSP is set with allow_running_insecure_content=false.
IN_PROC_BROWSER_TEST_P(
    SSLUIWorkerFetchTest,
    MixedContentSettingsWithBlockingCSP_DisallowRunningInsecureContent) {
  ChromeContentBrowserClientForMixedContentTest browser_client;
  content::ScopedContentBrowserClientSetting setting(&browser_client);

  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  embedded_test_server()->ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  WriteTestFiles(*embedded_test_server(), "example.com");
  WriteFile(FILE_PATH_LITERAL("worker.js.mock-http-headers"),
            "HTTP/1.1 200 OK\n"
            "Content-Type: application/javascript\n"
            "Content-Security-Policy: block-all-mixed-content;");
  for (bool strict_mixed_content_checking : {true, false}) {
    for (bool strictly_block_blockable_mixed_content : {true, false}) {
      RunMixedContentSettingsTest(
          &browser_client, false /* allow_running_insecure_content */,
          strict_mixed_content_checking, strictly_block_blockable_mixed_content,
          false /* expected_load */, false /* expected_show_blocked */,
          false /* expected_show_dangerous */,
          false /* expected_load_after_allow */,
          false /* expected_show_blocked_after_allow */,
          false /* expected_show_dangerous_after_allow */);
    }
  }
}

// This test checks that all mixed content requests from a dedicated worker
// which is started from a subframe are blocked if
// allow_running_insecure_content setting is false or
// strict_mixed_content_checking setting is true.
// TODO(carlosil): Re-enable to check if this triggers flakiness due to
// committed interstitials.
IN_PROC_BROWSER_TEST_P(SSLUIWorkerFetchTest, DISABLED_MixedContentSubFrame) {
  ChromeContentBrowserClientForMixedContentTest browser_client;
  content::ScopedContentBrowserClientSetting setting(&browser_client);

  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  embedded_test_server()->ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  WriteTestFiles(*embedded_test_server(), "example.com");
  WriteFile(FILE_PATH_LITERAL("worker_iframe.html"),
            "<script>"
            "var worker = new Worker('worker.js');"
            "worker.addEventListener("
            "    'message',"
            "    event => { parent.postMessage(event.data, '*'); });"
            "</script>");
  WriteFile(FILE_PATH_LITERAL("worker_test.html"),
            "<script>"
            "window.addEventListener("
            "    'message',"
            "    event => { document.title = event.data; });"
            "</script>"
            "<iframe src=\"./worker_iframe.html\" />");

  for (bool allow_running_insecure_content : {true, false}) {
    for (bool strict_mixed_content_checking : {true, false}) {
      for (bool strictly_block_blockable_mixed_content : {true, false}) {
        if (allow_running_insecure_content && !strict_mixed_content_checking) {
          RunMixedContentSettingsTest(
              &browser_client, allow_running_insecure_content,
              strict_mixed_content_checking,
              strictly_block_blockable_mixed_content, true /* expected_load */,
              false /* expected_show_blocked */,
              true /* expected_show_dangerous */,
              true /* expected_load_after_allow */,
              false /* expected_show_blocked_after_allow */,
              true /* expected_show_dangerous_after_allow */);
        } else {
          RunMixedContentSettingsTest(
              &browser_client, allow_running_insecure_content,
              strict_mixed_content_checking,
              strictly_block_blockable_mixed_content, false /* expected_load */,
              false /* expected_show_blocked */,
              false /* expected_show_dangerous */,
              false /* expected_load_after_allow */,
              false /* expected_show_blocked_after_allow */,
              false /* expected_show_dangerous_after_allow */);
        }
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SSLUIWorkerFetchTest,
    ::testing::Values(SSLUIWorkerFetchTestType::kUseFetch,
                      SSLUIWorkerFetchTestType::kUseImportScripts));

// Visits a page with unsafe content and makes sure that if a user exception
// to the certificate error is present, the image is loaded and script
// executes.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeContentsWithUserException) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(SetUpUnsafeContentsWithUserException(
      "/ssl/page_with_unsafe_contents.html"));
  ssl_test_util::CheckAuthenticationBrokenState(tab, CertError::NONE,
                                                AuthState::NONE);

  SecurityStateTabHelper* helper = SecurityStateTabHelper::FromWebContents(tab);
  ASSERT_TRUE(helper);
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();
  EXPECT_FALSE(visible_security_state->ran_mixed_content);
  EXPECT_FALSE(visible_security_state->displayed_mixed_content);
  EXPECT_TRUE(visible_security_state->ran_content_with_cert_errors);
  EXPECT_TRUE(visible_security_state->displayed_content_with_cert_errors);

  // In order to check that the image was loaded, we check its width.
  // The actual image (Google logo) is 114 pixels wide, so we assume a good
  // image is greater than 100.
  EXPECT_GT(content::EvalJs(tab, "ImageWidth();"), 100);

  EXPECT_EQ(true, content::EvalJs(tab, "IsFooSet();"));

  // Test that active subresources with the same certificate errors as
  // the main resources also get noted in |content_with_cert_errors_status|.
  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_unsafe_contents.html",
      https_server_mismatched_.host_port_pair());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL(replacement_path)));
  EXPECT_EQ(true, content::EvalJs(tab, "IsFooSet();"));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID, AuthState::NONE);

  visible_security_state = helper->GetVisibleSecurityState();
  EXPECT_FALSE(visible_security_state->ran_mixed_content);
  EXPECT_FALSE(visible_security_state->displayed_mixed_content);
  EXPECT_TRUE(visible_security_state->ran_content_with_cert_errors);
  EXPECT_TRUE(visible_security_state->displayed_content_with_cert_errors);
}

// Like the test above, but only displaying inactive content (an image).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeImageWithUserException) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(
      SetUpUnsafeContentsWithUserException("/ssl/page_with_unsafe_image.html"));

  SecurityStateTabHelper* helper = SecurityStateTabHelper::FromWebContents(tab);
  ASSERT_TRUE(helper);
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();
  EXPECT_FALSE(visible_security_state->ran_mixed_content);
  EXPECT_FALSE(visible_security_state->displayed_mixed_content);
  EXPECT_FALSE(visible_security_state->ran_content_with_cert_errors);
  EXPECT_TRUE(visible_security_state->displayed_content_with_cert_errors);
  EXPECT_EQ(0u, visible_security_state->cert_status);

  // In order to check that the image was loaded, we check its width.
  // The actual image (Google logo) is 114 pixels wide, so we assume a good
  // image is greater than 100.
  EXPECT_GT(content::EvalJs(tab, "ImageWidth();"), 100);
}

// Test that when the browser blocks displaying insecure content (iframes),
// the indicator shows a secure page, because the blocking made the otherwise
// unsafe page safe (the notification of this state is handled by other means)
IN_PROC_BROWSER_TEST_F(SSLUITestBlock, TestBlockDisplayingInsecureIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_iframe.html",
      embedded_test_server()->host_port_pair());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));

  ssl_test_util::CheckAuthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
}

// Test that when the browser blocks running insecure content, the
// indicator shows a secure page, because the blocking made the otherwise
// unsafe page safe (the notification of this state is handled by other
// means).
IN_PROC_BROWSER_TEST_F(SSLUITestBlock, TestBlockRunningInsecureContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html",
      embedded_test_server()->host_port_pair());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));

  ssl_test_util::CheckAuthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
}

// Visit a page and establish a WebSocket connection over bad https with
// --ignore-certificate-errors. The connection should be established without
// interstitial page showing.
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreCertErrors, TestWSS) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(wss_server_expired_.Start());

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, u"PASS");
  watcher.AlsoWaitForTitle(u"FAIL");

  // Visit bad HTTPS page.
  GURL::Replacements replacements;
  replacements.SetSchemeStr("https");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), wss_server_expired_.GetURL("connect_check.html")
                     .ReplaceComponents(replacements)));

  // We shouldn't have an interstitial page showing here.

  // Test page run a WebSocket wss connection test. The result will be shown
  // as page title.
  const std::u16string result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::EqualsCaseInsensitiveASCII(result, "pass"));
}

// Visit a page and establish a WebSocket connection over bad https with
// --ignore-certificate-errors-spki-list. The connection should be established
// without interstitial page showing.
#if !BUILDFLAG(IS_CHROMEOS_ASH)  // Chrome OS does not support the flag.
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreCertErrorsBySPKIWSS, TestWSSExpired) {
  ASSERT_TRUE(wss_server_expired_.Start());

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, u"PASS");
  watcher.AlsoWaitForTitle(u"FAIL");

  // Visit bad HTTPS page.
  GURL::Replacements replacements;
  replacements.SetSchemeStr("https");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), wss_server_expired_.GetURL("connect_check.html")
                     .ReplaceComponents(replacements)));

  // We shouldn't have an interstitial page showing here.

  // Test page run a WebSocket wss connection test. The result will be shown
  // as page title.
  const std::u16string result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::EqualsCaseInsensitiveASCII(result, "pass"));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Test that HTTPS pages with a bad certificate don't show an interstitial if
// the public key matches a value from --ignore-certificate-errors-spki-list.
#if !BUILDFLAG(IS_CHROMEOS_ASH)  // Chrome OS does not support the flag.
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreCertErrorsBySPKIHTTPS, TestHTTPS) {
  ASSERT_TRUE(https_server_mismatched_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server_mismatched_.GetURL("/ssl/page_with_subresource.html")));

  // We should see no interstitial. The script tag in the page should have
  // loaded and ran (and wasn't blocked by the certificate error).
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
  std::u16string title;
  ui_test_utils::GetCurrentTabTitle(browser(), &title);
  EXPECT_EQ(title, u"This script has loaded");
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Test subresources from an origin with a bad certificate are loaded if the
// public key matches a value from --ignore-certificate-errors-spki-list.
#if !BUILDFLAG(IS_CHROMEOS_ASH)  // Chrome OS does not support the flag.
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreCertErrorsBySPKIHTTPS,
                       TestInsecureSubresource) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_mismatched_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_unsafe_image.html",
      https_server_mismatched_.host_port_pair());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));

  // We should see no interstitial.
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
  // In order to check that the image was loaded, check its width.
  // The actual image (Google logo) is 276 pixels wide.
  EXPECT_GT(content::EvalJs(tab, "ImageWidth();"), 200);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Verifies that the interstitial can proceed, even if JavaScript is disabled.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestInterstitialJavaScriptProceeds) {
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));

  content::LoadStopObserver observer(tab);
  const std::string javascript =
      "window.certificateErrorPageController.proceed();";
  EXPECT_TRUE(content::ExecJs(tab, javascript));
  observer.Wait();
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// Verifies that the interstitial can go back, even if JavaScript is disabled.
// http://crbug.com/322948
IN_PROC_BROWSER_TEST_F(SSLUITest, TestInterstitialJavaScriptGoesBack) {
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  content::LoadStopObserver observer(tab);
  const std::string javascript =
      "window.certificateErrorPageController.dontProceed();";
  EXPECT_TRUE(content::ExecJs(tab, javascript));
  observer.Wait();
  EXPECT_EQ("about:blank", tab->GetVisibleURL().spec());
}

// Verifies that an overridable interstitial has a proceed link.
IN_PROC_BROWSER_TEST_F(SSLUITest, ProceedLinkOverridable) {
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));

  EXPECT_TRUE(chrome_browser_interstitials::InterstitialHasProceedLink(
      tab->GetPrimaryMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestLearnMoreLinkContainsErrorCode) {
  ASSERT_TRUE(https_server_expired_.Start());

  // Navigate to a site that causes an interstitial.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/title1.html")));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));

  // Simulate clicking the learn more link.
  SendInterstitialCommand(browser()->tab_strip_model()->GetActiveWebContents(),
                          security_interstitials::CMD_OPEN_HELP_CENTER);
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL()
                .ref(),
            base::NumberToString(net::ERR_CERT_DATE_INVALID));
}

// Checks that interstitials are not used for subframe SSL errors. Regression
// test for https://crbug.com/808797.
IN_PROC_BROWSER_TEST_F(SSLUITest, SubframeCertError) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  // Insert a broken-HTTPS iframe on the page and check that a generic net
  // error, not a certificate error page, is shown.
  content::TestNavigationObserver observer(tab, 1);
  std::string insert_frame = base::StringPrintf(
      "var i = document.createElement('iframe');"
      "i.src = '%s';"
      "document.body.appendChild(i);",
      https_server_expired_.GetURL("/ssl/google.html").spec().c_str());
  EXPECT_TRUE(content::ExecJs(tab, insert_frame));
  observer.Wait();

  content::RenderFrameHost* child =
      content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child);
  const std::string javascript = base::StringPrintf(
      "(document.querySelector(\"#proceed-link\") === null) "
      "? (%d) : (%d)",
      security_interstitials::CMD_TEXT_NOT_FOUND,
      security_interstitials::CMD_TEXT_FOUND);
  int result = content::EvalJs(child, javascript).ExtractInt();
  EXPECT_EQ(security_interstitials::CMD_TEXT_NOT_FOUND, result);
}

// Verifies that a non-overridable interstitial does not have a proceed link.
IN_PROC_BROWSER_TEST_F(SSLUITestHSTS, TestInterstitialOptionsNonOverridable) {
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL::Replacements replacements;
  replacements.SetHostStr(kHstsTestHostName);
  GURL url = https_server_expired_.GetURL("/ssl/google.html")
                 .ReplaceComponents(replacements);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // Since we are connecting to a different domain than the test server default,
  // we also expect CERT_STATUS_COMMON_NAME_INVALID.
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID | net::CERT_STATUS_COMMON_NAME_INVALID,
      AuthState::SHOWING_INTERSTITIAL);

  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));

  const std::string javascript = base::StringPrintf(
      "(document.querySelector(\"#proceed-link\") === null) "
      "? (%d) : (%d)",
      security_interstitials::CMD_TEXT_NOT_FOUND,
      security_interstitials::CMD_TEXT_FOUND);
  int result =
      content::EvalJs(tab->GetPrimaryMainFrame(), javascript).ExtractInt();
  EXPECT_EQ(security_interstitials::CMD_TEXT_NOT_FOUND, result);
}

// Verifies that links in the interstitial open in a new tab.
// https://crbug.com/717616
IN_PROC_BROWSER_TEST_F(SSLUITest, TestInterstitialLinksOpenInNewTab) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  WebContents* interstitial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingSSLInterstitial(interstitial_tab));
  ssl_test_util::CheckAuthenticationBrokenState(
      interstitial_tab, net::CERT_STATUS_DATE_INVALID,
      AuthState::SHOWING_INTERSTITIAL);

  content::TestNavigationObserver nav_observer(nullptr);
  nav_observer.StartWatchingNewWebContents();

  SSLBlockingPage* ssl_interstitial =
      static_cast<SSLBlockingPage*>(GetInterstitialPage(interstitial_tab));
  security_interstitials::SecurityInterstitialControllerClient* client =
      GetControllerClientFromSSLBlockingPage(ssl_interstitial);

  // Mock out the help center URL so that our test will hit the test server
  // instead of a real server.
  // NOTE: The CMD_OPEN_HELP_CENTER code in
  // components/security_interstitials/core/ssl_error_ui.cc ends up appending
  // a path to whatever URL is passed to it. Since that path doesn't exist on
  // our test server, this results in a 404. This is expected behavior, and
  // things are still working as expected so long as the test passes!
  const GURL mock_help_center_url = https_server_.GetURL("/title1.html");
  client->SetBaseHelpCenterUrlForTesting(mock_help_center_url);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  SendInterstitialCommand(interstitial_tab,
                          security_interstitials::CMD_OPEN_HELP_CENTER);

  nav_observer.Wait();

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  WebContents* new_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(mock_help_center_url.host(), new_tab->GetLastCommittedURL().host());
}

// Verifies that switching tabs, while showing interstitial page, will not
// affect the visibility of the interstitial.
// https://crbug.com/381439
IN_PROC_BROWSER_TEST_F(SSLUITest, InterstitialNotAffectedByHideShow) {
  ASSERT_TRUE(https_server_expired_.Start());
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);
  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());

  ASSERT_TRUE(AddTabAtIndex(0, https_server_.GetURL("/ssl/google.html"),
                            ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(tab, browser()->tab_strip_model()->GetWebContentsAt(1));
  EXPECT_FALSE(tab->GetRenderWidgetHostView()->IsShowing());

  browser()->tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
}

// Verifies that if a bad certificate is seen for any host and the user proceeds
// through the interstitial, the decision to proceed is initially remembered.
// However, if this is followed by another visit, and a good certificate is seen
// for the same host, the original exception is forgotten.
IN_PROC_BROWSER_TEST_F(SSLUITestReduceSubresourceNotifications,
                       HasAllowExceptionForAnyHost) {
  ASSERT_TRUE(https_server_expired_.Start());
  ASSERT_TRUE(https_server_.Start());

  std::string https_server_expired_host =
      https_server_expired_.GetURL("/ssl/google.html").host();
  std::string https_server_host =
      https_server_.GetURL("/ssl/google.html").host();
  ASSERT_EQ(https_server_expired_host, https_server_host);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());

  // First check that frame requests revoke the decision.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));

  ProceedThroughInterstitial(tab);
  EXPECT_TRUE(state->HasAllowExceptionForAnyHost(
      tab->GetPrimaryMainFrame()->GetStoragePartition()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  EXPECT_FALSE(state->HasAllowExceptionForAnyHost(
      tab->GetPrimaryMainFrame()->GetStoragePartition()));
}

// Verifies that if a bad certificate is seen for any host and the user proceeds
// through the interstitial, the decision to proceed is initially remembered.
// However, if this is followed by another visit, and a good certificate is seen
// for the same host, the original exception is forgotten. The state of
// send_subresource_notification does not change in the Webcontents even after a
// good certificate has been seen until the browser process is restarted.
IN_PROC_BROWSER_TEST_F(
    SSLUITestReduceSubresourceNotifications,
    PRE_BadCertFollowedByGoodCertNavigationFollowedByRestart) {
  ASSERT_TRUE(https_server_expired_.Start());
  ASSERT_TRUE(https_server_.Start());

  std::string https_server_expired_host =
      https_server_expired_.GetURL("/ssl/google.html").host();
  std::string https_server_host =
      https_server_.GetURL("/ssl/google.html").host();
  ASSERT_EQ(https_server_expired_host, https_server_host);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());

  // HTTPS-related warning exceptions have not been allowed by the user.
  ASSERT_FALSE(state->HasAllowExceptionForAnyHost(
      tab->GetPrimaryMainFrame()->GetStoragePartition()));
  // Not sending subresource notifications since no HTTPS-exceptions have been
  // allowed by the user.
  ASSERT_FALSE(tab->GetSendSubresourceNotification());

  // Navigate to a page with a certificate error.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Click through the interstitial.
  ProceedThroughInterstitial(tab);

  // HTTPS-related warning exception has been allowed by the user.
  EXPECT_TRUE(state->HasAllowExceptionForAnyHost(
      tab->GetPrimaryMainFrame()->GetStoragePartition()));

  // State in browser process has been updated, i.e., start renderers need to
  // start sending subresource notifications.
  EXPECT_TRUE(tab->GetSendSubresourceNotification());

  // See a good certificate for the same host. This removes the allowed
  // exception but `renderer_preferences_.send_subresource_notification_` is not
  // set to false. This is because allowing and revoking HTTPS related warning
  // exception is rare, and thus is update at the startup of the browser.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  EXPECT_FALSE(state->HasAllowExceptionForAnyHost(
      tab->GetPrimaryMainFrame()->GetStoragePartition()));

  EXPECT_TRUE(tab->GetSendSubresourceNotification());
}

// Verifies that on browser restarts, we update `renderer_preferences_`
// according to state of allowed exceptions at browser start-up.
IN_PROC_BROWSER_TEST_F(SSLUITestReduceSubresourceNotifications,
                       BadCertFollowedByGoodCertNavigationFollowedByRestart) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());

  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());

  EXPECT_FALSE(state->HasAllowExceptionForAnyHost(
      tab->GetPrimaryMainFrame()->GetStoragePartition()));

  EXPECT_FALSE(tab->GetSendSubresourceNotification());
}

// Tests whether any certificate error exceptions made are persisted across
// sessions. This also verifies persistence of `send_subresource_notification_`.
IN_PROC_BROWSER_TEST_F(SSLUITestReduceSubresourceNotifications,
                       PRE_CertDecisionPersistsSessions) {
  ASSERT_TRUE(https_server_expired_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());

  // HTTPS-related warning exceptions have not been allowed by the user.
  ASSERT_FALSE(state->HasAllowExceptionForAnyHost(
      tab->GetPrimaryMainFrame()->GetStoragePartition()));
  // Not sending subresource notifications since no HTTPS-exceptions have been
  // allowed by the user.
  ASSERT_FALSE(tab->GetSendSubresourceNotification());

  // Navigate to a page with a certificate error.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Click through the interstitial.
  ProceedThroughInterstitial(tab);

  // HTTPS-related warning exception has been allowed by the user.
  EXPECT_TRUE(state->HasAllowExceptionForAnyHost(
      tab->GetPrimaryMainFrame()->GetStoragePartition()));

  // renderer_preferences_.send_subresource_notification_ state updated.
  EXPECT_TRUE(tab->GetSendSubresourceNotification());
}

IN_PROC_BROWSER_TEST_F(SSLUITestReduceSubresourceNotifications,
                       CertDecisionPersistsSessions) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());

  // HTTPS-related warning exceptions has been allowed by the user in the past
  // which has not expired.
  EXPECT_TRUE(state->HasAllowExceptionForAnyHost(
      tab->GetPrimaryMainFrame()->GetStoragePartition()));

  // State in browser persists after restart.
  ASSERT_TRUE(tab->GetSendSubresourceNotification());
}

// Tests persistence of `send_subresource_notification_` when multiple bad
// certificates are allowed by the user.
IN_PROC_BROWSER_TEST_F(SSLUITestReduceSubresourceNotifications,
                       MultipleBadCertNavigations) {
  ASSERT_TRUE(https_server_expired_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());

  // HTTPS-related warning exceptions have not been allowed by the user.
  ASSERT_FALSE(state->HasAllowExceptionForAnyHost(
      tab->GetPrimaryMainFrame()->GetStoragePartition()));
  // Not sending subresource notifications since no HTTPS-exceptions have been
  // allowed by the user.
  ASSERT_FALSE(tab->GetSendSubresourceNotification());

  // Navigate to a page with a certificate error.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Click through the interstitial.
  ProceedThroughInterstitial(tab);

  // HTTPS-related warning exception has been allowed by the user.
  EXPECT_TRUE(state->HasAllowExceptionForAnyHost(
      tab->GetPrimaryMainFrame()->GetStoragePartition()));

  // renderer_preferences_.send_subresource_notification_ state updated.
  EXPECT_TRUE(tab->GetSendSubresourceNotification());

  // Navigate to a page with a certificate error, and click through the
  // interstitial so the certificate is allowlisted.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://site.test:" +
                      base::NumberToString(https_server_expired_.port()) +
                      "/ssl/blank_page.html")));
  ProceedThroughInterstitial(tab);

  // HTTPS-related warning exception has been allowed by the user.
  EXPECT_TRUE(state->HasAllowExceptionForAnyHost(
      tab->GetPrimaryMainFrame()->GetStoragePartition()));

  EXPECT_TRUE(tab->GetSendSubresourceNotification());
}

// Verifies that if a bad certificate is seen for a host and the user proceeds
// through the interstitial, the decision to proceed is initially remembered.
// However, if this is followed by another visit, and a good certificate
// is seen for the same host, the original exception is forgotten.
IN_PROC_BROWSER_TEST_F(SSLUITest, BadCertFollowedByGoodCertNavigation) {
  // It is necessary to use |https_server_expired_| rather than
  // |https_server_mismatched| because the former shares a host with
  // |https_server_| and cert exceptions are per host.
  ASSERT_TRUE(https_server_expired_.Start());
  ASSERT_TRUE(https_server_.Start());

  std::string https_server_expired_host =
      https_server_expired_.GetURL("/ssl/google.html").host();
  std::string https_server_host =
      https_server_.GetURL("/ssl/google.html").host();
  ASSERT_EQ(https_server_expired_host, https_server_host);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());

  // First check that frame requests revoke the decision.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));

  ProceedThroughInterstitial(tab);
  EXPECT_TRUE(state->HasAllowException(
      https_server_host, tab->GetPrimaryMainFrame()->GetStoragePartition()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  EXPECT_FALSE(state->HasAllowException(
      https_server_host, tab->GetPrimaryMainFrame()->GetStoragePartition()));
}

// Verifies that if a bad certificate is seen for a host and the user proceeds
// through the interstitial, the decision to proceed is initially remembered.
// However, if this is followed by a subresource load, and a good certificate
// is seen for the same host via the subresource load, the original exception
// is forgotten.
IN_PROC_BROWSER_TEST_F(SSLUITest, BadCertFollowedByGoodCertSubresource) {
  // As in SSLUITest.BadCertFollowedByGoodCertNavigation, it is necessary to use
  // |https_server_expired_| rather than |https_server_mismatched| because the
  // former shares a host with |https_server_| and cert exceptions are per host.
  ASSERT_TRUE(https_server_expired_.Start());
  ASSERT_TRUE(https_server_.Start());

  std::string https_server_expired_host =
      https_server_expired_.GetURL("/ssl/google.html").host();
  std::string https_server_host =
      https_server_.GetURL("/ssl/google.html").host();
  ASSERT_EQ(https_server_expired_host, https_server_host);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());

  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

  ProceedThroughInterstitial(tab);
  EXPECT_TRUE(state->HasAllowException(
      https_server_host, tab->GetPrimaryMainFrame()->GetStoragePartition()));

  GURL image = https_server_.GetURL("/ssl/google_files/logo.gif");
  EXPECT_EQ(
      true,
      EvalJs(tab,
             std::string("var img = document.createElement('img');img.src ='") +
                 image.spec() +
                 "';"
                 "new Promise(resolve => {"
                 "  img.onload=function() { "
                 "    resolve(true); };"
                 "  document.body.appendChild(img);"
                 "});"));
  EXPECT_FALSE(state->HasAllowException(
      https_server_host, tab->GetPrimaryMainFrame()->GetStoragePartition()));
}

// Verifies that if a bad certificate is seen for a host and the user proceeds
// through the interstitial, the decision to proceed is not forgotten once blob
// URLs are loaded (blob loads never have certificate errors).  This is a
// regression test for https://crbug.com/1049625.
IN_PROC_BROWSER_TEST_F(SSLUITest, BadCertFollowedByBlobUrl) {
  ASSERT_TRUE(https_server_expired_.Start());
  std::string https_server_host =
      https_server_expired_.GetURL("/ssl/google.html").host();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());

  // Proceed through the interstitial, accepting the broken cert.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));
  ProceedThroughInterstitial(tab);
  ASSERT_TRUE(state->HasAllowException(
      https_server_host, tab->GetPrimaryMainFrame()->GetStoragePartition()));

  // Load a blob URL.
  content::WebContentsConsoleObserver console_observer(tab);
  console_observer.SetPattern("hello from blob");
  const char kScript[] = R"(
      new Promise(function (resolvePromise, rejectPromise) {
          var blob = new Blob(['console.log("hello from blob")'],
                              {type : 'application/javascript'});
          script = document.createElement('script');
          script.onerror = rejectPromise;
          script.onload = () => resolvePromise('success');
          script.src = URL.createObjectURL(blob);
          document.body.appendChild(script);
      });
  )";
  ASSERT_EQ("success", content::EvalJs(tab, kScript));

  // Verify that the script from the blob has successfully run.
  ASSERT_TRUE(console_observer.Wait());

  // Verify that the decision to accept the broken cert has not been revoked
  // (this is a regression test for https://crbug.com/1049625).
  EXPECT_TRUE(state->HasAllowException(
      https_server_host, tab->GetPrimaryMainFrame()->GetStoragePartition()));
}

// Tests that the SSLStatus of a navigation entry for an SSL
// interstitial matches the navigation entry once the interstitial is
// clicked through. https://crbug.com/529456
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       SSLStatusMatchesOnInterstitialAndAfterProceed) {
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

  content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  content::SSLStatus interstitial_ssl_status = entry->GetSSL();

  ProceedThroughInterstitial(tab);
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(tab));
  entry = tab->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);

  content::SSLStatus after_interstitial_ssl_status = entry->GetSSL();
  EXPECT_TRUE(ComparePreAndPostInterstitialSSLStatuses(
      interstitial_ssl_status, after_interstitial_ssl_status));
}

// As above, but for a bad clock interstitial. Tests that a clock
// interstitial's SSLStatus matches the SSLStatus of the HTTPS page
// after proceeding through a normal SSL interstitial.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       SSLStatusMatchesonClockInterstitialAndAfterProceed) {
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Set up the build and current clock times to be more than a year apart.
  base::SimpleTestClock mock_clock;
  mock_clock.SetNow(base::Time::NowFromSystemTime());
  mock_clock.Advance(base::Days(367));
  SSLErrorHandler::SetClockForTesting(&mock_clock);
  ssl_errors::SetBuildTimeForTesting(base::Time::NowFromSystemTime());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/title1.html")));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingBadClockInterstitial(tab));

  // Grab the SSLStatus on the clock interstitial.
  content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  content::SSLStatus clock_interstitial_ssl_status = entry->GetSSL();

  // Put the clock back to normal, trigger a normal SSL interstitial,
  // and proceed through it.
  mock_clock.SetNow(base::Time::NowFromSystemTime());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/title1.html")));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  ProceedThroughInterstitial(tab);
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(tab));

  // Grab the SSLStatus from the page and check that it is the same as
  // on the clock interstitial.
  entry = tab->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  content::SSLStatus after_interstitial_ssl_status = entry->GetSSL();
  EXPECT_TRUE(ComparePreAndPostInterstitialSSLStatuses(
      clock_interstitial_ssl_status, after_interstitial_ssl_status));
}

// A fixture for testing on-demand network time queries on SSL
// certificate date errors. It can simulate a delayed network time
// request, and it allows the user to configure the experimental
// parameters of the NetworkTimeTracker. Expects only one network time
// request to be issued during the test.
class SSLNetworkTimeBrowserTest : public SSLUITest {
 public:
  SSLNetworkTimeBrowserTest() : SSLUITest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        network_time::kNetworkTimeServiceQuerying,
        {{"FetchBehavior", "on-demand-only"}});
  }

  SSLNetworkTimeBrowserTest(const SSLNetworkTimeBrowserTest&) = delete;
  SSLNetworkTimeBrowserTest& operator=(const SSLNetworkTimeBrowserTest&) =
      delete;

  ~SSLNetworkTimeBrowserTest() override = default;

  void SetUpOnMainThread() override {
    SSLUITest::SetUpOnMainThread();
    controllable_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/", true);
    ASSERT_TRUE(embedded_test_server()->Start());
    g_browser_process->network_time_tracker()->SetTimeServerURLForTesting(
        embedded_test_server()->GetURL("/"));
  }

 protected:
  void TriggerTimeResponse() {
    std::string response = "HTTP/1.1 200 OK\nContent-type: text/plain\n";
    response += base::StringPrintf(
        "Content-Length: %1d\n",
        static_cast<int>(strlen(network_time::kGoodTimeResponseBody[0])));
    response +=
        "x-cup-server-proof: " +
        std::string(network_time::kGoodTimeResponseServerProofHeader[0]);
    response += "\n\n";
    response += std::string(network_time::kGoodTimeResponseBody[0]);
    controllable_response_->WaitForRequest();
    controllable_response_->Send(response);
  }

  // Asserts that the first time request to the server is currently pending.
  void CheckTimeQueryPending() {
    base::Time unused_time;
    base::TimeDelta unused_uncertainty;
    ASSERT_EQ(network_time::NetworkTimeTracker::NETWORK_TIME_FIRST_SYNC_PENDING,
              g_browser_process->network_time_tracker()->GetNetworkTime(
                  &unused_time, &unused_uncertainty));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      controllable_response_;
};

// Tests that if an on-demand network time fetch returns that the clock
// is okay, a normal SSL interstitial is shown.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest, OnDemandFetchClockOk) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Use a testing clock set to the time that GoodTimeResponseHandler
  // returns, to simulate the system clock matching the network time.
  base::SimpleTestClock testing_clock;
  SSLErrorHandler::SetClockForTesting(&testing_clock);
  testing_clock.SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      network_time::kGoodTimeResponseHandlerJsTime[0]));
  // Set the build time to match the testing clock, to ensure that the
  // build time heuristic doesn't fire.
  ssl_errors::SetBuildTimeForTesting(testing_clock.Now());

  // Set a long timeout to ensure that the on-demand time fetch completes.
  SSLErrorHandler::SetInterstitialDelayForTesting(base::Hours(1));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_expired_.GetURL("/"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);

  // Once |interstitial_timer_observer| has fired, the request has been
  // sent. Override the nonce that NetworkTimeTracker expects so that
  // when the response comes back, it will validate. The nonce can only
  // be overriden for the current in-flight request, so the test must
  // call OverrideNonceForTesting() after the request has been sent and
  // before the response has been received.
  interstitial_timer_observer.WaitForTimerStarted();
  g_browser_process->network_time_tracker()->OverrideNonceForTesting(123123123);
  TriggerTimeResponse();

  EXPECT_TRUE(contents->IsLoading());
  // False, because an interstitial is not a normal load result.
  EXPECT_FALSE(content::WaitForLoadStop(contents));

  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(contents));
}

// Tests that if an on-demand network time fetch returns that the clock
// is wrong, a bad clock interstitial is shown.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest, OnDemandFetchClockWrong) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Use a testing clock set to a time that is different from what
  // GoodTimeResponseHandler returns, simulating a system clock that is
  // 30 days ahead of the network time.
  base::SimpleTestClock testing_clock;
  SSLErrorHandler::SetClockForTesting(&testing_clock);
  testing_clock.SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      network_time::kGoodTimeResponseHandlerJsTime[0]));
  testing_clock.Advance(base::Days(30));
  // Set the build time to match the testing clock, to ensure that the
  // build time heuristic doesn't fire.
  ssl_errors::SetBuildTimeForTesting(testing_clock.Now());

  // Set a long timeout to ensure that the on-demand time fetch completes.
  SSLErrorHandler::SetInterstitialDelayForTesting(base::Hours(1));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_expired_.GetURL("/"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);

  // Once |interstitial_timer_observer| has fired, the request has been
  // sent. Override the nonce that NetworkTimeTracker expects so that
  // when the response comes back, it will validate. The nonce can only
  // be overriden for the current in-flight request, so the test must
  // call OverrideNonceForTesting() after the request has been sent and
  // before the response has been received.
  interstitial_timer_observer.WaitForTimerStarted();
  g_browser_process->network_time_tracker()->OverrideNonceForTesting(123123123);
  TriggerTimeResponse();

  EXPECT_TRUE(contents->IsLoading());
  // False, because an interstitial is not a normal load result.
  EXPECT_FALSE(content::WaitForLoadStop(contents));

  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingBadClockInterstitial(contents));
}

// Tests that if the timeout expires before the network time fetch
// returns, then a normal SSL interstitial is shown.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest,
                       TimeoutExpiresBeforeFetchCompletes) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Set the timer to fire immediately.
  SSLErrorHandler::SetInterstitialDelayForTesting(base::TimeDelta());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           https_server_expired_.GetURL("/")));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(contents));

  // Navigate away, and then trigger the network time response; no crash should
  // occur.
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/")));
  ASSERT_NO_FATAL_FAILURE(CheckTimeQueryPending());
  TriggerTimeResponse();
}

// Tests that if the user stops the page load before either the network
// time fetch completes or the timeout expires, then there is no interstitial.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest, StopBeforeTimeoutExpires) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Set the timer to a long delay.
  SSLErrorHandler::SetInterstitialDelayForTesting(base::Hours(1));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_expired_.GetURL("/"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  contents->Stop();
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(SSLErrorHandler::FromWebContents(contents));
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  EXPECT_FALSE(contents->IsLoading());

  // Navigate away, and then trigger the network time response; no crash should
  // occur.
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/title1.html")));
  ASSERT_NO_FATAL_FAILURE(CheckTimeQueryPending());
  TriggerTimeResponse();
}

// Tests that if the user reloads the page before either the network
// time fetch completes or the timeout expires, then there is no interstitial.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest, ReloadBeforeTimeoutExpires) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Set the timer to a long delay.
  SSLErrorHandler::SetInterstitialDelayForTesting(base::Hours(1));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_expired_.GetURL("/"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::TestNavigationObserver observer(contents);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();

  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(SSLErrorHandler::FromWebContents(contents));
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  EXPECT_FALSE(contents->IsLoading());

  // Navigate away, and then trigger the network time response and wait
  // for the response; no crash should occur.
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/")));
  ASSERT_NO_FATAL_FAILURE(CheckTimeQueryPending());
  TriggerTimeResponse();
}

// Tests that if the user navigates away before either the network time
// fetch completes or the timeout expires, then there is no
// interstitial.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest,
                       NavigateAwayBeforeTimeoutExpires) {
  ASSERT_TRUE(https_server_expired_.Start());
  ASSERT_TRUE(https_server_.Start());
  // Set the timer to a long delay.
  SSLErrorHandler::SetInterstitialDelayForTesting(base::Hours(1));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_expired_.GetURL("/"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::TestNavigationObserver observer(contents, 1);
  browser()->OpenURL(
      content::OpenURLParams(https_server_.GetURL("/"), content::Referrer(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});
  observer.Wait();

  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(SSLErrorHandler::FromWebContents(contents));
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  EXPECT_FALSE(contents->IsLoading());

  // Navigate away, and then trigger the network time response and wait
  // for the response; no crash should occur.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/")));
  ASSERT_NO_FATAL_FAILURE(CheckTimeQueryPending());
  TriggerTimeResponse();
}

// Tests that if the user closes the tab before the network time fetch
// completes, it doesn't cause a crash.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest,
                       CloseTabBeforeNetworkFetchCompletes) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Set the timer to fire immediately.
  SSLErrorHandler::SetInterstitialDelayForTesting(base::TimeDelta());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           https_server_expired_.GetURL("/")));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(contents));

  // Open a second tab, close the first, and then trigger the network time
  // response and wait for the response; no crash should occur.
  ASSERT_TRUE(https_server_.Start());
  ASSERT_FALSE(
      AddTabAtIndex(1, https_server_.GetURL("/"), ui::PAGE_TRANSITION_TYPED));
  chrome::CloseWebContents(browser(), contents, false);
  ASSERT_NO_FATAL_FAILURE(CheckTimeQueryPending());
  TriggerTimeResponse();
}

class CommonNameMismatchBrowserTest : public CertVerifierBrowserTest {
 public:
  CommonNameMismatchBrowserTest() : CertVerifierBrowserTest() {
    // Enable finch experiment for SSL common name mismatch handling.
    base::FieldTrialList::CreateFieldTrial("SSLCommonNameMismatchHandling",
                                           "Enabled");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    CertVerifierBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void TearDownOnMainThread() override {
    CertVerifierBrowserTest::TearDownOnMainThread();
  }
};

// Visit the URL www.mail.example.com on a server that presents a valid
// certificate for mail.example.com. Verify that the page navigates to
// mail.example.com.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       ShouldShowWWWSubdomainMismatchInterstitial) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain.ServeFilesFromSourceDirectory(
      GetChromeTestDataDir());
  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  // Use the "spdy_pooling.pem" cert which has "mail.example.com"
  // as one of its SANs.
  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  // Request to "www.mail.example.com" should result in
  // |net::ERR_CERT_COMMON_NAME_INVALID| error.
  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "www.mail.example.com", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  // Request to "www.mail.example.com" should not result in any error.
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "mail.example.com",
                                                verify_result_valid, net::OK);

  // Use a complex URL to ensure the path, etc., are preserved. The path itself
  // does not matter.
  const GURL https_server_url =
      https_server_example_domain.GetURL("/ssl/google.html?a=b#anchor");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  const GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(contents, 1);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), https_server_mismatched_url));
  observer.Wait();

  ssl_test_util::CheckSecurityState(contents, CertError::NONE,
                                    security_state::SECURE, AuthState::NONE);
  replacements.SetHostStr("mail.example.com");
  GURL https_server_new_url = https_server_url.ReplaceComponents(replacements);
  // Verify that the current URL is the suggested URL.
  EXPECT_EQ(https_server_new_url.spec(),
            contents->GetLastCommittedURL().spec());
}

// Visit the URL www.mail.example.com on a server that presents an invalid
// certificate for mail.example.com. Verify that the page shows an interstitial
// for www.mail.example.com with no crash.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       NoCrashIfBothSubdomainsHaveCommonNameErrors) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain.ServeFilesFromSourceDirectory(
      GetChromeTestDataDir());
  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  // Use the "spdy_pooling.pem" cert which has "mail.example.com"
  // as one of its SANs.
  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  // Request to "www.mail.example.com" should result in
  // |net::ERR_CERT_COMMON_NAME_INVALID| error.
  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "www.mail.example.com", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  // Request to "mail.example.com" should also result in
  // |net::ERR_CERT_COMMON_NAME_INVALID| error.
  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "mail.example.com", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  // Use a complex URL to ensure the path, etc., are preserved. The path itself
  // does not matter.
  const GURL https_server_url =
      https_server_example_domain.GetURL("/ssl/google.html?a=b#anchor");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  const GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  // Should simply show an interstitial, because both subdomains have common
  // name errors.
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), https_server_mismatched_url));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(contents));

  ssl_test_util::CheckSecurityState(
      contents, net::CERT_STATUS_COMMON_NAME_INVALID, security_state::DANGEROUS,
      AuthState::SHOWING_INTERSTITIAL);
}

// Visit the URL example.org on a server that presents a valid certificate
// for www.example.org. Verify that the page redirects to www.example.org.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       CheckWWWSubdomainMismatchInverse) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain.ServeFilesFromSourceDirectory(
      GetChromeTestDataDir());
  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "example.org", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "www.example.org",
                                                verify_result_valid, net::OK);

  const GURL https_server_url =
      https_server_example_domain.GetURL("/ssl/google.html?a=b");
  GURL::Replacements replacements;
  replacements.SetHostStr("example.org");
  const GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(contents, 1);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), https_server_mismatched_url));
  observer.Wait();

  ssl_test_util::CheckSecurityState(contents, CertError::NONE,
                                    security_state::SECURE, AuthState::NONE);
}

namespace {
// Redirects incoming request to http://example.org.
std::unique_ptr<net::test_server::HttpResponse> HTTPSToHTTPRedirectHandler(
    const net::EmbeddedTestServer* test_server,
    const net::test_server::HttpRequest& request) {
  GURL::Replacements replacements;
  replacements.SetHostStr("example.org");
  replacements.SetSchemeStr("http");
  const GURL redirect_url =
      test_server->base_url().ReplaceComponents(replacements);

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", redirect_url.spec());
  return std::move(http_response);
}
}  // namespace

// Common name mismatch handling feature should ignore redirects when pinging
// the suggested hostname. Visit the URL example.org on a server that presents a
// valid certificate for www.example.org. In this case, www.example.org
// redirects to http://example.org, and the SSL error should not be redirected
// to this URL.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       WWWSubdomainMismatch_StopOnRedirects) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);

  // Redirect all URLs to http://example.org. Since this test will trigger only
  // one request to check the suggested URL, redirecting all requests is OK.
  // We would normally use content::SetupCrossSiteRedirector here, but that
  // function does not support https to http redirects.
  // This must be done before ServeFilesFromSourceDirectory(), otherwise the
  // test server will serve files instead of redirecting requests to them.
  https_server_example_domain.RegisterRequestHandler(base::BindRepeating(
      &HTTPSToHTTPRedirectHandler, &https_server_example_domain));

  https_server_example_domain.ServeFilesFromSourceDirectory(
      GetChromeTestDataDir());

  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "example.org", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "www.example.org",
                                                verify_result_valid, net::OK);

  // The user will visit https://example.org:port/ssl/blank.html.
  GURL::Replacements replacements;
  replacements.SetHostStr("example.org");
  const GURL https_server_mismatched_url =
      https_server_example_domain.GetURL("/ssl/blank.html")
          .ReplaceComponents(replacements);

  // Should simply show an interstitial, because the suggested URL
  // (https://www.example.org) redirected to http://example.org.
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), https_server_mismatched_url));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(contents));

  ssl_test_util::CheckSecurityState(
      contents, net::CERT_STATUS_COMMON_NAME_INVALID, security_state::DANGEROUS,
      AuthState::SHOWING_INTERSTITIAL);
}

// Tests this scenario:
// - |CommonNameMismatchHandler| does not give a callback as it's set into the
//   state |IGNORE_REQUESTS_FOR_TESTING|. So no suggested URL check result can
//   arrive.
// - A cert error triggers an interstitial timer with a very long timeout.
// - No suggested URL check results arrive, causing the tab to appear as loading
//   indefinitely (also because the timer has a long timeout).
// - Stopping the page load shouldn't result in any interstitials.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       InterstitialStopNavigationWhileLoading) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain.ServeFilesFromSourceDirectory(
      GetChromeTestDataDir());
  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "www.mail.example.com", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "mail.example.com",
                                                verify_result_valid, net::OK);

  const GURL https_server_url =
      https_server_example_domain.GetURL("/ssl/google.html?a=b");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  const GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  CommonNameMismatchHandler::set_state_for_testing(
      CommonNameMismatchHandler::IGNORE_REQUESTS_FOR_TESTING);
  // Set delay long enough so that the page appears loading.
  SSLErrorHandler::SetInterstitialDelayForTesting(base::Hours(1));
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_mismatched_url,
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  contents->Stop();
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  SSLErrorHandler* ssl_error_handler =
      SSLErrorHandler::FromWebContents(contents);
  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(ssl_error_handler);
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  EXPECT_FALSE(contents->IsLoading());
}

// Same as above, but instead of stopping, the loading page is reloaded. The end
// result is the same. (i.e. page load stops, no interstitials shown)
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       InterstitialReloadNavigationWhileLoading) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain.ServeFilesFromSourceDirectory(
      GetChromeTestDataDir());
  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "www.mail.example.com", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "mail.example.com",
                                                verify_result_valid, net::OK);

  const GURL https_server_url =
      https_server_example_domain.GetURL("/ssl/google.html?a=b");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  const GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  CommonNameMismatchHandler::set_state_for_testing(
      CommonNameMismatchHandler::IGNORE_REQUESTS_FOR_TESTING);
  // Set delay long enough so that the page appears loading.
  SSLErrorHandler::SetInterstitialDelayForTesting(base::Hours(1));
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_mismatched_url,
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::TestNavigationObserver observer(contents, 1);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();

  SSLErrorHandler* ssl_error_handler =
      SSLErrorHandler::FromWebContents(contents);
  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(ssl_error_handler);
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  EXPECT_FALSE(contents->IsLoading());
}

// Same as above, but instead of reloading, the page is navigated away. The
// new page should load, and no interstitials should be shown.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       InterstitialNavigateAwayWhileLoading) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain.ServeFilesFromSourceDirectory(
      GetChromeTestDataDir());
  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "www.mail.example.com", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "mail.example.com",
                                                verify_result_valid, net::OK);

  const GURL https_server_url =
      https_server_example_domain.GetURL("/ssl/google.html?a=b");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  const GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  CommonNameMismatchHandler::set_state_for_testing(
      CommonNameMismatchHandler::IGNORE_REQUESTS_FOR_TESTING);
  // Set delay long enough so that the page appears loading.
  SSLErrorHandler::SetInterstitialDelayForTesting(base::Hours(1));
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_mismatched_url,
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::TestNavigationObserver observer(contents, 1);
  browser()->OpenURL(
      content::OpenURLParams(GURL("https://google.com"), content::Referrer(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});
  observer.Wait();

  SSLErrorHandler* ssl_error_handler =
      SSLErrorHandler::FromWebContents(contents);
  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(ssl_error_handler);
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  EXPECT_FALSE(contents->IsLoading());
}

class SSLBlockingPageIDNTest
    : public chrome_browser_interstitials::SecurityInterstitialIDNTest {
 protected:
  // chrome_browser_interstitials::SecurityInterstitialIDNTest:
  security_interstitials::SecurityInterstitialPage* CreateInterstitial(
      WebContents* contents,
      const GURL& request_url) const override {
    net::SSLInfo ssl_info;
    ssl_info.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ChromeSecurityBlockingPageFactory blocking_page_factory;
    return blocking_page_factory
        .CreateSSLPage(contents, net::ERR_CERT_CONTAINS_ERRORS, ssl_info,
                       request_url, 0, base::Time::NowFromSystemTime(), GURL())
        .release();
  }
};

// Flaky on mac OS and Windows: https://crbug.com/689846
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_SSLBlockingPageDecodesIDN DISABLED_SSLBlockingPageDecodesIDN
#else
#define MAYBE_SSLBlockingPageDecodesIDN SSLBlockingPageDecodesIDN
#endif

IN_PROC_BROWSER_TEST_F(SSLBlockingPageIDNTest,
                       MAYBE_SSLBlockingPageDecodesIDN) {
  EXPECT_TRUE(VerifyIDNDecoded());
}

IN_PROC_BROWSER_TEST_F(CertVerifierBrowserTest, MockCertVerifierSmokeTest) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  mock_cert_verifier()->set_default_result(
      net::ERR_CERT_NAME_CONSTRAINT_VIOLATION);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("/ssl/google.html")));

  ssl_test_util::CheckSecurityState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      net::CERT_STATUS_NAME_CONSTRAINT_VIOLATION, security_state::DANGEROUS,
      AuthState::SHOWING_INTERSTITIAL);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, RestoreHasSSLState) {
  ASSERT_TRUE(https_server_.Start());
  GURL url(https_server_.GetURL("/ssl/google.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  content::NavigationEntry* entry =
      tab->GetController().GetLastCommittedEntry();
  std::unique_ptr<content::NavigationEntry> restored_entry =
      content::NavigationController::CreateNavigationEntry(
          url, content::Referrer(), /* initiator_origin= */ std::nullopt,
          /* initiator_base_url= */ std::nullopt, ui::PAGE_TRANSITION_RELOAD,
          false, std::string(), tab->GetBrowserContext(),
          nullptr /* blob_url_loader_factory */);
  std::unique_ptr<content::NavigationEntryRestoreContext> context =
      content::NavigationEntryRestoreContext::Create();
  restored_entry->SetPageState(entry->GetPageState(), context.get());

  WebContents::CreateParams params(tab->GetBrowserContext());
  std::unique_ptr<WebContents> tab2 = WebContents::Create(params);
  WebContents* raw_tab2 = tab2.get();
  tab->GetDelegate()->AddNewContents(
      nullptr, std::move(tab2), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      blink::mojom::WindowFeatures(), false, nullptr);
  std::vector<std::unique_ptr<content::NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  content::TestNavigationObserver observer(raw_tab2);
  raw_tab2->GetController().Restore(entries.size() - 1,
                                    content::RestoreType::kRestored, &entries);
  raw_tab2->GetController().LoadIfNecessary();
  observer.Wait();
  ssl_test_util::CheckAuthenticatedState(raw_tab2, AuthState::NONE);
}

void SetupRestoredTabWithNavigation(
    net::test_server::EmbeddedTestServer* https_server,
    Browser* browser) {
  ASSERT_TRUE(https_server->Start());
  GURL url(https_server->GetURL("/ssl/google.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, url));
  WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver observer(tab);
  EXPECT_TRUE(ExecJs(tab, "history.pushState({}, '', '');"));
  observer.Wait();

  ui_test_utils::NavigateToURLWithDisposition(
      browser, GURL("about:blank"), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  chrome::CloseTab(browser);

  WebContents* blank_tab = browser->tab_strip_model()->GetActiveWebContents();

  // Restore the tab.
  ui_test_utils::TabAddedWaiter tab_added_waiter(browser);
  chrome::RestoreTab(browser);
  tab_added_waiter.Wait();
  tab = browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  EXPECT_NE(tab, blank_tab);
}

// Simulate a browser-initiated in-page navigation in a restored tab.
// https://crbug.com/662267
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       BrowserInitiatedExistingPageAfterRestoreHasSSLState) {
  SetupRestoredTabWithNavigation(&https_server_, browser());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WaitForLoadStop(tab));
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Simulate a renderer-initiated in-page navigation in a restored tab.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       RendererInitiatedExistingPageAfterRestoreHasSSLState) {
  SetupRestoredTabWithNavigation(&https_server_, browser());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  content::TestNavigationObserver observer(tab);
  ASSERT_TRUE(
      content::ExecJs(tab, "location.replace(window.location.href + '#1')"));
  observer.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

namespace {

// A handler which changes the response. The first time it's called for
// |relative_url| it'll give an empty response. The second time it'll
// redirect to |redirect_url|.
std::unique_ptr<net::test_server::HttpResponse> ChangingHandler(
    int* count,
    const std::string& relative_url,
    const GURL& redirect_url,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != relative_url)
    return nullptr;

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  if ((*count)++) {
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", redirect_url.spec());
  }
  return std::move(http_response);
}

}  // namespace

// Check that SSL state isn't stale when navigating to an existing page that
// gives a different response. This covers the case of going from http to
// https. http://crbug.com/792221
IN_PROC_BROWSER_TEST_F(SSLUITest, ExistingPageHTTPToHTTPSSSLState) {
  ASSERT_TRUE(https_server_.Start());
  int count = 0;
  std::string relative_url = "/foo";
  GURL redirect_url = https_server_.GetURL("/simple.html");
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(ChangingHandler, &count, relative_url, redirect_url));
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL(relative_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);

  content::TestNavigationObserver observer(tab, 1);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();

  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Check that SSL state isn't stale when navigating to an existing page that
// gives a different response. This covers the case of going from https to
// http URL. http://crbug.com/792221
IN_PROC_BROWSER_TEST_F(SSLUITest, ExistingPageHTTPSToHTTPSSLState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  int count = 0;
  std::string relative_url = "/foo";
  GURL redirect_url = embedded_test_server()->GetURL("/simple.html");
  https_server_.RegisterRequestHandler(
      base::BindRepeating(ChangingHandler, &count, relative_url, redirect_url));
  ASSERT_TRUE(https_server_.Start());
  const GURL url = https_server_.GetURL(relative_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  content::TestNavigationObserver observer(tab, 1);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();

  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);

  // We also manually check the cert on the NavigationEntry, since in the case
  // of http URLs GetSecurityLevelForRequest will return SecurityLevel::NONE for
  // http URLs.
  content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
  ASSERT_FALSE(entry->GetSSL().certificate);
}

// Checks that a restore followed immediately by a history navigation doesn't
// lose SSL state.
// Disabled since this is a test for bug 738177.
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_RestoreThenNavigateHasSSLState) {
  ASSERT_TRUE(https_server_.Start());
  GURL url1(https_server_.GetURL("/ssl/google.html"));
  GURL url2(https_server_.GetURL("/ssl/page_with_refs.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  chrome::CloseTab(browser());

  ui_test_utils::TabAddedWaiter tab_added_waiter(browser());
  chrome::RestoreTab(browser());
  tab_added_waiter.Wait();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationManager observer(tab, url1);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(observer.WaitForNavigationFinished());
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Simulate the URL changing when the user presses enter in the omnibox. This
// could happen when the user's login is expired and the server redirects them
// to a login page. This will be considered a same document navigation but we
// do want to update the SSL state.
IN_PROC_BROWSER_TEST_F(SSLUITest, SameDocumentHasSSLState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a simple page and then perform an in-page navigation.
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));

  GURL fragment_change_url(embedded_test_server()->GetURL("/title1.html#foo"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), fragment_change_url));
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);

  // Replace the URL of the current NavigationEntry with one that will cause
  // a server redirect when loaded.
  {
    GURL redirect_dest_url(https_server_.GetURL("/ssl/google.html"));
    content::TestNavigationObserver observer(tab);
    std::string script = "history.replaceState({}, '', '/server-redirect?" +
                         redirect_dest_url.spec() + "')";
    EXPECT_TRUE(ExecJs(tab, script));
    observer.Wait();
  }

  // Simulate the user hitting Enter in the omnibox without changing the URL.
  {
    content::TestNavigationObserver observer(tab);
    tab->GetController().LoadURL(tab->GetLastCommittedURL(),
                                 content::Referrer(), ui::PAGE_TRANSITION_LINK,
                                 std::string());
    observer.Wait();
  }

  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Simulate the user revisiting a page without triggering a reload (e.g., when
// clicking a bookmark with an anchor hash twice).  As this is a same document
// navigation, the SSL state should be left intact despite not triggering a
// network request. Regression test for https://crbug.com/877618.
IN_PROC_BROWSER_TEST_F(SSLUITest, SameDocumentHasSSLStateNoLoad) {
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL start_url(https_server_.GetURL("/ssl/google.html#foo"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));

  // Simulate clicking on a bookmark.
  {
    content::LoadStopObserver observer(tab);
    NavigateParams navigate_params(browser(), start_url,
                                   ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    Navigate(&navigate_params);
    observer.Wait();
  }

  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Checks that if a client redirect occurs while the page is loading, the SSL
// state reflects the final URL.
IN_PROC_BROWSER_TEST_F(SSLUITest, ClientRedirectSSLState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  GURL https_url = https_server_.GetURL("/ssl/redirect.html?");
  GURL http_url = embedded_test_server()->GetURL("/ssl/google.html");
  GURL url(https_url.spec() + http_url.spec());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationManager navigation_observer_https(tab, url);
  content::TestNavigationManager navigation_observer_http(tab, http_url);

  tab->GetController().LoadURL(url, content::Referrer(),
                               ui::PAGE_TRANSITION_LINK, std::string());

  ASSERT_TRUE(navigation_observer_https.WaitForNavigationFinished());
  ASSERT_TRUE(navigation_observer_http.WaitForNavigationFinished());
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Checks that if a redirect occurs while the page is loading from a mixed
// content to a valid HTTPS page, the SSL state reflects the final URL.
IN_PROC_BROWSER_TEST_F(SSLUITest, ClientRedirectFromMixedContentSSLState) {
  ASSERT_TRUE(https_server_.Start());

  GURL url = GURL(
      https_server_.GetURL("/ssl/redirect_with_mixed_content.html").spec() +
      "?" + https_server_.GetURL("/ssl/google.html").spec());

  // Load a page that displays insecure content.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Checks that if a redirect occurs while the page is loading from a valid HTTPS
// page to a mixed content page, the SSL state reflects the final URL.
IN_PROC_BROWSER_TEST_F(SSLUITest, ClientRedirectToMixedContentSSLState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  GURL redirect(https_server_.GetURL("/ssl/redirect.html"));
  GURL final_url(
      https_server_.GetURL("/ssl/page_displays_insecure_content.html"));
  GURL url = GURL(redirect.spec() + "?" + final_url.spec());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationManager navigation_manager_redirect(tab, url);
  content::TestNavigationManager navigation_manager_final_url(tab, final_url);

  tab->GetController().LoadURL(url, content::Referrer(),
                               ui::PAGE_TRANSITION_LINK, std::string());

  ASSERT_TRUE(navigation_manager_redirect.WaitForNavigationFinished());
  ASSERT_TRUE(navigation_manager_final_url.WaitForNavigationFinished());
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  ssl_test_util::CheckSecurityState(tab, CertError::NONE,
                                    security_state::WARNING,
                                    AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Checks that same-document navigations during page load preserve SSL state.
IN_PROC_BROWSER_TEST_F(SSLUITest, SameDocumentNavigationDuringLoadSSLState) {
  ASSERT_TRUE(https_server_.Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server_.GetURL("/ssl/same_document_navigation_during_load.html")));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Checks that same-document navigations after the page load preserve SSL
// state.
IN_PROC_BROWSER_TEST_F(SSLUITest, SameDocumentNavigationAfterLoadSSLState) {
  ASSERT_TRUE(https_server_.Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(tab, "location.hash = Math.random()"));
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Checks that navigations after pushState maintain the SSL status.
// Flaky, see https://crbug.com/872029 and https://crbug.com/872030.
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_PushStateSSLState) {
  ASSERT_TRUE(https_server_.Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  content::TestNavigationObserver observer(tab);
  EXPECT_TRUE(ExecJs(tab, "history.pushState({}, '', '');"));
  observer.Wait();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WaitForLoadStop(tab));
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

class SSLUITestNoCert : public SSLUITest,
                        public CertificateManagerModel::Observer {
 public:
  SSLUITestNoCert() = default;
  ~SSLUITestNoCert() override = default;

  void SetUp() override {
    net::TestRootCerts::GetInstance()->Clear();
    SSLUITest::SetUp();
  }

  // CertificateManagerModel::Observer implementation:
  void CertificatesRefreshed() override {}
};

class TestCertDatabaseObserver : public net::CertDatabase::Observer {
 public:
  TestCertDatabaseObserver() {
    net::CertDatabase* cert_db = net::CertDatabase::GetInstance();
    cert_db->AddObserver(this);
  }
  ~TestCertDatabaseObserver() override = default;
  void OnTrustStoreChanged() override { run_loop.Quit(); }
  void WaitForCertDBChange() { run_loop.Run(); }

 private:
  base::RunLoop run_loop;
};

// Checks that a newly-added certificate authority is usable immediately.
IN_PROC_BROWSER_TEST_F(SSLUITestNoCert, NewCertificateAuthority) {
  if (!content::IsOutOfProcessNetworkService())
    return;

  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

  std::unique_ptr<CertificateManagerModel> model;
  base::RunLoop run_loop;
  CertificateManagerModel::Create(
      browser()->profile(), this,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<CertificateManagerModel> model2) {
            model = std::move(model2);
            run_loop.Quit();
          }));
  run_loop.Run();

  scoped_refptr<net::X509Certificate> cert;
  net::ScopedCERTCertificateList nss_certs;

  {
    base::ScopedAllowBlockingForTesting allow_io;
    cert = net::CreateCertificateChainFromFile(
        net::GetTestCertsDirectory(), "root_ca_cert.pem",
        net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);

    nss_certs = net::x509_util::CreateCERTCertificateListFromX509Certificate(
        cert.get());
  }

  TestCertDatabaseObserver cert_database_observer;
  net::NSSCertDatabase::ImportCertFailureList not_imported;
  EXPECT_TRUE(model->ImportCACerts(nss_certs, net::NSSCertDatabase::TRUSTED_SSL,
                                   &not_imported));
  cert_database_observer.WaitForCertDBChange();
  EXPECT_TRUE(not_imported.empty());

  content::FlushNetworkServiceInstanceForTesting();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(tab));
}

// A test class which prepares two profiles and allows importing certificates
// into their NSS databases.
class SSLUITestCustomCACerts : public SSLUITestNoCert {
 public:
  SSLUITestCustomCACerts() = default;

  SSLUITestCustomCACerts(const SSLUITestCustomCACerts&) = delete;
  SSLUITestCustomCACerts& operator=(const SSLUITestCustomCACerts&) = delete;

  ~SSLUITestCustomCACerts() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SSLUITestNoCert::SetUpCommandLine(command_line);
    // Don't require policy for our sessions - this is required so the policy
    // code knows not to expect cached policy for the secondary profile.
    command_line->AppendSwitchASCII(ash::switches::kProfileRequiresPolicy,
                                    "false");
  }

  void SetUpOnMainThread() override {
    SSLUITestNoCert::SetUpOnMainThread();

    profile_1_ = browser()->profile();

    // Create a second profile.
    {
      static const char kSecondProfileAccount[] = "profile2@test.com";
      static const char kSecondProfileGaiaId[] = "9876543210";
      static const char kSecondProfileHash[] = "testProfile2";

      ON_CALL(policy_for_profile_2_, IsInitializationComplete(testing::_))
          .WillByDefault(testing::Return(true));
      ON_CALL(policy_for_profile_2_, IsFirstPolicyLoadComplete(testing::_))
          .WillByDefault(testing::Return(true));
      policy::PushProfilePolicyConnectorProviderForTesting(
          &policy_for_profile_2_);

      base::FilePath user_data_directory;
      base::PathService::Get(chrome::DIR_USER_DATA, &user_data_directory);
      session_manager::SessionManager::Get()->CreateSession(
          AccountId::FromUserEmailGaiaId(kSecondProfileAccount,
                                         kSecondProfileGaiaId),
          kSecondProfileHash, false);
      // Set up the secondary profile.
      base::FilePath profile_dir = user_data_directory.Append(
          ash::ProfileHelper::GetUserProfileDir(kSecondProfileHash).BaseName());
      profile_2_ =
          g_browser_process->profile_manager()->GetProfile(profile_dir);
    }

    // Get cert databases for both profiles.
    {
      base::RunLoop loop;
      NssServiceFactory::GetForContext(profile_1_)
          ->UnsafelyGetNSSCertDatabaseForTesting(base::BindOnce(
              &SSLUITestCustomCACerts::DidGetCertDatabase,
              base::Unretained(this), &loop, &profile_1_cert_db_));
      loop.Run();
    }

    {
      base::RunLoop loop;
      NssServiceFactory::GetForContext(profile_2_)
          ->UnsafelyGetNSSCertDatabaseForTesting(base::BindOnce(
              &SSLUITestCustomCACerts::DidGetCertDatabase,
              base::Unretained(this), &loop, &profile_2_cert_db_));
      loop.Run();
    }

    // Double-check that the profile initialization was correct and the two
    // profiles have distinct NSS databases with distinc NSS public slots.
    EXPECT_NE(profile_1_cert_db_, profile_2_cert_db_);
    EXPECT_NE(profile_1_cert_db_->GetPublicSlot().get(),
              profile_2_cert_db_->GetPublicSlot().get());
  }

  void TearDownOnMainThread() override {
    profile_1_cert_db_ = nullptr;
    profile_2_cert_db_ = nullptr;
    profile_1_ = nullptr;
    profile_2_ = nullptr;
  }

 protected:
  void ImportCACertAsTrusted(const std::string& cert_file_name,
                             net::NSSCertDatabase* cert_db) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    net::ScopedCERTCertificateList ca_cert_list =
        net::CreateCERTCertificateListFromFile(
            net::GetTestCertsDirectory(), cert_file_name,
            net::X509Certificate::FORMAT_AUTO);
    ASSERT_FALSE(ca_cert_list.empty());
    net::NSSCertDatabase::ImportCertFailureList failures;
    ASSERT_TRUE(cert_db->ImportCACerts(
        ca_cert_list, net::NSSCertDatabase::TRUSTED_SSL, &failures));
    ASSERT_TRUE(failures.empty());
  }

  // The first profile.
  raw_ptr<Profile> profile_1_;
  // The second profile.
  raw_ptr<Profile> profile_2_;

  // The NSSCertDatabase for |profile_1_|.
  raw_ptr<net::NSSCertDatabase> profile_1_cert_db_;

  // The NSSCertDatabase for |profile_2_|.
  raw_ptr<net::NSSCertDatabase> profile_2_cert_db_;

  // Policy provider for |profile_2_|. Overrides any other policy providers.
  testing::NiceMock<policy::MockConfigurationPolicyProvider>
      policy_for_profile_2_;

 private:
  void DidGetCertDatabase(base::RunLoop* loop,
                          raw_ptr<net::NSSCertDatabase>* out_cert_db,
                          net::NSSCertDatabase* cert_db) {
    *out_cert_db = cert_db;
    loop->Quit();
  }
};

// Imports a trusted CA certiifcate into a profile's NSS database.
// Verifies that the certificate is trusted in the context of the profile it was
// imported for.
// Verifies that the certificate is *not* trusted in the context of a different
// profile.
IN_PROC_BROWSER_TEST_F(SSLUITestCustomCACerts,
                       TrustedCertOnlyRespectedInProfileThatOwnsIt) {
  ASSERT_TRUE(https_server_.Start());

  ASSERT_NO_FATAL_FAILURE(
      ImportCACertAsTrusted("root_ca_cert.pem", profile_2_cert_db_));

  // Flush the network service instance so persistent NSS Database changes are
  // reflected in the network service.
  content::FlushNetworkServiceInstanceForTesting();

  // The certificate that is trusted in |profile_2_| should not be respected in
  // browsers that belong to |profile_1_|.
  Browser* browser_for_profile_1 = CreateBrowser(profile_1_);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser_for_profile_1, https_server_.GetURL("/ssl/google.html")));
  WebContents* tab_for_profile_1 =
      browser_for_profile_1->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingInterstitial(tab_for_profile_1));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab_for_profile_1, net::CERT_STATUS_AUTHORITY_INVALID,
      AuthState::SHOWING_INTERSTITIAL);

  // The certificate that is trusted in |profile_2_| should be respected in
  // browsers that belong to |profile_2_|.
  Browser* browser_for_profile_2 = CreateBrowser(profile_2_);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser_for_profile_2, https_server_.GetURL("/ssl/google.html")));
  WebContents* tab_for_profile_2 =
      browser_for_profile_2->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab_for_profile_2, AuthState::NONE);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Regression test for http://crbug.com/635833 (crash when a window with no
// NavigationEntry commits).
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreLocalhostCertErrors,
                       NoCrashOnLoadWithNoNavigationEntry) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html")));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(tab, "window.open()"));
}

std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
MakeCaptivePortalConfig(int version_id,
                        const std::set<std::string>& spki_hashes) {
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(version_id);
  for (const std::string& hash : spki_hashes) {
    config_proto->add_captive_portal_cert()->set_sha256_hash(hash);
  }
  return config_proto;
}

// Tests the scenario where the OS reports a captive portal. A captive portal
// interstitial should be displayed. The test then switches OS captive portal
// status to false and reloads the page. This time, a normal SSL interstitial
// will be displayed.
IN_PROC_BROWSER_TEST_F(SSLUITest, OSReportsCaptivePortal) {
  ASSERT_TRUE(https_server_mismatched_.Start());
  base::HistogramTester histograms;
  bool netwok_connectivity_reported = false;
  SSLErrorHandler::SetOSReportsCaptivePortalForTesting(true);
  SSLErrorHandler::SetReportNetworkConnectivityCallbackForTesting(
      base::BindLambdaForTesting([&]() {
        SSLErrorHandler::SetOSReportsCaptivePortalForTesting(false);
        netwok_connectivity_reported = true;
      }));

  // Navigate to an unsafe page on the server.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html")));

  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingCaptivePortalInterstitial(tab));
  EXPECT_FALSE(interstitial_timer_observer.timer_started());

  // Check that the histogram for the captive portal cert was recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::OS_REPORTS_CAPTIVE_PORTAL, 1);

  // Reload the URL. This time the OS should not report a captive portal.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html")));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  EXPECT_TRUE(netwok_connectivity_reported);
}

class SSLUITestWithCaptivePortalInterstitialDisabled : public SSLUITest {
 public:
  SSLUITestWithCaptivePortalInterstitialDisabled() {
    feature_list_.InitWithFeatures({} /* enabled */,
                                   {kCaptivePortalInterstitial} /* disabled */);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests the scenario where the OS reports a captive portal but captive portal
// interstitial feature is disabled. A captive portal interstitial should not be
// displayed.
IN_PROC_BROWSER_TEST_F(SSLUITestWithCaptivePortalInterstitialDisabled,
                       OSReportsCaptivePortal_FeatureDisabled) {
  ASSERT_TRUE(https_server_mismatched_.Start());
  base::HistogramTester histograms;

  SSLErrorHandler::SetOSReportsCaptivePortalForTesting(true);

  // Navigate to an unsafe page on the server.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html")));

  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  EXPECT_FALSE(interstitial_timer_observer.timer_started());

  // Check that the histogram for the SSL interstitial was recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 0);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::OS_REPORTS_CAPTIVE_PORTAL, 0);
}

// Tests that the committed interstitial flag triggers the code path to show an
// error PageType instead of an interstitial PageType.
IN_PROC_BROWSER_TEST_F(SSLUITest, ErrorPage) {
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));

  ssl_test_util::CheckSecurityState(tab, net::CERT_STATUS_DATE_INVALID,
                                    security_state::DANGEROUS,
                                    AuthState::SHOWING_ERROR);

  content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
  EXPECT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());
}

using security_interstitials::InsecureFormNavigationThrottle;

// Visits a page that displays an insecure form inside an iframe, attempts to
// submit the form, and checks an interstitial is not shown (submissions of
// mixed forms inside iframes are separately blocked, and that behavior is
// tested in mixed_content_navigation_throttle_unittest.cc).
IN_PROC_BROWSER_TEST_F(
    SSLUITest,
    TestDoesNotDisplayInsecureFormSubmissionWarningInIframe) {
  ChromeContentBrowserClientForMixedContentTest browser_client;
  browser_client.SetMixedContentSettings(
      false, /* allow_running_insecure_content */
      false, /* strict_mixed_content_checking */
      false /*strictly_block_blockable_mixed_content */);
  content::ScopedContentBrowserClientSetting setting(&browser_client);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  tab->OnWebPreferencesChanged();
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_form_in_iframe.html",
      https_server_.host_port_pair());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  content::TestNavigationObserver nav_observer(tab, 1);
  content::WebContentsConsoleObserver console_observer(tab);
  console_observer.SetPattern(
      "Mixed Content: The page at * was loaded over a secure connection, but "
      "contains a form that targets an insecure endpoint "
      "'http://does-not-exist.test/ssl/google_files/logo.gif'. This endpoint "
      "should be made available over a secure connection.");
  ASSERT_TRUE(content::ExecJs(tab, "submitForm();"));
  nav_observer.Wait();

  // We shouldn't be displaying an interstitial.
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  EXPECT_FALSE(helper);

  // Check console message was printed.
  EXPECT_EQ(console_observer.messages().size(), 1u);
}

// Checks insecure form warning works for forms that submit on a new tab.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       TestDisplaysInsecureFormSubmissionWarningTargetBlank) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_form_target_blank.html",
      embedded_test_server()->host_port_pair());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(tab, 1);
  nav_observer.StartWatchingNewWebContents();
  ASSERT_TRUE(content::ExecJs(tab, "submitForm();"));
  nav_observer.Wait();
  tab = browser()->tab_strip_model()->GetActiveWebContents();
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  EXPECT_TRUE(helper->IsDisplayingInterstitial());
  EXPECT_EQ(helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
                ->GetTypeForTesting(),
            security_interstitials::InsecureFormBlockingPage::kTypeForTesting);
}

// Checks reloading the interstitial is not treated as proceeding on a POST
// form.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       TestReloadInsecureFormSubmissionWarningIsNotProceed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_form.html",
      embedded_test_server()->host_port_pair());

  // Navigate to an insecure form, make sure we get a warning.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(tab, 1);
  ASSERT_TRUE(content::ExecJs(tab, "submitForm();"));
  nav_observer.Wait();
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  EXPECT_TRUE(helper->IsDisplayingInterstitial());
  EXPECT_EQ(helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
                ->GetTypeForTesting(),
            security_interstitials::InsecureFormBlockingPage::kTypeForTesting);
  // Reload the interstitial.
  content::TestNavigationObserver reload_observer(tab, 1);
  tab->GetController().Reload(content::ReloadType::NORMAL, false);
  reload_observer.Wait();
  // Check we get another interstitial.
  helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  EXPECT_TRUE(helper->IsDisplayingInterstitial());
  EXPECT_EQ(helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
                ->GetTypeForTesting(),
            security_interstitials::InsecureFormBlockingPage::kTypeForTesting);
}

// Checks reloading the interstitial is not treated as proceeding on a GET form.
IN_PROC_BROWSER_TEST_F(
    SSLUITest,
    TestReloadInsecureFormSubmissionWarningIsNotProceedGetForm) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_form_get.html",
      embedded_test_server()->host_port_pair());

  // Navigate to an insecure form that uses the GET method, make sure we get a
  // warning.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(tab, 1);
  ASSERT_TRUE(content::ExecJs(tab, "submitForm();"));
  nav_observer.Wait();
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  EXPECT_TRUE(helper->IsDisplayingInterstitial());
  EXPECT_EQ(helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
                ->GetTypeForTesting(),
            security_interstitials::InsecureFormBlockingPage::kTypeForTesting);
  // Reload the interstitial.
  content::TestNavigationObserver reload_observer(tab, 1);
  tab->GetController().Reload(content::ReloadType::NORMAL, false);
  reload_observer.Wait();
  // Check we get another interstitial.
  helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  EXPECT_TRUE(helper->IsDisplayingInterstitial());
  EXPECT_EQ(helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
                ->GetTypeForTesting(),
            security_interstitials::InsecureFormBlockingPage::kTypeForTesting);
}

// Checks navigating back and forward from the interstitial is not treated as
// proceeding on a GET form.
IN_PROC_BROWSER_TEST_F(
    SSLUITest,
    TestBackForwardOnInsecureFormSubmissionWarningIsNotProceedGetForm) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_form_get.html",
      embedded_test_server()->host_port_pair());

  // Navigate to an insecure form that uses the GET method, make sure we get a
  // warning.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(tab, 1);
  ASSERT_TRUE(content::ExecJs(tab, "submitForm();"));
  nav_observer.Wait();
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  EXPECT_TRUE(helper->IsDisplayingInterstitial());
  EXPECT_EQ(helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
                ->GetTypeForTesting(),
            security_interstitials::InsecureFormBlockingPage::kTypeForTesting);
  // Navigate back, then forward.
  content::TestNavigationObserver back_observer(tab, 1);
  tab->GetController().GoBack();
  back_observer.Wait();
  content::TestNavigationObserver forward_observer(tab, 1);
  tab->GetController().GoForward();
  forward_observer.Wait();
  // Check we get another interstitial.
  helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  // Currently the interstitial is only displayed when back/forward cache is
  // enabled, so return early when the feature is disabled.
  // TODO(crbug.com/40243001): Fix this.
  if (!content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    EXPECT_FALSE(helper->IsDisplayingInterstitial());
    return;
  }
  EXPECT_TRUE(helper->IsDisplayingInterstitial());
  EXPECT_EQ(helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
                ->GetTypeForTesting(),
            security_interstitials::InsecureFormBlockingPage::kTypeForTesting);
}

// Check proceed works correctly on insecure form warning.
IN_PROC_BROWSER_TEST_F(SSLUITest, ProceedThroughInsecureFormWarning) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_form.html",
      embedded_test_server()->host_port_pair());
  GURL form_target_url("http://does-not-exist.test/ssl/google_files/logo.gif");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(tab, 1);
  ASSERT_TRUE(content::ExecJs(tab, "submitForm();"));
  nav_observer.Wait();
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  EXPECT_TRUE(helper->IsDisplayingInterstitial());
  EXPECT_EQ(helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
                ->GetTypeForTesting(),
            security_interstitials::InsecureFormBlockingPage::kTypeForTesting);
  // After clicking Proceed, we should not be on an interstitial, and be
  // on the form target url;
  ProceedThroughInterstitial(tab);
  EXPECT_FALSE(helper->IsDisplayingInterstitial());
  EXPECT_EQ(tab->GetVisibleURL(), form_target_url);
}

// Check don't proceed works correctly on insecure form warning.
IN_PROC_BROWSER_TEST_F(SSLUITest, GoBackFromInsecureFormWarning) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_form.html",
      embedded_test_server()->host_port_pair());
  GURL form_site_url = https_server_.GetURL(replacement_path);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), form_site_url));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(tab, 1);
  ASSERT_TRUE(content::ExecJs(tab, "submitForm();"));
  nav_observer.Wait();
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  EXPECT_TRUE(helper->IsDisplayingInterstitial());
  EXPECT_EQ(helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
                ->GetTypeForTesting(),
            security_interstitials::InsecureFormBlockingPage::kTypeForTesting);
  // After clicking Don't Proceed, we should not be on an interstitial, and be
  // back on the site containing the insecure form.
  DontProceedThroughInterstitial(tab);
  EXPECT_FALSE(helper->IsDisplayingInterstitial());
  EXPECT_EQ(tab->GetVisibleURL(), form_site_url);
}

// Checks mixed form warnings work correctly for non-redirects.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysInsecureFormSubmissionWarning) {
  base::HistogramTester histograms;
  const std::string interstitial_histogram =
      "Security.MixedForm.InterstitialTriggerState";
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_form.html",
      embedded_test_server()->host_port_pair());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(tab, 1);
  ASSERT_TRUE(content::ExecJs(tab, "submitForm();"));
  nav_observer.Wait();
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  EXPECT_TRUE(helper->IsDisplayingInterstitial());
  EXPECT_EQ(helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
                ->GetTypeForTesting(),
            security_interstitials::InsecureFormBlockingPage::kTypeForTesting);

  // Check this was logged correctly as a non-redirect interstitial.
  histograms.ExpectTotalCount(interstitial_histogram, 1);
  histograms.ExpectBucketCount(interstitial_histogram,
                               InsecureFormNavigationThrottle::
                                   InterstitialTriggeredState::kMixedFormDirect,
                               1);
}

// Checks interstitial is shown for mixed forms caused by a 307 POST http
// redirect, and that metrics are logged.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       TestDisplaysInsecureFormSubmissionWarningRedirect) {
  base::HistogramTester histograms;
  const std::string interstitial_histogram =
      "Security.MixedForm.InterstitialTriggerState";
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_form_redirects_insecure.html",
      embedded_test_server()->host_port_pair());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(tab, 1);
  ASSERT_TRUE(content::ExecJs(tab, "submitForm();"));
  nav_observer.Wait();
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  EXPECT_TRUE(helper->IsDisplayingInterstitial());
  EXPECT_EQ(helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
                ->GetTypeForTesting(),
            security_interstitials::InsecureFormBlockingPage::kTypeForTesting);

  // Check this was logged correctly as a redirect mixed form that may expose
  // form data.
  histograms.ExpectTotalCount(interstitial_histogram, 1);
  histograms.ExpectBucketCount(
      interstitial_histogram,
      InsecureFormNavigationThrottle::InterstitialTriggeredState::
          kMixedFormRedirectWithFormData,
      1);
}

// Checks interstitial is shown for mixed forms caused by a 308 POST http
// redirect, and that metrics are logged.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       TestDisplaysInsecureFormSubmissionWarningRedirect308) {
  base::HistogramTester histograms;
  const std::string interstitial_histogram =
      "Security.MixedForm.InterstitialTriggerState";
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_form_redirects_308_insecure.html",
      embedded_test_server()->host_port_pair());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(tab, 1);
  ASSERT_TRUE(content::ExecJs(tab, "submitForm();"));
  nav_observer.Wait();
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  EXPECT_TRUE(helper->IsDisplayingInterstitial());
  EXPECT_EQ(helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
                ->GetTypeForTesting(),
            security_interstitials::InsecureFormBlockingPage::kTypeForTesting);

  // Check this was logged correctly as a redirect mixed form that may expose
  // form data.
  histograms.ExpectTotalCount(interstitial_histogram, 1);
  histograms.ExpectBucketCount(
      interstitial_histogram,
      InsecureFormNavigationThrottle::InterstitialTriggeredState::
          kMixedFormRedirectWithFormData,
      1);
}

// Checks no interstitial is shown for mixed forms caused for a POST form with a
// 301 redirect, and that metrics are logged correctly.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       TestDisplaysInsecureFormSubmissionWarningRedirect301) {
  base::HistogramTester histograms;
  const std::string interstitial_histogram =
      "Security.MixedForm.InterstitialTriggerState";
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  // This test posts to does-not-exist.test. Disable HTTPS upgrades on this
  // hostname for the test to work.
  // TODO(crbug.com/40248833): Remove the allowlist entry.
  ScopedAllowHttpForHostnamesForTesting scoped_allow_http(
      {"does-not-exist.test"}, browser()->profile()->GetPrefs());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_form_redirects_301_insecure.html",
      embedded_test_server()->host_port_pair());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(tab, 1);
  ASSERT_TRUE(content::ExecJs(tab, "submitForm();"));
  nav_observer.Wait();

  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  // There should have been no interstitial triggered.
  EXPECT_FALSE(helper);

  // Check this was logged correctly as a redirect mixed form that would not
  // expose form data.
  histograms.ExpectTotalCount(interstitial_histogram, 1);
  histograms.ExpectBucketCount(
      interstitial_histogram,
      InsecureFormNavigationThrottle::InterstitialTriggeredState::
          kMixedFormRedirectNoFormData,
      1);
}

// Checks no interstitial is shown for mixed forms caused for a POST form with a
// 302 redirect, and that metrics are logged correctly.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       TestDisplaysInsecureFormSubmissionWarningRedirect302) {
  base::HistogramTester histograms;
  const std::string interstitial_histogram =
      "Security.MixedForm.InterstitialTriggerState";
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  // This test posts to does-not-exist.test. Disable HTTPS upgrades on this
  // hostname for the test to work.
  // TODO(crbug.com/40248833): Remove the allowlist entry.
  ScopedAllowHttpForHostnamesForTesting scoped_allow_http(
      {"does-not-exist.test"}, browser()->profile()->GetPrefs());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_form_redirects_302_insecure.html",
      embedded_test_server()->host_port_pair());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(tab, 1);
  ASSERT_TRUE(content::ExecJs(tab, "submitForm();"));
  nav_observer.Wait();
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  // There should have been no interstitial triggered.
  EXPECT_FALSE(helper);

  // Check this was logged correctly as a redirect mixed form that would not
  // expose form data.
  histograms.ExpectTotalCount(interstitial_histogram, 1);
  histograms.ExpectBucketCount(
      interstitial_histogram,
      InsecureFormNavigationThrottle::InterstitialTriggeredState::
          kMixedFormRedirectNoFormData,
      1);
}

namespace {
// Redirects (with 307 code) requests with a redirect_to_http path to
// http://example.org. This custom handler is required for tests that include
// GET method forms to the redirect URL, since the built in /server-redirect-307
// handler takes the redirect-to URL as a query parameter, so it is not usable
// for GET method forms.
std::unique_ptr<net::test_server::HttpResponse> FormActionHTTPRedirectHandler(
    const net::EmbeddedTestServer* test_server,
    const net::test_server::HttpRequest& request) {
  GURL absolute_url = test_server->GetURL(request.relative_url);
  if (absolute_url.path() != "/redirect_to_http")
    return nullptr;
  GURL::Replacements replacements;
  replacements.SetHostStr("example.org");
  replacements.SetSchemeStr("http");
  const GURL redirect_url =
      test_server->base_url().ReplaceComponents(replacements);

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url.spec());
  return std::move(http_response);
}
}  // namespace

// Checks no interstitial is shown for mixed forms caused for a GET form with
// a 307 redirect to http, and that metrics are logged correctly.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       TestDisplaysInsecureFormSubmissionWarningRedirectGet) {
  base::HistogramTester histograms;
  const std::string interstitial_histogram =
      "Security.MixedForm.InterstitialTriggerState";
  ASSERT_TRUE(embedded_test_server()->Start());
  https_server_.RegisterRequestHandler(
      base::BindRepeating(&FormActionHTTPRedirectHandler, &https_server_));
  ASSERT_TRUE(https_server_.Start());

  // This test redirects to example.org. Disable HTTPS upgrades on this
  // hostname for the test to work.
  // TODO(crbug.com/40248833): Remove the allowlist entry.
  ScopedAllowHttpForHostnamesForTesting scoped_allow_http(
      {"example.org"}, browser()->profile()->GetPrefs());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_form_redirects_insecure_get.html",
      embedded_test_server()->host_port_pair());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(tab, 1);
  ASSERT_TRUE(content::ExecJs(tab, "submitForm();"));
  nav_observer.Wait();
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  // There should have been no interstitial triggered.
  EXPECT_FALSE(helper);

  // Check this was logged correctly as a redirect mixed form that would not
  // expose form data.
  histograms.ExpectTotalCount(interstitial_histogram, 1);
  histograms.ExpectBucketCount(
      interstitial_histogram,
      InsecureFormNavigationThrottle::InterstitialTriggeredState::
          kMixedFormRedirectNoFormData,
      1);
}

class MixedFormsPolicyTest : public policy::PolicyTest {};

// Check no warning is shown if disabled by policy.
IN_PROC_BROWSER_TEST_F(MixedFormsPolicyTest, NoWarningOptOutPolicy) {
  ASSERT_TRUE(embedded_test_server()->Start());
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  // Check pref is set to true by default.
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kMixedFormsWarningsEnabled));
  // Set policy to disable mixed form warnings.
  policy::PolicyMap policies;
  policies.Set(policy::key::kInsecureFormsWarningsEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  UpdateProviderPolicy(policies);
  // Pref should now be set to false.
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kMixedFormsWarningsEnabled));

  std::string replacement_path =
      SSLUITestBase::GetFilePathWithHostAndPortReplacement(
          "/ssl/page_displays_insecure_form.html",
          embedded_test_server()->host_port_pair());
  GURL form_site_url = https_server.GetURL(replacement_path);
  GURL form_target_url("http://does-not-exist.test/ssl/google_files/logo.gif");

  // Navigate to site with insecure form and submit it.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), form_site_url));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(tab, 1);
  ASSERT_TRUE(content::ExecJs(tab, "submitForm();"));
  nav_observer.Wait();

  // No interstitial should be shown, and we should be in the form action URL.
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  EXPECT_TRUE(!helper || !helper->IsDisplayingInterstitial());
  EXPECT_EQ(tab->GetVisibleURL(), form_target_url);
}

namespace {

char kTestMITMSoftwareName[] = "Misconfigured Firewall";
char16_t kTestMITMSoftwareName16[] = u"Misconfigured Firewall";

class SSLUIMITMSoftwareTest : public CertVerifierBrowserTest {
 public:
  SSLUIMITMSoftwareTest()
      : CertVerifierBrowserTest(),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  SSLUIMITMSoftwareTest(const SSLUIMITMSoftwareTest&) = delete;
  SSLUIMITMSoftwareTest& operator=(const SSLUIMITMSoftwareTest&) = delete;

  ~SSLUIMITMSoftwareTest() override {}

  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ssl_test_util::SetHSTSForHostName(browser()->profile(), kHstsTestHostName);
  }

  // Set up the cert verifier to return the error passed in as the cert_error
  // parameter.
  void SetUpCertVerifier(net::CertStatus cert_error) {
    net::CertVerifyResult verify_result;
    verify_result.verified_cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(verify_result.verified_cert);

    verify_result.cert_status = cert_error;
    mock_cert_verifier()->AddResultForCert(
        https_server()->GetCertificate().get(), verify_result,
        net::MapCertStatusToNetError(cert_error));
  }

  // Sets up an SSLErrorAssistantProto that lists |https_server_|'s default
  // certificate as a MITM software certificate.
  void SetUpMITMSoftwareCertList(uint32_t version_id) {
    auto config_proto =
        std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
    config_proto->set_version_id(version_id);

    chrome_browser_ssl::MITMSoftware* mitm_software =
        config_proto->add_mitm_software();
    mitm_software->set_name(kTestMITMSoftwareName);
    mitm_software->set_issuer_common_name_regex(
        https_server()->GetCertificate().get()->issuer().common_name);
    mitm_software->set_issuer_organization_regex(
        https_server()->GetCertificate().get()->issuer().organization_names[0]);
    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() >
                0);
  }

  // Returns a URL which triggers an interstitial with the host name that has
  // HSTS set.
  GURL GetHSTSTestURL() const {
    GURL::Replacements replacements;
    replacements.SetHostStr(kHstsTestHostName);
    return https_server()
        ->GetURL("/ssl/blank_page.html")
        .ReplaceComponents(replacements);
  }

  void TestMITMSoftwareInterstitial() {
    base::HistogramTester histograms;
    ASSERT_TRUE(https_server()->Start());
    ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() >
                0);

    // Navigate to an unsafe page on the server. Mock out the URL host name to
    // equal the one set for HSTS.
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    SSLInterstitialTimerObserver interstitial_timer_observer(tab);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetHSTSTestURL()));
    ASSERT_TRUE(chrome_browser_interstitials::IsShowingMITMInterstitial(tab));
    EXPECT_FALSE(interstitial_timer_observer.timer_started());

    // Check that the histograms for the MITM software interstitial were
    // recorded.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 0);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL, 1);
  }

  void TestNoMITMSoftwareInterstitial() {
    base::HistogramTester histograms;
    ASSERT_TRUE(https_server()->Start());
    ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() >
                0);

    // Navigate to an unsafe page on the server. Mock out the URL host name to
    // equal the one set for HSTS.
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    SSLInterstitialTimerObserver interstitial_timer_observer(tab);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetHSTSTestURL()));
    ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
    EXPECT_TRUE(interstitial_timer_observer.timer_started());

    // Check that a MITM software interstitial was not recorded in histogram.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 0);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL, 0);
  }

  // Returns the https server. Guaranteed to be non-NULL.
  const net::EmbeddedTestServer* https_server() const { return &https_server_; }
  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  net::EmbeddedTestServer https_server_;
};

// The SSLUIMITMSoftwareEnabled and Disabled test classes exist so that the
// scoped feature list can be instantiated in the set up method of the class
// rather than in the test itself. Bug crbug.com/713390 was causing some of the
// tests in SSLUIMITMSoftwareTest to be flaky. Refactoring these tests so that
// the scoped feature list initialization is done in the set up method fixes
// this flakiness.

class SSLUIMITMSoftwareEnabledTest : public SSLUIMITMSoftwareTest {
 public:
  SSLUIMITMSoftwareEnabledTest() {
    scoped_feature_list_.InitWithFeatures(
        {kMITMSoftwareInterstitial} /* enabled */, {} /* disabled */);
  }

  SSLUIMITMSoftwareEnabledTest(const SSLUIMITMSoftwareEnabledTest&) = delete;
  SSLUIMITMSoftwareEnabledTest& operator=(const SSLUIMITMSoftwareEnabledTest&) =
      delete;

  ~SSLUIMITMSoftwareEnabledTest() override {}

  void SetUpOnMainThread() override {
    SSLUIMITMSoftwareTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class SSLUIMITMSoftwareDisabledTest : public SSLUIMITMSoftwareTest {
 public:
  SSLUIMITMSoftwareDisabledTest() {
    scoped_feature_list_.InitWithFeatures(
        {} /* enabled */, {kMITMSoftwareInterstitial} /* disabled */);
  }

  SSLUIMITMSoftwareDisabledTest(const SSLUIMITMSoftwareDisabledTest&) = delete;
  SSLUIMITMSoftwareDisabledTest& operator=(
      const SSLUIMITMSoftwareDisabledTest&) = delete;

  ~SSLUIMITMSoftwareDisabledTest() override {}

  void SetUpOnMainThread() override {
    SSLUIMITMSoftwareTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// Tests that the MITM software interstitial is not displayed when the feature
// is disabled by Finch.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareDisabledTest, DisabledWithFinch) {
  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestNoMITMSoftwareInterstitial();
}

// Tests that the MITM software interstitial is displayed when the feature is
// enabled by Finch.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest, EnabledWithFinch) {
  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestMITMSoftwareInterstitial();
}

// Tests that if a certificates matches the common name of a known MITM software
// cert on the list but not the organization name, the MITM software
// interstitial will not be displayed.
IN_PROC_BROWSER_TEST_F(
    SSLUIMITMSoftwareEnabledTest,
    CertificateCommonNameMatchOnly_NoMITMSoftwareInterstitial) {
  base::HistogramTester histograms;

  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  ASSERT_TRUE(https_server()->Start());

  // Set up an error assistant proto with a list of MITM software regexed that
  // the certificate issued by our server won't match.
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);
  chrome_browser_ssl::MITMSoftware* mitm_software =
      config_proto->add_mitm_software();
  mitm_software->set_name(kTestMITMSoftwareName);
  mitm_software->set_issuer_common_name_regex(
      https_server()->GetCertificate().get()->issuer().common_name);
  mitm_software->set_issuer_organization_regex(
      "pattern-that-does-not-match-anything");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
            kLargeVersionId);

  // Navigate to an unsafe page on the server. Mock out the URL host name to
  // equal the one set for HSTS.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetHSTSTestURL()));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  EXPECT_TRUE(interstitial_timer_observer.timer_started());

  // Check that a MITM software interstitial was not recorded in histogram.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL,
                               0);
}

// Tests that if a certificates matches the organization name of a known MITM
// software cert on the list but not the common name, the MITM software
// interstitial will not be displayed.
IN_PROC_BROWSER_TEST_F(
    SSLUIMITMSoftwareEnabledTest,
    CertificateOrganizationMatchOnly_NoMITMSoftwareInterstitial) {
  base::HistogramTester histograms;

  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  ASSERT_TRUE(https_server()->Start());

  // Set up an error assistant proto with a list of MITM software regexed that
  // the certificate issued by our server won't match.
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);
  chrome_browser_ssl::MITMSoftware* mitm_software =
      config_proto->add_mitm_software();
  mitm_software->set_name(kTestMITMSoftwareName);
  mitm_software->set_issuer_common_name_regex(
      "pattern-that-does-not-match-anything");
  mitm_software->set_issuer_organization_regex(
      https_server()->GetCertificate().get()->issuer().organization_names[0]);
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
            kLargeVersionId);

  // Navigate to an unsafe page on the server. Mock out the URL host name to
  // equal the one set for HSTS.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetHSTSTestURL()));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  EXPECT_TRUE(interstitial_timer_observer.timer_started());

  // Check that a MITM software interstitial was not recorded in histogram.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL,
                               0);
}

// Tests that if the certificate does not match any entry on the list of known
// MITM software, the MITM software interstitial will not be displayed.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest,
                       NonMatchingCertificate_NoMITMSoftwareInterstitial) {
  base::HistogramTester histograms;

  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  ASSERT_TRUE(https_server()->Start());

  // Set up an error assistant proto with a list of MITM software regexes that
  // the certificate issued by our server won't match.
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);
  chrome_browser_ssl::MITMSoftware* mitm_software =
      config_proto->add_mitm_software();
  mitm_software->set_name("Non-Matching MITM Software");
  mitm_software->set_issuer_common_name_regex(
      "pattern-that-does-not-match-anything");
  mitm_software->set_issuer_organization_regex(
      "pattern-that-does-not-match-anything");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
            kLargeVersionId);

  // Navigate to an unsafe page on the server. Mock out the URL host name to
  // equal the one set for HSTS.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetHSTSTestURL()));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  EXPECT_TRUE(interstitial_timer_observer.timer_started());

  // Check that a MITM software interstitial was not recorded in histogram.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL,
                               0);
}

// Tests that if there is more than one error on the certificate the MITM
// software interstitial will not be displayed.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest,
                       TwoCertErrors_NoMITMSoftwareInterstitial) {
  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID |
                    net::CERT_STATUS_COMMON_NAME_INVALID);
  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestNoMITMSoftwareInterstitial();
}

// Tests that a certificate error other than |CERT_STATUS_AUTHORITY_INVALID|
// will not trigger the MITM software interstitial.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest,
                       WrongCertError_NoMITMSoftwareInterstitial) {
  SetUpCertVerifier(net::CERT_STATUS_COMMON_NAME_INVALID);
  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestNoMITMSoftwareInterstitial();
}

// Tests that if the error on the certificate served is overridable the MITM
// software interstitial will not be displayed.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest,
                       OverridableError_NoMITMSoftwareInterstitial) {
  base::HistogramTester histograms;

  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);

  ASSERT_TRUE(https_server()->Start());
  SetUpMITMSoftwareCertList(kLargeVersionId);

  // Navigate to an unsafe page to trigger an interstitial, but don't replace
  // the host name with the one set for HSTS.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("/ssl/blank_page.html")));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  EXPECT_TRUE(interstitial_timer_observer.timer_started());

  // Check that the histogram for an overridable SSL interstitial was
  // recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL,
                               0);
}

// Tests that the correct strings are displayed on the interstitial in the
// enterprise managed case.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest, EnterpriseManaged) {
  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  ChromeSecurityBlockingPageFactory::SetEnterpriseManagedForTesting(true);

  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestMITMSoftwareInterstitial();

  const std::string expected_primary_paragraph =
      l10n_util::GetStringFUTF8(IDS_MITM_SOFTWARE_PRIMARY_PARAGRAPH_ENTERPRISE,
                                base::EscapeForHTML(kTestMITMSoftwareName16));
  const std::string expected_explanation = l10n_util::GetStringFUTF8(
      IDS_MITM_SOFTWARE_EXPLANATION_ENTERPRISE,
      base::EscapeForHTML(kTestMITMSoftwareName16),
      l10n_util::GetStringUTF16(IDS_MITM_SOFTWARE_EXPLANATION));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetPrimaryMainFrame(), expected_explanation));
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetPrimaryMainFrame(), expected_primary_paragraph));
}

// Tests that the correct strings are displayed on the interstitial in the
// non-enterprise managed case.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest, NotEnterpriseManaged) {
  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  ChromeSecurityBlockingPageFactory::SetEnterpriseManagedForTesting(false);

  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestMITMSoftwareInterstitial();

  // Don't check the primary paragraph in the non-enterprise case, because it
  // has escaped HTML characters which throw an error.
  const std::string expected_explanation = l10n_util::GetStringFUTF8(
      IDS_MITM_SOFTWARE_EXPLANATION_NONENTERPRISE,
      base::EscapeForHTML(kTestMITMSoftwareName16),
      l10n_util::GetStringUTF16(IDS_MITM_SOFTWARE_EXPLANATION));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetPrimaryMainFrame(), expected_explanation));
}

// Initialize MITMSoftware certificate list but set the version_id to zero. This
// less than the version_id of the local resource bundle, so the dynamic
// update will be ignored and a non-MITM interstitial will be shown.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest,
                       IgnoreDynamicUpdateWithSmallVersionId) {
  auto config_proto =
      SSLErrorAssistant::GetErrorAssistantProtoFromResourceBundle();
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));

  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  ChromeSecurityBlockingPageFactory::SetEnterpriseManagedForTesting(false);

  SetUpMITMSoftwareCertList(0u);
  TestNoMITMSoftwareInterstitial();
}

// Checks that SimpleURLLoader, which uses services/network/url_loader.cc, goes
// through the new NetworkServiceClient interface to deliver cert error
// notifications to the browser which then overrides the certificate error.
IN_PROC_BROWSER_TEST_F(SSLUITest, SimpleURLLoaderCertError) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(SetUpUnsafeContentsWithUserException(
      "/ssl/page_with_unsafe_contents.html"));
  ssl_test_util::CheckAuthenticationBrokenState(tab, CertError::NONE,
                                                AuthState::NONE);

  EXPECT_EQ(net::OK,
            content::LoadBasicRequest(
                tab->GetPrimaryMainFrame(),
                https_server_mismatched_.GetURL("/anchor_download_test.png")));
}

IN_PROC_BROWSER_TEST_F(SSLUITest, NetworkErrorDoesntRevokeExemptions) {
  ASSERT_TRUE(https_server_expired_.Start());
  GURL expired_url = https_server_expired_.GetURL("/title1.html");
  int server_port = expired_url.IntPort();

  // Navigate to the expired cert URL, make sure we get an interstitial.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), expired_url));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

  // Click through the interstitial.
  ProceedThroughInterstitial(tab);

  // Shut down the server and navigate again to cause a network error.
  ASSERT_TRUE(https_server_expired_.ShutdownAndWaitUntilComplete());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), expired_url));

  // Create a new server in the same url (including port), the certificate
  // should still be invalid.
  net::EmbeddedTestServer new_https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  new_https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  new_https_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(new_https_server.Start(server_port));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), expired_url));

  // We shouldn't get an interstitial this time.
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(tab));
}

// Checks we don't attempt to show an interstitial (or crash) when visiting an
// SSL error related page in chrome://network-errors. Regression test for
// crbug.com/953812
IN_PROC_BROWSER_TEST_F(SSLUITest, NoInterstitialOnNetworkErrorPage) {
  GURL invalid_cert_url(blink::kChromeUINetworkErrorURL);
  GURL::Replacements replacements;
  replacements.SetPathStr("-207");
  invalid_cert_url = invalid_cert_url.ReplaceComponents(replacements);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), invalid_cert_url));
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
}

// This SPKI hash is from a self signed certificate generated using the
// following openssl command:
//  openssl req -new -x509 -keyout server.pem -out server.pem -days 365 -nodes
//  openssl x509 -noout -in certificate.pem -pubkey | \
//  openssl asn1parse -noout -inform pem -out public.key;
//  openssl dgst -sha256 -binary public.key | openssl enc -base64
// The actual value of the hash doesn't matter as long it's a valid SPKI hash.
const char kMatchingDynamicInterstitialCert[] =
    "sha256/eFi0afYJLdI0YsZFu4U8ra2B5/5ynzfKkI88M94iVFA=";

namespace {

class SSLUIDynamicInterstitialTest : public CertVerifierBrowserTest {
 public:
  SSLUIDynamicInterstitialTest()
      : CertVerifierBrowserTest(),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  SSLUIDynamicInterstitialTest(const SSLUIDynamicInterstitialTest&) = delete;
  SSLUIDynamicInterstitialTest& operator=(const SSLUIDynamicInterstitialTest&) =
      delete;

  ~SSLUIDynamicInterstitialTest() override {}

  void SetUpCertVerifier() {
    scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.verified_cert = cert;
    verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

    net::HashValue hash;
    ASSERT_TRUE(hash.FromString(kMatchingDynamicInterstitialCert));
    verify_result.public_key_hashes.push_back(hash);

    mock_cert_verifier()->AddResultForCert(cert, verify_result,
                                           net::ERR_CERT_COMMON_NAME_INVALID);
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  // Creates and returns a SSLErrorAssistantConfig object.
  std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
  CreateSSLErrorAssistantConfig() {
    auto config_proto =
        std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
    config_proto->set_version_id(kLargeVersionId);
    return config_proto;
  }

  // Adds a dynamic interstitial to |config_proto|. All of the dynamic
  // interstitial's fields mismatch with |https_server_|'s SSL info.
  void AddMismatchDynamicInterstitial(
      chrome_browser_ssl::SSLErrorAssistantConfig* config_proto) {
    chrome_browser_ssl::DynamicInterstitial* filter =
        config_proto->add_dynamic_interstitial();
    filter->set_interstitial_type(
        chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL);
    filter->set_cert_error(
        chrome_browser_ssl::DynamicInterstitial::ERR_CERT_DATE_INVALID);

    filter->add_sha256_hash("sha256/killdeer");
    filter->add_sha256_hash("sha256/thickkne");

    filter->set_issuer_common_name_regex("beeeater");
    filter->set_issuer_organization_regex("honeycreeper");
    filter->set_mitm_software_name(kTestMITMSoftwareName);
  }

  // Adds a dynamic interstitial to |config_proto| and returns it. All of the
  // fields in the dynamic intersitial matches with |https_server_|'s
  // SSL info. Optionally set the flag for triggering dynamic interstitials
  // only on non-overridable errors.
  chrome_browser_ssl::DynamicInterstitial* AddMatchingDynamicInterstitial(
      chrome_browser_ssl::SSLErrorAssistantConfig* config_proto,
      bool show_only_for_nonoverridable_errors = false) {
    chrome_browser_ssl::DynamicInterstitial* filter =
        config_proto->add_dynamic_interstitial();
    filter->set_interstitial_type(chrome_browser_ssl::DynamicInterstitial::
                                      INTERSTITIAL_PAGE_CAPTIVE_PORTAL);
    filter->set_cert_error(
        chrome_browser_ssl::DynamicInterstitial::ERR_CERT_COMMON_NAME_INVALID);

    filter->add_sha256_hash("sha256/kingfisher");
    filter->add_sha256_hash(kMatchingDynamicInterstitialCert);
    filter->add_sha256_hash("sha256/flycatcher");

    scoped_refptr<net::X509Certificate> cert = https_server_.GetCertificate();
    filter->set_issuer_common_name_regex(cert.get()->issuer().common_name);

    if (!cert.get()->issuer().organization_names.empty()) {
      filter->set_issuer_organization_regex(
          cert.get()->issuer().organization_names[0]);
    }

    filter->set_mitm_software_name(kTestMITMSoftwareName);

    filter->set_support_url("https://google.com");

    filter->set_show_only_for_nonoverridable_errors(
        show_only_for_nonoverridable_errors);

    return filter;
  }

  security_interstitials::SecurityInterstitialPage* GetInterstitialDelegate(
      WebContents* tab) {
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            tab);
    if (!helper)
      return nullptr;
    return helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();
  }

 private:
  net::EmbeddedTestServer https_server_;
};

}  // namespace

// Tests that the dynamic interstitial list is used when the feature is
// enabled via Finch. The list is passed to SSLErrorHandler via a proto.
IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest, Match) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());
    AddMatchingDynamicInterstitial(config_proto.get());

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/")));
    ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    ASSERT_EQ(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest, MatchUnknownCertError) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());

    // Add a matching dynamic interstitial with the UNKNOWN_CERT_ERROR status.
    chrome_browser_ssl::DynamicInterstitial* match =
        AddMatchingDynamicInterstitial(config_proto.get());
    match->set_cert_error(
        chrome_browser_ssl::DynamicInterstitial::UNKNOWN_CERT_ERROR);

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/")));
    ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    ASSERT_EQ(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest,
                       MatchEmptyCommonNameRegex) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());

    // Add a matching dynamic interstitial with an empty issuer common name
    // regex.
    chrome_browser_ssl::DynamicInterstitial* match =
        AddMatchingDynamicInterstitial(config_proto.get());
    match->set_issuer_common_name_regex(std::string());

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/")));
    ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    ASSERT_EQ(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest,
                       MatchEmptyOrganizationRegex) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());

    // Add a matching dynamic interstitial with an empty issuer organization
    // name regex.
    chrome_browser_ssl::DynamicInterstitial* match =
        AddMatchingDynamicInterstitial(config_proto.get());
    match->set_issuer_organization_regex(std::string());

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/")));
    ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    ASSERT_EQ(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest, MismatchHash) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());

    // Add a dynamic interstitial with matching fields, except for the
    // certificate hashes.
    chrome_browser_ssl::DynamicInterstitial* match =
        AddMatchingDynamicInterstitial(config_proto.get());
    match->clear_sha256_hash();
    match->add_sha256_hash("sha256/sapsucker");
    match->add_sha256_hash("sha256/flowerpiercer");

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/")));
    ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    EXPECT_NE(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest, MismatchCertError) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());

    // Add a dynamic interstitial with matching fields, except for the
    // cert error field.
    chrome_browser_ssl::DynamicInterstitial* match =
        AddMatchingDynamicInterstitial(config_proto.get());
    match->set_cert_error(
        chrome_browser_ssl::DynamicInterstitial::ERR_CERT_DATE_INVALID);

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/")));
    ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    EXPECT_NE(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest, MismatchCommonNameRegex) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());

    // Add a dynamic interstitial with matching fields, except for the
    // issuer common name regex field.
    chrome_browser_ssl::DynamicInterstitial* match =
        AddMatchingDynamicInterstitial(config_proto.get());
    match->set_issuer_common_name_regex("beeeater");

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/")));
    ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    EXPECT_NE(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest,
                       MismatchOrganizationRegex) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());

    // Add a dynamic interstitial with matching fields, except for the
    // issuer organization name regex field.
    chrome_browser_ssl::DynamicInterstitial* match =
        AddMatchingDynamicInterstitial(config_proto.get());
    match->set_issuer_organization_regex("honeycreeper");

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/")));
    ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    EXPECT_NE(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest, MismatchWhenOverridable) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());

    // Add a matching dynamic interstitial, except for the
    // show_only_for_nonoverridable_errors flag is set to true.
    chrome_browser_ssl::DynamicInterstitial* match =
        AddMatchingDynamicInterstitial(config_proto.get(), true);
    match->set_cert_error(
        chrome_browser_ssl::DynamicInterstitial::UNKNOWN_CERT_ERROR);

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/")));
    ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    EXPECT_NE(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

class RecurrentInterstitialBrowserTest : public CertVerifierBrowserTest {
 public:
  RecurrentInterstitialBrowserTest() : CertVerifierBrowserTest() {}

  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// Tests that a message is added to the interstitial when an error code recurs
// multiple times.
IN_PROC_BROWSER_TEST_F(RecurrentInterstitialBrowserTest,
                       RecurrentInterstitial) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_server.Start());
  mock_cert_verifier()->set_default_result(
      net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED);

  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          browser()->profile()->GetSSLHostStateDelegate());
  state->ResetRecurrentErrorCountForTesting();

  state->SetRecurrentInterstitialThresholdForTesting(2);

  // Use different hostnames for the two test cases to avoid the clickthrough
  // from one interfering with the other.
  GURL url = https_server.GetURL("show_error_message.test", "/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));
  ExpectInterstitialElementHidden(tab, "recurrent-error-message",
                                  true /* expect_hidden */);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));
  ExpectInterstitialElementHidden(tab, "recurrent-error-message",
                                  false /* expect_hidden */);

  // Proceed through the interstitial and observe that the histogram is
  // recorded correctly.
  content::TestNavigationObserver nav_observer(tab, 1);
  ASSERT_TRUE(
      content::ExecJs(tab, "window.certificateErrorPageController.proceed();"));
  nav_observer.Wait();
}

// Tests that mixed content is tracked by origin, not by URL. This is tested by
// checking that mixed content flags are set appropriately for about:blank URLs
// (who inherit the origin of their opener).
//
// Note: we test that mixed content flags are propagated from an opener page to
// about:blank, but not the other way around. This is because there is no way
// for a mixed content flag to propagate from about:blank to a different
// tab. Passive mixed content flags are not propagated from one tab to another,
// and for active mixed content, there's no way to bypass mixed content blocking
// on about:blank pages, so there's no way that the origin would get flagged for
// active mixed content from an about:blank page. (There's no way to bypass
// mixed content blocking on about:blank pages because the bypass is implemented
// as a content setting, which doesn't apply to about:blank.)
IN_PROC_BROWSER_TEST_F(SSLUITest, ActiveMixedContentTrackedByOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html",
      embedded_test_server()->host_port_pair());

  // The insecure script is allowed to load because SSLUITestBase sets the
  // --allow-running-insecure-content flag.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, CertError::NONE, AuthState::RAN_INSECURE_CONTENT);

  // Open a new tab from the current page. After an initial navigation,
  // navigate it to about:blank and check that the about:blank page is
  // downgraded, because it shares an origin with |tab| which ran mixed
  // content.
  //
  // Note that the security indicator is not downgraded on the initial
  // about:blank navigation in the new tab. Initial about:blank navigations
  // don't have navigation entries (yet), so there is no way to track the mixed
  // content state for these navigations. See https://crbug.com/1038765.
  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  ASSERT_TRUE(content::ExecJs(tab, "w = window.open()"));
  tab_waiter.Wait();
  WebContents* opened_tab = browser()->tab_strip_model()->GetWebContentsAt(1);
  content::TestNavigationObserver first_navigation(opened_tab);
  ASSERT_TRUE(content::ExecJs(
      tab, content::JsReplace("w.location.href = $1",
                              embedded_test_server()->GetURL("/title1.html"))));
  first_navigation.Wait();

  if (content::IsIsolatedOriginRequiredToGuaranteeDedicatedProcess()) {
    // Verify that in process models where sites need to be explicitly isolated,
    // the new tab ends up in the same process as the original one.
    // As a result of being in the same process, expect the SSL state to
    // reflect that the process has run insecure content.
    EXPECT_EQ(tab->GetSiteInstance()->GetProcess(),
              opened_tab->GetSiteInstance()->GetProcess());
    ssl_test_util::CheckAuthenticationBrokenState(
        opened_tab, CertError::NONE, AuthState::RAN_INSECURE_CONTENT);
  } else {
    // Verify that tabs are in different processes and that the new tab does
    // not have insecure status yet.
    EXPECT_NE(tab->GetSiteInstance()->GetProcess(),
              opened_tab->GetSiteInstance()->GetProcess());
    ssl_test_util::CheckUnauthenticatedState(opened_tab, AuthState::NONE);
  }

  content::TestNavigationObserver about_blank_navigation(opened_tab);
  ASSERT_TRUE(content::ExecJs(tab, "w.location.href = 'about:blank'"));
  about_blank_navigation.Wait();
  ssl_test_util::CheckAuthenticationBrokenState(
      opened_tab, CertError::NONE, AuthState::RAN_INSECURE_CONTENT);

  // Verify the two tabs are now in the same process independent of
  // process model.
  EXPECT_EQ(tab->GetSiteInstance()->GetProcess(),
            opened_tab->GetSiteInstance()->GetProcess());
}

// Tests that MixedContentShown histogram doesn't get logged when a site with
// a bad certificate loads a subresource (which also has a bad certificate).
IN_PROC_BROWSER_TEST_F(
    SSLUITest,
    MixedContentHistogramNotLoggedForSiteWithBadCertificate) {
  ASSERT_TRUE(https_server_expired_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Navigate to a page with a certificate error, and click through the
  // interstitial.
  // page_with_subresource.html loads both a script (which would count as
  // blockable mixed content), and an image (which would count as optionally
  // blockable mixed content) from the same origin as the main site.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server_expired_.GetURL("/ssl/page_with_subresource.html")));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);
  ProceedThroughInterstitial(tab);
}

// Tests that MixedContentShown histogram gets logged when a site with
// a valid certificate loads a subresource with a bad certificate.
IN_PROC_BROWSER_TEST_F(
    SSLUITest,
    MixedContentHistogramLoggedForBadCertificateSubresource) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());
  GURL base_url("https://site.test");

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Navigate to a page with a certificate error, and click through the
  // interstitial so the certificate is allowlisted.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://site.test:" +
                      base::NumberToString(https_server_expired_.port()) +
                      "/ssl/blank_page.html")));
  ProceedThroughInterstitial(tab);

  // Navigate to a page with a valid certificate, that contains subresouces from
  // the previously allowlisted bad certificate page.
  base::StringPairs replacement_text;
  replacement_text.push_back(make_pair(
      "REPLACE_WITH_HOST_AND_PORT",
      ("site.test:" + base::NumberToString(https_server_expired_.port()))));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server_.GetURL((net::test_server::GetFilePathWithReplacements(
          "/ssl/page_with_unsafe_contents.html", replacement_text)))));
}

// Tests that MixedContentShown histogram gets logged when a site with
// a valid certificate loads an insecure form.
IN_PROC_BROWSER_TEST_F(SSLUITest, MixedContentHistogramLoggedForInsecureForm) {
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server_.GetURL("/ssl/page_displays_insecure_form.html")));
}

// Tests that MixedContentShown histogram gets logged when a site with
// a valid certificate loads an insecure blockable resource (a script).
// TODO(carlosil): This test works because SSLUITest has
// kMixedContentAutoupgrade disabled. When cleaning up the autoupgrade flag,
// this will need to be rewritten to use content settings.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       MixedContentHistogramLoggedForBlockableMixedContent) {
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/page_runs_insecure_content.html")));
}

// Tests that MixedContentShown histogram gets logged when a site with
// a valid certificate loads an insecure optionally blockable resource (an
// image).
// TODO(carlosil): This test works because SSLUITest has
// kMixedContentAutoupgrade disabled. When cleaning up the autoupgrade flag,
// this will need to be rewritten to use content settings.
IN_PROC_BROWSER_TEST_F(
    SSLUITest,
    MixedContentHistogramLoggedForOptionallyBlockableMixedContent) {
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server_.GetURL("/ssl/page_displays_insecure_content.html")));
}

class SSLUIAutoReloadTest : public SSLUITest {
 public:
  SSLUIAutoReloadTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(embedder_support::kEnableAutoReload);
    SSLUITest::SetUpCommandLine(command_line);
  }
};

// SSL interstitials should disable autoreload timer.
IN_PROC_BROWSER_TEST_F(SSLUIAutoReloadTest, AutoReloadDisabled) {
  ASSERT_TRUE(https_server_expired_.Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  auto* reloader = error_page::NetErrorAutoReloader::FromWebContents(tab);
  const std::optional<base::OneShotTimer>& timer =
      reloader->next_reload_timer_for_testing();
  EXPECT_EQ(std::nullopt, timer);
}

class SSLUITestWithEnhancedProtectionMessage : public SSLUITest {
 public:
  SSLUITestWithEnhancedProtectionMessage() = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SSLUITestWithEnhancedProtectionMessage,
                       VerifyEnhancedProtectionMessageShown) {
  base::HistogramTester histograms;
  const std::string interaction_histogram =
      "interstitial.ssl_overridable.interaction";

  safe_browsing::SetExtendedReportingPrefForTests(
      browser()->profile()->GetPrefs(), true);
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(contents));
  ExpectInterstitialElementHidden(contents, "enhanced-protection-message",
                                  false /* expect_hidden */);

  histograms.ExpectTotalCount(interaction_histogram, 2);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::SHOW_ENHANCED_PROTECTION, 1);
}

IN_PROC_BROWSER_TEST_F(SSLUITestWithEnhancedProtectionMessage,
                       VerifyEnhancedProtectionMessageNotShownAlreadyInEp) {
  safe_browsing::SetExtendedReportingPrefForTests(
      browser()->profile()->GetPrefs(), true);
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(contents));
  ExpectInterstitialElementHidden(contents, "extended-reporting-opt-in",
                                  true /* expect_hidden */);
  ExpectInterstitialElementHidden(contents, "enhanced-protection-message",
                                  true /* expect_hidden */);
}

IN_PROC_BROWSER_TEST_F(SSLUITestWithEnhancedProtectionMessage,
                       VerifyEnhancedProtectionMessageNotShownManaged) {
  policy::PolicyMap policies;
  policies.Set(policy::key::kSafeBrowsingProtectionLevel,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(/* standard protection */ 1), nullptr);
  UpdateChromePolicy(policies);
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html")));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(contents));
  ExpectInterstitialElementHidden(contents, "enhanced-protection-message",
                                  true /* expect_hidden */);
}

class InsecureFormNavigationThrottleFencedFrameBrowserTest
    : public InProcessBrowserTest {
 public:
  InsecureFormNavigationThrottleFencedFrameBrowserTest() = default;
  ~InsecureFormNavigationThrottleFencedFrameBrowserTest() override = default;

  WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

// Tests that a fenced frame doesn't create a security interstitial.
IN_PROC_BROWSER_TEST_F(InsecureFormNavigationThrottleFencedFrameBrowserTest,
                       DoNotCreateSecurityInterstitialInFencedFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  GURL initial_url = https_server.GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  std::string replacement_path =
      SSLUITestBase::GetFilePathWithHostAndPortReplacement(
          "/ssl/page_displays_insecure_form.html",
          embedded_test_server()->host_port_pair());
  GURL form_site_url = https_server.GetURL(replacement_path);

  // Navigate to site with an insecure form and submit it in a fenced frame.
  content::RenderFrameHost* fenced_frame =
      fenced_frame_test_helper().CreateFencedFrame(browser()
                                                       ->tab_strip_model()
                                                       ->GetActiveWebContents()
                                                       ->GetPrimaryMainFrame(),
                                                   form_site_url);
  ASSERT_TRUE(fenced_frame);
  content::TestNavigationObserver observer(GetWebContents());
  content::WebContentsConsoleObserver console_observer(GetWebContents());
  console_observer.SetPattern(
      "Mixed Content: The page at * was loaded over a secure connection, but "
      "contains a form that targets an insecure endpoint "
      "'http://does-not-exist.test/ssl/google_files/logo.gif'. This endpoint "
      "should be made available over a secure connection.");
  ASSERT_TRUE(content::ExecJs(fenced_frame, "submitForm();"));
  observer.Wait();

  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          GetWebContents());

  // No interstitial should be created in the fenced frame, and the the fenced
  // frame should be in |form_site_url| and primary mainframe should be in the
  // initial URL.
  EXPECT_TRUE(!helper || !helper->IsDisplayingInterstitial());
  EXPECT_EQ(fenced_frame->GetLastCommittedURL(), form_site_url);
  EXPECT_EQ(GetWebContents()->GetVisibleURL(), initial_url);

  // Check console message was printed.
  EXPECT_EQ(console_observer.messages().size(), 1u);
}

// TODO(jcampan): more tests to do below.

// Visit a page over https that contains a frame with a redirect.

// XMLHttpRequest insecure content in synchronous mode.

// XMLHttpRequest insecure content in asynchronous mode.

// XMLHttpRequest over bad ssl in synchronous mode.

// XMLHttpRequest over OK ssl in synchronous mode.

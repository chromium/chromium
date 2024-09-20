// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_request_content.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_test_util.h"
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_type.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/signed_exchange_browser_test_helper.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_database.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace {

using password_manager::metrics_util::PasswordType;
using safe_browsing::LoginReputationClientResponse;
using safe_browsing::RequestOutcome;

const char kCreateFilesystemUrlJavascript[] =
    "new Promise(resolve => {"
    "  window.webkitRequestFileSystem(window.TEMPORARY, 4096, function(fs) {"
    "    fs.root.getFile('test.html', {create: true}, function(fileEntry) {"
    "      fileEntry.createWriter(function(writer) {"
    "        writer.onwriteend = function(e) {"
    "          resolve(fileEntry.toURL());"
    "        };"
    "        var blob = new Blob(['<html>hello</html>'], {type: 'text/html'});"
    "        writer.write(blob);"
    "      });"
    "    });"
    "  });"
    "});";

const char kCreateBlobUrlJavascript[] =
    "var blob = new Blob(['<html>hello</html>'],"
    "                    {type: 'text/html'});"
    "URL.createObjectURL(blob);";

enum CertificateStatus { VALID_CERTIFICATE, INVALID_CERTIFICATE };

// Inject a script into every frame in the active page. Used by tests that check
// for visible password fields to wait for notifications about these fields.
// Notifications about visible password fields are queued at the end of the
// event loop, so waiting for a dummy script to run ensures that these
// notifications have been sent.
void InjectScript(content::WebContents* contents) {
  // Any frame in the active page might have a password field, so inject scripts
  // into all of them to ensure that notifications from all of them have been
  // sent.
  contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [](content::RenderFrameHost* frame) {
        EXPECT_EQ(true, content::EvalJs(frame, "true;"));
      });
}

// A WebContentsObserver useful for testing the DidChangeVisibleSecurityState()
// method: it keeps track of the latest security level that was fired.
class SecurityStyleTestObserver : public content::WebContentsObserver {
 public:
  explicit SecurityStyleTestObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  SecurityStyleTestObserver(const SecurityStyleTestObserver&) = delete;
  SecurityStyleTestObserver& operator=(const SecurityStyleTestObserver&) =
      delete;

  void DidChangeVisibleSecurityState() override {
    auto* helper = SecurityStateTabHelper::FromWebContents(web_contents());
    latest_security_level_ = helper->GetSecurityLevel();
    run_loop_.Quit();
  }

  void WaitForDidChangeVisibleSecurityState() { run_loop_.Run(); }

  std::optional<security_state::SecurityLevel> latest_security_level() const {
    return latest_security_level_;
  }

  void ClearLatestSecurityLevel() { latest_security_level_ = std::nullopt; }

 private:
  std::optional<security_state::SecurityLevel> latest_security_level_;
  base::RunLoop run_loop_;
};

void CheckSecurityInfoForSecure(
    content::WebContents* contents,
    security_state::SecurityLevel expect_security_level,
    bool expect_sha1_in_chain,
    bool expect_displayed_mixed_content,
    bool expect_ran_mixed_content,
    bool expect_cert_error) {
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();
  EXPECT_EQ(expect_security_level, helper->GetSecurityLevel());
  EXPECT_EQ(expect_sha1_in_chain,
            security_state::IsSHA1InChain(*visible_security_state));
  EXPECT_EQ(expect_displayed_mixed_content,
            visible_security_state->displayed_mixed_content);
  EXPECT_EQ(expect_ran_mixed_content,
            visible_security_state->ran_mixed_content);
  EXPECT_TRUE(
      security_state::IsSchemeCryptographic(visible_security_state->url));
  EXPECT_EQ(expect_cert_error,
            net::IsCertStatusError(visible_security_state->cert_status));
  EXPECT_TRUE(visible_security_state->connection_info_initialized);
  EXPECT_TRUE(visible_security_state->certificate);
}

// Check that the current security state reflects a non-committed navigation.
void CheckSecurityInfoForNonCommitted(content::WebContents* contents) {
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();
  EXPECT_EQ(security_state::NONE, helper->GetSecurityLevel());
  EXPECT_FALSE(security_state::IsSHA1InChain(*visible_security_state));
  EXPECT_FALSE(visible_security_state->displayed_mixed_content);
  EXPECT_FALSE(visible_security_state->ran_mixed_content);
  EXPECT_FALSE(
      security_state::IsSchemeCryptographic(visible_security_state->url));
  EXPECT_FALSE(net::IsCertStatusError(visible_security_state->cert_status));
  EXPECT_FALSE(visible_security_state->connection_info_initialized);
  EXPECT_FALSE(visible_security_state->certificate);
}

void ProceedThroughInterstitial(content::WebContents* tab) {
  content::TestNavigationObserver nav_observer(tab, 1);
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
      ->CommandReceived(
          base::NumberToString(security_interstitials::CMD_PROCEED));
  nav_observer.Wait();
}

std::string GetFilePathWithHostAndPortReplacement(
    const std::string& original_file_path,
    const net::HostPortPair& host_port_pair) {
  base::StringPairs replacement_text;
  replacement_text.push_back(
      make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
  return net::test_server::GetFilePathWithReplacements(original_file_path,
                                                       replacement_text);
}

GURL GetURLWithNonLocalHostname(net::EmbeddedTestServer* server,
                                const std::string& path) {
  GURL::Replacements replace_host;
  replace_host.SetHostStr("example1.test");
  return server->GetURL(path).ReplaceComponents(replace_host);
}

class SecurityStateTabHelperTest : public CertVerifierBrowserTest {
 public:
  SecurityStateTabHelperTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        true);
  }

  SecurityStateTabHelperTest(const SecurityStateTabHelperTest&) = delete;
  SecurityStateTabHelperTest& operator=(const SecurityStateTabHelperTest&) =
      delete;

  ~SecurityStateTabHelperTest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        std::nullopt);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(https_server_.Start());
    host_resolver()->AddRule("*", "127.0.0.1");
    // Allow tests to trigger error pages.
    host_resolver()->AddSimulatedFailure("nonexistent.test");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    CertVerifierBrowserTest::SetUpCommandLine(command_line);
    // Browser will both run and display insecure content.
    command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
  }

 protected:
  void SetUpMockCertVerifierForHttpsServer(net::CertStatus cert_status,
                                           int net_result) {
    scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.is_issued_by_known_root = false;
    verify_result.verified_cert = cert;
    verify_result.cert_status = cert_status;

    mock_cert_verifier()->AddResultForCert(cert, verify_result, net_result);
  }

  // Navigates to an empty page and runs |javascript| to create a URL with with
  // a scheme of |scheme|. Expects a security level of NONE if
  // |use_secure_inner_origin| is true and DANGEROUS otherwise.
  void TestBlobOrFilesystemURL(const std::string& scheme,
                               const std::string& javascript,
                               bool use_secure_inner_origin) {
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(contents);

    SecurityStateTabHelper* helper =
        SecurityStateTabHelper::FromWebContents(contents);
    ASSERT_TRUE(helper);

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        GetURLWithNonLocalHostname(
            use_secure_inner_origin ? &https_server_ : embedded_test_server(),
            "/empty.html")));

    // Create a URL and navigate to it.
    std::string blob_or_filesystem_url =
        content::EvalJs(contents, javascript).ExtractString();
    EXPECT_TRUE(GURL(blob_or_filesystem_url).SchemeIs(scheme));

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(blob_or_filesystem_url)));
    InjectScript(contents);

    content::NavigationEntry* entry =
        contents->GetController().GetVisibleEntry();
    ASSERT_TRUE(entry);

    EXPECT_EQ(use_secure_inner_origin ? security_state::NONE
                                      : security_state::WARNING,
              helper->GetSecurityLevel());
  }

  net::EmbeddedTestServer https_server_;
};

// Same as SecurityStateTabHelperTest, but with Incognito enabled.
class SecurityStateTabHelperIncognitoTest : public SecurityStateTabHelperTest {
 public:
  SecurityStateTabHelperIncognitoTest() : SecurityStateTabHelperTest() {}

  SecurityStateTabHelperIncognitoTest(
      const SecurityStateTabHelperIncognitoTest&) = delete;
  SecurityStateTabHelperIncognitoTest& operator=(
      const SecurityStateTabHelperIncognitoTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SecurityStateTabHelperTest::SetUpCommandLine(command_line);
    // Test should run Incognito.
    command_line->AppendSwitch(switches::kIncognito);
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class DidChangeVisibleSecurityStateTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  DidChangeVisibleSecurityStateTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  }

  DidChangeVisibleSecurityStateTest(const DidChangeVisibleSecurityStateTest&) =
      delete;
  DidChangeVisibleSecurityStateTest& operator=(
      const DidChangeVisibleSecurityStateTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Browser will both run and display insecure content.
    command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, HttpPage) {
  GURL http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/ssl/google.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), http_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());
  EXPECT_FALSE(security_state::IsSHA1InChain(*visible_security_state));
  EXPECT_FALSE(visible_security_state->displayed_mixed_content);
  EXPECT_FALSE(visible_security_state->ran_mixed_content);
  EXPECT_FALSE(
      security_state::IsSchemeCryptographic(visible_security_state->url));
  EXPECT_FALSE(net::IsCertStatusError(visible_security_state->cert_status));
  // We expect that the security info fields for this committed navigation will
  // be initialized to reflect the unencrypted connection.
  EXPECT_TRUE(visible_security_state->connection_info_initialized);
  EXPECT_FALSE(visible_security_state->certificate);
  EXPECT_EQ(0, visible_security_state->connection_status);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, HttpsPage) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, false, false,
      false /* expect cert status error */);
}

// TODO(crbug.com/40928765): Add an end-to-end test for
// security_state::SECURE_WITH_POLICY_INSTALLED_CERT (currently that depends on
// a cros-specific policy/service).

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, DevToolsPage) {
  GURL devtools_url("devtools://devtools/bundled/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), devtools_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();
  EXPECT_EQ(security_state::NONE, helper->GetSecurityLevel());
  EXPECT_TRUE(visible_security_state->is_devtools);
}

// Tests that interstitial.ssl.visited_site_after_warning is being logged to
// correctly.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, UMALogsVisitsAfterWarning) {
  const char kHistogramName[] = "interstitial.ssl.visited_site_after_warning";
  base::HistogramTester histograms;
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_DATE_INVALID,
                                      net::ERR_CERT_DATE_INVALID);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  // Histogram shouldn't log before clicking through interstitial.
  histograms.ExpectTotalCount(kHistogramName, 0);
  ProceedThroughInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents());
  // Histogram should log after clicking through.
  histograms.ExpectTotalCount(kHistogramName, 1);
  histograms.ExpectBucketCount(kHistogramName, true, 1);
}

// Tests that interstitial.ssl.visited_site_after_warning is being logged
// on ERR_CERT_UNABLE_TO_CHECK_REVOCATION (which previously did not show an
// interstitial.)
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       UMALogsVisitsAfterRevocationCheckFailureWarning) {
  const char kHistogramName[] = "interstitial.ssl.visited_site_after_warning";
  base::HistogramTester histograms;
  SetUpMockCertVerifierForHttpsServer(
      net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION,
      net::ERR_CERT_UNABLE_TO_CHECK_REVOCATION);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  // Histogram shouldn't log before clicking through interstitial.
  histograms.ExpectTotalCount(kHistogramName, 0);
  ProceedThroughInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents());
  // Histogram should log after clicking through.
  histograms.ExpectUniqueSample(kHistogramName, true, 1);
}

// Test security state after clickthrough for a SHA-1 certificate that is
// blocked by default.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, SHA1CertificateBlocked) {
  SetUpMockCertVerifierForHttpsServer(
      net::CERT_STATUS_SHA1_SIGNATURE_PRESENT |
          net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM,
      net::ERR_CERT_WEAK_SIGNATURE_ALGORITHM);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, true, false, false,
      true /* expect cert status error */);

  ProceedThroughInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents());

  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, true, false, false,
      true /* expect cert status error */);
}

// Test security state for a SHA-1 certificate that is allowed by policy.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, SHA1CertificateWarning) {
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT,
                                      net::OK);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, true, false, false,
      false /* expect cert status error */);
}

class SecurityStateTabHelperTestWithAutoupgradesDisabled
    : public SecurityStateTabHelperTest {
 public:
  SecurityStateTabHelperTestWithAutoupgradesDisabled() {
    feature_list.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTestWithAutoupgradesDisabled,
                       MixedContent) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  net::HostPortPair replacement_pair = embedded_test_server()->host_port_pair();
  replacement_pair.set_host("example.test");

  // Navigate to an HTTPS page that displays mixed content.
  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html", replacement_pair);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::WARNING, false, true, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that displays mixed content dynamically.
  replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html", replacement_pair);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, false, false,
      false /* expect cert status error */);
  // Load the insecure image.
  EXPECT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      "loadBadImage();"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::WARNING, false, true, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs mixed content.
  replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html", replacement_pair);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false, false, true,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs and displays mixed content.
  replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_and_displays_insecure_content.html", replacement_pair);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false, true, true,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs mixed content in an iframe.
  net::HostPortPair host_port_pair =
      net::HostPortPair::FromURL(https_server_.GetURL("/title1.html"));
  host_port_pair.set_host("different-host.test");
  replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content_in_iframe.html", host_port_pair);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false, false, true,
      false /* expect cert status error */);
}

class
    SecurityStateTabHelperTestWithAutoupgradesDisabledAndMixedContentWarningEnabled
    : public SecurityStateTabHelperTest {
 public:
  SecurityStateTabHelperTestWithAutoupgradesDisabledAndMixedContentWarningEnabled() {
    feature_list.InitWithFeatures(
        /* enabled_features */
        {},
        /* disabled_features */
        {blink::features::kMixedContentAutoupgrade});
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

// Checks that the proper security state is displayed when the passive mixed
// content warning feature is enabled.
// TODO(crbug.com/40108287): Once that launch is finished and other tests run
// with the feature enabled this test and the class will be redundant and can be
// removed.
IN_PROC_BROWSER_TEST_F(
    SecurityStateTabHelperTestWithAutoupgradesDisabledAndMixedContentWarningEnabled,
    PassiveMixedContentWarning) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  net::HostPortPair replacement_pair = embedded_test_server()->host_port_pair();
  replacement_pair.set_host("example.test");

  // Navigate to an HTTPS page that displays mixed content.
  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html", replacement_pair);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::WARNING, false, true, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that displays mixed content dynamically.
  replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html", replacement_pair);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, false, false,
      false /* expect cert status error */);
  // Load the insecure image.
  EXPECT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      "loadBadImage();"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::WARNING, false, true, false,
      false /* expect cert status error */);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       ActiveContentWithCertErrors) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Navigate to an HTTPS page and simulate active content with
  // certificate errors.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/title1.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  entry->GetSSL().content_status |=
      content::SSLStatus::RAN_CONTENT_WITH_CERT_ERRORS;

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  ASSERT_TRUE(helper);
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();

  EXPECT_FALSE(net::IsCertStatusError(visible_security_state->cert_status));
  EXPECT_EQ(security_state::DANGEROUS, helper->GetSecurityLevel());
  EXPECT_TRUE(visible_security_state->ran_content_with_cert_errors);
  EXPECT_FALSE(visible_security_state->displayed_content_with_cert_errors);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       PassiveContentWithCertErrors) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Navigate to an HTTPS page and simulate passive content with
  // certificate errors.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/title1.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  entry->GetSSL().content_status |=
      content::SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS;

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  ASSERT_TRUE(helper);
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();

  EXPECT_FALSE(net::IsCertStatusError(visible_security_state->cert_status));
  EXPECT_EQ(security_state::NONE, helper->GetSecurityLevel());
  EXPECT_FALSE(visible_security_state->ran_content_with_cert_errors);
  EXPECT_TRUE(visible_security_state->displayed_content_with_cert_errors);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       ActiveAndPassiveContentWithCertErrors) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Navigate to an HTTPS page and simulate active and passive content
  // with certificate errors.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/title1.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  entry->GetSSL().content_status |=
      content::SSLStatus::RAN_CONTENT_WITH_CERT_ERRORS;
  entry->GetSSL().content_status |=
      content::SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS;

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  ASSERT_TRUE(helper);
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();

  EXPECT_FALSE(net::IsCertStatusError(visible_security_state->cert_status));
  EXPECT_EQ(security_state::DANGEROUS, helper->GetSecurityLevel());
  EXPECT_TRUE(visible_security_state->ran_content_with_cert_errors);
  EXPECT_TRUE(visible_security_state->displayed_content_with_cert_errors);
}

// Same as SecurityStateTabHelperTest.ActiveAndPassiveContentWithCertErrors but
// with a SHA1 cert.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTestWithAutoupgradesDisabled,
                       MixedContentWithSHA1Cert) {
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT,
                                      net::OK);

  net::HostPortPair replacement_pair = embedded_test_server()->host_port_pair();
  replacement_pair.set_host("example.test");

  // Navigate to an HTTPS page that displays mixed content.
  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html", replacement_pair);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, true, true, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that displays mixed content dynamically.
  replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html", replacement_pair);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, true, false, false,
      false /* expect cert status error */);
  // Load the insecure image.
  EXPECT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      "loadBadImage();"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, true, true, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs mixed content.
  replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html", replacement_pair);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, true, false, true,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs and displays mixed content.
  replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_and_displays_insecure_content.html", replacement_pair);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, true, true, true,
      false /* expect cert status error */);
}

// Tests that the Content Security Policy block-all-mixed-content
// directive stops mixed content from running.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTestWithAutoupgradesDisabled,
                       MixedContentStrictBlocking) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Navigate to an HTTPS page that tries to run mixed content in an
  // iframe, with strict mixed content blocking.
  net::HostPortPair host_port_pair =
      net::HostPortPair::FromURL(https_server_.GetURL("/title1.html"));
  host_port_pair.set_host("different-host.test");
  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content_in_iframe_with_strict_blocking.html",
      host_port_pair);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, false, false,
      false /* expect cert status error */);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTestWithAutoupgradesDisabled,
                       BrokenHTTPS) {
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_DATE_INVALID,
                                      net::ERR_CERT_DATE_INVALID);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false, false, false,
      true /* expect cert status error */);

  ProceedThroughInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents());

  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false, false, false,
      true /* expect cert status error */);

  // Navigate to a broken HTTPS page that displays mixed content.
  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false, true, false,
      true /* expect cert status error */);
}

// Tests that the security level of HTTP pages is set properly.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, SecurityLevelForHttpPage) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStyleTestObserver observer(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  GURL http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), http_url));
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(content::SSLStatus::NORMAL_CONTENT, entry->GetSSL().content_status);
}

// Tests that the security level of data: URLs is always downgraded to
// WARNING.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       SecurityLevelDowngradedOnDataUrl) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStyleTestObserver observer(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<html></html>")));
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(content::SSLStatus::NORMAL_CONTENT, entry->GetSSL().content_status);
}

// Tests the security level and malicious content status for sign-in password
// reuse threat type.
IN_PROC_BROWSER_TEST_F(
    SecurityStateTabHelperTest,
    VerifySignInPasswordReuseMaliciousContentAndSecurityLevel) {
  // Setup https server. This makes sure that the DANGEROUS security level is
  // not caused by any certificate error rather than the password reuse SB
  // threat type.
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStyleTestObserver observer(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  // Update security state of the current page to match
  // SB_THREAT_TYPE_GAIA_PASSWORD_REUSE.
  safe_browsing::ChromePasswordProtectionService* service =
      safe_browsing::ChromePasswordProtectionService::
          GetPasswordProtectionService(browser()->profile());
  safe_browsing::ReusedPasswordAccountType account_type;
  account_type.set_account_type(
      safe_browsing::ReusedPasswordAccountType::GSUITE);
  account_type.set_is_account_syncing(true);
  scoped_refptr<safe_browsing::PasswordProtectionRequest> request =
      safe_browsing::CreateDummyRequest(contents);
  service->ShowModalWarning(
      request.get(), LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      "unused_token", account_type);
  observer.WaitForDidChangeVisibleSecurityState();

  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();

  EXPECT_EQ(security_state::DANGEROUS, helper->GetSecurityLevel());
  EXPECT_EQ(
      security_state::MALICIOUS_CONTENT_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE,
      visible_security_state->malicious_content_status);

  // Simulates a Gaia password change, then malicious content status will
  // change to MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING.
  service->OnGaiaPasswordChanged(/*username=*/"", false);
  base::RunLoop().RunUntilIdle();
  visible_security_state = helper->GetVisibleSecurityState();
  EXPECT_EQ(security_state::DANGEROUS, helper->GetSecurityLevel());
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING,
            visible_security_state->malicious_content_status);
}

// Tests the security level and malicious content status for enterprise password
// reuse threat type.
IN_PROC_BROWSER_TEST_F(
    SecurityStateTabHelperTest,
    VerifyEnterprisePasswordReuseMaliciousContentAndSecurityLevel) {
  // Setup https server. This makes sure that the DANGEROUS security level is
  // not caused by any certificate error rather than the password reuse SB
  // threat type.
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  SecurityStyleTestObserver observer(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  // Update security state of the current page to match
  // SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE.
  safe_browsing::ChromePasswordProtectionService* service =
      safe_browsing::ChromePasswordProtectionService::
          GetPasswordProtectionService(browser()->profile());
  scoped_refptr<safe_browsing::PasswordProtectionRequest> request =
      safe_browsing::CreateDummyRequest(contents);
  service->ShowModalWarning(
      request.get(), LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      "unused_token",
      service->GetPasswordProtectionReusedPasswordAccountType(
          PasswordType::ENTERPRISE_PASSWORD, /*username=*/""));
  observer.WaitForDidChangeVisibleSecurityState();

  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();
  EXPECT_EQ(security_state::DANGEROUS, helper->GetSecurityLevel());
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE,
            visible_security_state->malicious_content_status);

  // Since these are non-Gaia enterprise passwords, Gaia password change won't
  // have any impact here.
}

class PKPModelClientTest : public SecurityStateTabHelperTest {
 public:
  static constexpr const char* kPKPHost = "example.test";

  void SetUpOnMainThread() override {
    // This test class intentionally does not call the parent SetUpOnMainThread.

    // Switch HTTPS server to use the "localhost" cert. The test mocks out cert
    // verification results based on the used cert.
    https_server_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
    ASSERT_TRUE(https_server_.Start());

    host_resolver()->AddIPLiteralRule(
        kPKPHost, https_server_.GetIPLiteralString(), std::string());

    EnableStaticPins();
  }

  void TearDownOnMainThread() override {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;

    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterfaceForTesting(
        network_service_test.BindNewPipeAndPassReceiver());
    network_service_test->SetTransportSecurityStateSource(0);

    // This test class intentionally does not call the parent
    // TearDownOnMainThread.
  }

 private:
  void EnableStaticPins() {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;

    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterfaceForTesting(
        network_service_test.BindNewPipeAndPassReceiver());
    // The tests don't depend on reporting, so the port doesn't matter.
    network_service_test->SetTransportSecurityStateSource(80);

    content::StoragePartition* partition =
        browser()->profile()->GetDefaultStoragePartition();
    partition->GetNetworkContext()->EnableStaticKeyPinningForTesting();
  }
};

IN_PROC_BROWSER_TEST_F(PKPModelClientTest, PKPBypass) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);

  scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
  net::CertVerifyResult verify_result;
  // PKP is bypassed when |is_issued_by_known_root| is false.
  verify_result.is_issued_by_known_root = false;
  verify_result.verified_cert = cert;
  // Public key hash which does not match the value in the static pin.
  net::HashValue hash(net::HASH_VALUE_SHA256);
  memset(hash.data(), 1, hash.size());
  verify_result.public_key_hashes.push_back(hash);

  mock_cert_verifier()->AddResultForCert(cert, verify_result, net::OK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(kPKPHost, "/ssl/google.html")));

  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, false, false, false);
}

IN_PROC_BROWSER_TEST_F(PKPModelClientTest, PKPEnforced) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);

  scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
  net::CertVerifyResult verify_result;
  // PKP requires |is_issued_by_known_root| to be true.
  verify_result.is_issued_by_known_root = true;
  verify_result.verified_cert = cert;
  // Public key hash which does not match the value in the static pin.
  net::HashValue hash(net::HASH_VALUE_SHA256);
  memset(hash.data(), 1, hash.size());
  verify_result.public_key_hashes.push_back(hash);

  mock_cert_verifier()->AddResultForCert(cert, verify_result, net::OK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(kPKPHost, "/ssl/google.html")));
  EXPECT_EQ(security_state::SecurityLevel::DANGEROUS,
            observer.latest_security_level());
}

class SecurityStateLoadingTest : public SecurityStateTabHelperTest {
 public:
  SecurityStateLoadingTest() : SecurityStateTabHelperTest() {}

  SecurityStateLoadingTest(const SecurityStateLoadingTest&) = delete;
  SecurityStateLoadingTest& operator=(const SecurityStateLoadingTest&) = delete;

  ~SecurityStateLoadingTest() override {}

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Tests that navigation state changes cause the security state to be
// updated.
IN_PROC_BROWSER_TEST_F(SecurityStateLoadingTest, NavigationStateChanges) {
  ASSERT_TRUE(https_server_.Start());
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Navigate to an HTTPS page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, false, false,
      false /* expect cert status error */);

  // Navigate to a page that doesn't finish loading. Test that the
  // security state is neutral while the page is loading.
  browser()->OpenURL(
      content::OpenURLParams(
          embedded_test_server()->GetURL("/title1.html"), content::Referrer(),
          WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});
  CheckSecurityInfoForNonCommitted(
      browser()->tab_strip_model()->GetActiveWebContents());
}

// Tests the default security level on blob URLs.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       DefaultSecurityLevelOnBlobUrl) {
  TestBlobOrFilesystemURL("blob", kCreateBlobUrlJavascript,
                          false /* use_secure_inner_origin */);
}

// Same as DefaultSecurityLevelOnBlobUrl, but instead of a blob URL,
// this creates a filesystem URL.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       DefaultSecurityLevelOnFilesystemUrl) {
  TestBlobOrFilesystemURL("filesystem", kCreateFilesystemUrlJavascript,
                          false /* use_secure_inner_origin */);
}

// Tests the default security level on blob URLs with a secure inner origin.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       DefaultSecurityLevelOnSecureBlobUrl) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  TestBlobOrFilesystemURL("blob", kCreateBlobUrlJavascript,
                          true /* use_secure_inner_origin */);
}

// Same as DefaultSecurityLevelOnBlobUrl, but instead of a blob URL,
// this creates a filesystem URL with a secure inner origin.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       DefaultSecurityLevelOnSecureFilesystemUrl) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  TestBlobOrFilesystemURL("filesystem", kCreateFilesystemUrlJavascript,
                          true /* use_secure_inner_origin */);
}

// Tests that the security state for a WebContents is up to date when the
// WebContents is inserted into a Browser's TabStripModel.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, AddedTab) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(tab->GetBrowserContext()));
  content::NavigationController& controller = new_contents->GetController();
  ChromeSecurityStateTabHelper::CreateForWebContents(new_contents.get());
  CheckSecurityInfoForNonCommitted(new_contents.get());
  controller.LoadURL(https_server_.GetURL("/title1.html"), content::Referrer(),
                     ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(content::WaitForLoadStop(new_contents.get()));
  CheckSecurityInfoForSecure(new_contents.get(), security_state::SECURE, false,
                             false, false,
                             false /* expect cert status error */);

  content::WebContents* raw_new_contents = new_contents.get();
  browser()->tab_strip_model()->InsertWebContentsAt(0, std::move(new_contents),
                                                    AddTabTypes::ADD_NONE);
  CheckSecurityInfoForSecure(raw_new_contents, security_state::SECURE, false,
                             false, false,
                             false /* expect cert status error */);
}

class DidChangeVisibleSecurityStateTestWithAutoupgradesDisabled
    : public DidChangeVisibleSecurityStateTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DidChangeVisibleSecurityStateTest::SetUpCommandLine(command_line);
    feature_list.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

// Tests that the WebContentsObserver::DidChangeVisibleSecurityState event fires
// with the current level on HTTP, broken HTTPS, and valid HTTPS pages.
IN_PROC_BROWSER_TEST_F(
    DidChangeVisibleSecurityStateTestWithAutoupgradesDisabled,
    DidChangeVisibleSecurityStateObserver) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  net::EmbeddedTestServer https_test_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_test_server_expired.ServeFilesFromSourceDirectory(
      GetChromeTestDataDir());
  ASSERT_TRUE(https_test_server_expired.Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);

  // Visit an HTTP url.
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), http_url));
  EXPECT_EQ(security_state::SecurityLevel::NONE,
            observer.latest_security_level());

  // Visit an (otherwise valid) HTTPS page that displays mixed content.
  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair());

  GURL mixed_content_url(https_server_.GetURL(replacement_path));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), mixed_content_url));

  EXPECT_EQ(security_state::SecurityLevel::WARNING,
            observer.latest_security_level());

  // Visit a broken HTTPS url.
  GURL expired_url(https_test_server_expired.GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), expired_url));

  // An interstitial should show, and an event for the lock icon on the
  // interstitial should fire.
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingInterstitial(web_contents));
  EXPECT_EQ(security_state::SecurityLevel::DANGEROUS,
            observer.latest_security_level());

  // Before clicking through, navigate to a different page, and then go
  // back to the interstitial.
  GURL valid_https_url(https_server_.GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), valid_https_url));
  EXPECT_EQ(security_state::SecurityLevel::SECURE,
            observer.latest_security_level());

  // After going back to the interstitial, an event for a broken lock
  // icon should fire again.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), expired_url));
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingInterstitial(web_contents));
  EXPECT_EQ(security_state::SecurityLevel::DANGEROUS,
            observer.latest_security_level());

  // Since the next expected level is the same as the previous, clear
  // the observer (to make sure that the event fires twice and we don't
  // just see the previous event's level).
  observer.ClearLatestSecurityLevel();

  // Other conditions cannot be tested on this host after clicking
  // through because once the interstitial is clicked through, all URLs
  // for this host will remain in a broken state.
  ProceedThroughInterstitial(web_contents);
  EXPECT_EQ(security_state::SecurityLevel::DANGEROUS,
            observer.latest_security_level());
}

// Visit a valid HTTPS page, then a broken HTTPS page, and then go back,
// and test that the observed security level matches.
IN_PROC_BROWSER_TEST_F(DidChangeVisibleSecurityStateTest,
                       DidChangeVisibleSecurityStateObserverGoBack) {
  ASSERT_TRUE(https_server_.Start());

  net::EmbeddedTestServer https_test_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_test_server_expired.ServeFilesFromSourceDirectory(
      GetChromeTestDataDir());
  ASSERT_TRUE(https_test_server_expired.Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);

  // Visit a valid HTTPS url.
  GURL valid_https_url(https_server_.GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), valid_https_url));
  EXPECT_EQ(security_state::SecurityLevel::SECURE,
            observer.latest_security_level());

  // Navigate to a bad HTTPS page on a different host, and then click
  // Back to verify that the previous good security level is seen again.
  GURL expired_https_url(https_test_server_expired.GetURL("/title1.html"));
  GURL::Replacements replace_host;
  replace_host.SetHostStr("www.example_broken.test");
  GURL https_url_different_host =
      expired_https_url.ReplaceComponents(replace_host);

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), https_url_different_host));

  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingInterstitial(web_contents));
  EXPECT_EQ(security_state::SecurityLevel::DANGEROUS,
            observer.latest_security_level());
  ProceedThroughInterstitial(web_contents);
  EXPECT_EQ(security_state::SecurityLevel::DANGEROUS,
            observer.latest_security_level());

  content::LoadStopObserver back_nav_load_observer(web_contents);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  back_nav_load_observer.Wait();

  EXPECT_EQ(security_state::SecurityLevel::SECURE,
            observer.latest_security_level());
}

// Tests that the Not Secure chip does not show for error pages on http:// URLs.
// Regression test for https://crbug.com/760647.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperIncognitoTest, HttpErrorPage) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Disable HTTPS upgrades on nonexistent.test for this test to work.
  ScopedAllowHttpForHostnamesForTesting scoped_allow_http(
      {"nonexistent.test"}, browser()->profile()->GetPrefs());

  // Navigate to a URL that results in an error page. Even though the displayed
  // URL is http://, there shouldn't be a Not Secure warning because the browser
  // hasn't really navigated to an http:// page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("http://nonexistent.test:17")));
  // Sanity-check that it is indeed an error page.
  content::NavigationEntry* entry = browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetController()
                                        .GetVisibleEntry();
  ASSERT_TRUE(entry);
  ASSERT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());

  EXPECT_EQ(security_state::NONE, helper->GetSecurityLevel());
}

// Tests that the security level form submission histogram is logged correctly.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, FormSecurityLevelHistogram) {
  const char kHistogramName[] = "Security.SecurityLevel.FormSubmission";
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  base::HistogramTester histograms;
  // Create a server with an expired certificate for the form to target.
  net::EmbeddedTestServer broken_https_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  broken_https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  broken_https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(broken_https_server.Start());

  // Make the form target the expired certificate server.
  net::HostPortPair host_port_pair = net::HostPortPair::FromURL(
      broken_https_server.GetURL("/ssl/google.html"));
  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_form_targeting_insecure_url.html", host_port_pair);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(replacement_path)));
  content::TestNavigationObserver navigation_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.getElementById('submit').click();"));
  navigation_observer.Wait();
  // Check that the histogram count logs the security level of the page
  // containing the form, not of the form target page.
  histograms.ExpectUniqueSample(kHistogramName, security_state::SECURE, 1);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       MixedFormsShowLockIfWarningsAreEnabled) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server_.GetURL("/ssl/page_displays_insecure_form.html")));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, false, false,
      false /* expect cert status error */);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       MixedFormsDontShowLockIfWarningsAreDisabledByPolicy) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kMixedFormsWarningsEnabled, false);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server_.GetURL("/ssl/page_displays_insecure_form.html")));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, false, false, false,
      false /* expect cert status error */);
}

class SignedExchangeSecurityStateTest : public CertVerifierBrowserTest {
 public:
  SignedExchangeSecurityStateTest() = default;
  ~SignedExchangeSecurityStateTest() override = default;
  SignedExchangeSecurityStateTest(const SignedExchangeSecurityStateTest&) =
      delete;
  SignedExchangeSecurityStateTest& operator=(
      const SignedExchangeSecurityStateTest&) = delete;

 protected:
  void PreRunTestOnMainThread() override {
    CertVerifierBrowserTest::PreRunTestOnMainThread();

    sxg_test_helper_.InstallMockCert(mock_cert_verifier());
    sxg_test_helper_.InstallMockCertChainInterceptor();
  }

 private:
  void SetUp() override {
    sxg_test_helper_.SetUp();

    CertVerifierBrowserTest::SetUp();
  }

  void TearDownOnMainThread() override {
    sxg_test_helper_.TearDownOnMainThread();
    CertVerifierBrowserTest::TearDownOnMainThread();
  }

  content::SignedExchangeBrowserTestHelper sxg_test_helper_;
};

IN_PROC_BROWSER_TEST_F(SignedExchangeSecurityStateTest, SecurityLevelIsSecure) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  const GURL inner_url("https://test.example.org/test/");
  const GURL sxg_url =
      embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");
  std::u16string expected_title = base::ASCIIToUTF16(inner_url.spec());
  content::TitleWatcher title_watcher(contents, expected_title);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), sxg_url));
  // The inner content of test.example.org_test.sxg has
  // "<script> document.title = document.location.href; </script>".
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  ASSERT_EQ(inner_url, contents->GetLastCommittedURL());

  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false /* expect_sha1_in_chain */,
      false /* expect_displayed_mixed_content */,
      false /* expect_ran_mixed_content */, false /* expect_cert_error */);
}

// TODO(crbug.com/40921701): This test is failing on Mac12 Tests.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SecurityLevelIsSecureAfterPrefetch \
  DISABLED_SecurityLevelIsSecureAfterPrefetch
#else
#define MAYBE_SecurityLevelIsSecureAfterPrefetch \
  SecurityLevelIsSecureAfterPrefetch
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(SignedExchangeSecurityStateTest,
                       MAYBE_SecurityLevelIsSecureAfterPrefetch) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  const GURL inner_url("https://test.example.org/test/");
  const GURL sxg_url =
      embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");

  // prefetch.html prefetches the signed exchange. And the signed exchange will
  // be served from PrefetchedSignedExchangeCache.
  const GURL prefetch_html_url = embedded_test_server()->GetURL(
      std::string("/sxg/prefetch.html#") + sxg_url.spec());
  {
    std::u16string expected_title = u"OK";
    content::TitleWatcher title_watcher(contents, expected_title);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), prefetch_html_url));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  {
    std::u16string expected_title = base::ASCIIToUTF16(inner_url.spec());
    content::TitleWatcher title_watcher(contents, expected_title);
    // Execute the JavaScript code to trigger the followup navigation from the
    // current page.
    EXPECT_TRUE(content::ExecJs(
        contents,
        base::StringPrintf("location.href = '%s';", sxg_url.spec().c_str())));
    // The inner content of test.example.org_test.sxg has
    // "<script> document.title = document.location.href; </script>".
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
    ASSERT_EQ(inner_url, contents->GetLastCommittedURL());
  }

  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false /* expect_sha1_in_chain */,
      false /* expect_displayed_mixed_content */,
      false /* expect_ran_mixed_content */, false /* expect_cert_error */);
}

class SecurityStateTabHelperPrerenderTest : public SecurityStateTabHelperTest {
 public:
  SecurityStateTabHelperPrerenderTest()
      : prerender_helper_(base::BindRepeating(
            &SecurityStateTabHelperPrerenderTest::web_contents,
            base::Unretained(this))) {
    // Disable mixed content autoupgrading to allow mixed content to load and
    // test that mixed content is recorded correctly on the prerender navigation
    // entry.
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
  }
  ~SecurityStateTabHelperPrerenderTest() override = default;
  SecurityStateTabHelperPrerenderTest(
      const SecurityStateTabHelperPrerenderTest&) = delete;
  SecurityStateTabHelperPrerenderTest& operator=(
      const SecurityStateTabHelperPrerenderTest&) = delete;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(&https_server_);
    SecurityStateTabHelperTest::SetUp();
  }

  void SetUpOnMainThread() override {
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    SecurityStateTabHelperTest::SetUpOnMainThread();
  }

  void SetUpMockCertVerifierForHttpsServer(net::EmbeddedTestServer& test_server,
                                           net::CertStatus cert_status,
                                           int net_result) {
    scoped_refptr<net::X509Certificate> cert(test_server.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.is_issued_by_known_root = false;
    verify_result.verified_cert = cert;
    verify_result.cert_status = cert_status;

    mock_cert_verifier()->AddResultForCert(cert, verify_result, net_result);
  }

  content::WebContents* web_contents() { return web_contents_; }

 protected:
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_ =
      nullptr;
  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that starting a prerender will not modify the existing security info
// and navigating to a previously prerendered URL may trigger a failure.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperPrerenderTest, InvalidPrerender) {
  // Start test server.
  auto test_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  test_server->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  test_server->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(test_server->Start());

  // Setup a mock certificate verifier.
  SetUpMockCertVerifierForHttpsServer(*test_server, 0, net::OK);

  // Load a valid HTTPS page.
  auto primary_url = test_server->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), primary_url));
  CheckSecurityInfoForSecure(web_contents(), security_state::SECURE, false,
                             false, false,
                             false /* expect cert status error */);

  // Shutdown server.
  ASSERT_TRUE(test_server->ShutdownAndWaitUntilComplete());

  // Start test server with invalid certificate.
  auto port = test_server->port();
  test_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  test_server->SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  test_server->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(test_server->Start(port));

  // Setup a mock certificate verifier.
  SetUpMockCertVerifierForHttpsServer(
      *test_server, net::CERT_STATUS_DATE_INVALID, net::ERR_CERT_DATE_INVALID);

  // Try to prerender the page with an invalid certificate.
  auto prerender_url = test_server->GetURL("/title1.html");
  content::test::PrerenderHostRegistryObserver registry_observer(
      *web_contents());
  content::NavigationHandleObserver nav_observer(web_contents(), prerender_url);
  prerender_helper_.AddPrerenderAsync(prerender_url);

  // Ensure that the prerender has started.
  registry_observer.WaitForTrigger(prerender_url);
  auto prerender_id = prerender_helper_.GetHostForUrl(prerender_url);
  EXPECT_TRUE(prerender_id);
  content::test::PrerenderHostObserver host_observer(*web_contents(),
                                                     prerender_id);

  // Since the prerender has not yet activated, the state should still be
  // secure.
  CheckSecurityInfoForSecure(web_contents(), security_state::SECURE, false,
                             false, false,
                             false /* expect cert status error */);

  // The prerender should be abandoned since the it commits an interstitial,
  // wait for the PrerenderHost to be destroyed.
  host_observer.WaitForDestroyed();
  EXPECT_TRUE(prerender_helper_.GetHostForUrl(prerender_url).is_null());

  // Navigate to prerender_url and expect cert error.
  prerender_helper_.NavigatePrimaryPage(prerender_url);
  CheckSecurityInfoForSecure(web_contents(), security_state::DANGEROUS, false,
                             false, false, true /* expect cert status error */);

  // Make sure that the prerender was not activated.
  EXPECT_FALSE(host_observer.was_activated());

  // Shutdown server.
  ASSERT_TRUE(test_server->ShutdownAndWaitUntilComplete());
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperPrerenderTest,
                       PrerenderedMixedContent) {
  // Start test server.
  auto test_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  test_server->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  test_server->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(test_server->Start());

  // Setup a mock certificate verifier.
  SetUpMockCertVerifierForHttpsServer(*test_server, 0, net::OK);

  // Load a valid HTTPS page.
  auto primary_url = test_server->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), primary_url));
  CheckSecurityInfoForSecure(web_contents(), security_state::SECURE, false,
                             false, false,
                             false /* expect cert status error */);

  // Load prerender page that displays mixed content.
  auto prerender_url =
      test_server->GetURL("/ssl/page_displays_insecure_content.html");
  prerender_helper_.AddPrerender(prerender_url);
  auto prerender_id = prerender_helper_.GetHostForUrl(prerender_url);
  EXPECT_TRUE(prerender_id);
  content::test::PrerenderHostObserver host_observer(*web_contents(),
                                                     prerender_id);

  // Since the prerender has not yet activated, the state should still be
  // secure.
  CheckSecurityInfoForSecure(web_contents(), security_state::SECURE, false,
                             false, false,
                             false /* expect cert status error */);

  // Navigate to prerender_url and expect warning.
  prerender_helper_.NavigatePrimaryPage(prerender_url);
  CheckSecurityInfoForSecure(web_contents(), security_state::WARNING, false,
                             true, false, false /* expect cert status error */);

  // Make sure that the prerender was activated.
  EXPECT_TRUE(host_observer.was_activated());

  // Shutdown server.
  ASSERT_TRUE(test_server->ShutdownAndWaitUntilComplete());
}

// For tests which use fenced frame.
class SecurityStateTabHelperFencedFrameTest
    : public SecurityStateTabHelperTest,
      public testing::WithParamInterface<bool> {
 public:
  SecurityStateTabHelperFencedFrameTest() {
    // Enable or disable MixedContentAutoupgrade feature based on the test
    // parameter.
    scoped_feature_list_.InitWithFeatureState(
        blink::features::kMixedContentAutoupgrade, GetParam());
  }
  ~SecurityStateTabHelperFencedFrameTest() override = default;
  SecurityStateTabHelperFencedFrameTest(
      const SecurityStateTabHelperFencedFrameTest&) = delete;

  SecurityStateTabHelperFencedFrameTest& operator=(
      const SecurityStateTabHelperFencedFrameTest&) = delete;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  bool IsAutoupgradeEnabled() { return GetParam(); }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SecurityStateTabHelperFencedFrameTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperFencedFrameTest,
                       ChangeSecurityStateWhenLoadingInsecureContent) {
  // Setup a mock certificate verifier.
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Load a valid HTTPS page.
  auto primary_url = https_server_.GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), primary_url));
  CheckSecurityInfoForSecure(web_contents(), security_state::SECURE, false,
                             false, false,
                             false /* expect cert status error */);

  // Create a fenced frame with a page that displays insecure content.
  GURL fenced_frame_url =
      https_server_.GetURL("/ssl/page_displays_insecure_content.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  EXPECT_NE(nullptr, fenced_frame_host);

  if (IsAutoupgradeEnabled()) {
    // The security state of the web contents should not be changed because
    // the MixedContentAutoupgrade feature changes the internal insecure url
    // from http to https.
    CheckSecurityInfoForSecure(web_contents(), security_state::SECURE, false,
                               false, false,
                               false /* expect cert status error */);
  } else {
    // The security state of the web contents should be changed to WARNING.
    CheckSecurityInfoForSecure(web_contents(), security_state::WARNING, false,
                               true, false,
                               false /* expect cert status error */);
  }
}

IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperFencedFrameTest,
                       LoadFencedFrameViaInsecureURL) {
  // Setup a mock certificate verifier.
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Load a valid HTTPS page.
  auto primary_url = https_server_.GetURL("/empty.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), primary_url));
  CheckSecurityInfoForSecure(web_contents(), security_state::SECURE, false,
                             false, false,
                             false /* expect cert status error */);

  // Create a fenced frame with an insecure url.
  GURL fenced_frame_url =
      embedded_test_server()->GetURL("b.com", "/fenced_frames/title1.html");

  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  EXPECT_NE(nullptr, fenced_frame_host);
  // Check that nothing has been loaded in the fenced frame.
  EXPECT_EQ(
      0, content::EvalJs(fenced_frame_host, "document.body.childElementCount"));

  // Since we are blocking http content in a fenced frame, the security
  // indicator should not change, and there should be no mixed content loaded.
  CheckSecurityInfoForSecure(web_contents(), security_state::SECURE, false,
                             false, false /* expect no mixed content loaded */,
                             false /* expect cert status error */);
}

}  // namespace

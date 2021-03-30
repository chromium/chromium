// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/security_state_tab_helper.h"

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/reputation/reputation_web_contents_observer.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ssl/tls_deprecation_test_utils.h"
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
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/prefs/pref_service.h"
#include "components/reputation/core/safety_tip_test_utils.h"
#include "components/safe_browsing/content/password_protection/password_protection_request_content.h"
#include "components/safe_browsing/content/password_protection/password_protection_test_util.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/password_protection/metrics_util.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/core/features.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/security_state/content/ssl_status_input_event_data.h"
#include "components/security_state/core/features.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/security_style_explanation.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_type.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/signed_exchange_browser_test_helper.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "mojo/public/cpp/bindings/remote.h"
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
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using password_manager::metrics_util::PasswordType;
using safe_browsing::LoginReputationClientResponse;
using safe_browsing::RequestOutcome;

const char kCreateFilesystemUrlJavascript[] =
    "window.webkitRequestFileSystem(window.TEMPORARY, 4096, function(fs) {"
    "  fs.root.getFile('test.html', {create: true}, function(fileEntry) {"
    "    fileEntry.createWriter(function(writer) {"
    "      writer.onwriteend = function(e) {"
    "        window.domAutomationController.send(fileEntry.toURL());"
    "      };"
    "      var blob = new Blob(['<html>hello</html>'], {type: 'text/html'});"
    "      writer.write(blob);"
    "    });"
    "  });"
    "});";

const char kCreateBlobUrlJavascript[] =
    "var blob = new Blob(['<html>hello</html>'],"
    "                    {type: 'text/html'});"
    "window.domAutomationController.send(URL.createObjectURL(blob));";

enum CertificateStatus { VALID_CERTIFICATE, INVALID_CERTIFICATE };

const char kTestCertificateIssuerName[] = "Test Root CA";

bool IsShowingInterstitial(content::WebContents* tab) {
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  if (!helper) {
    return false;
  }
  return helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting() !=
         nullptr;
}

// Waits until an interstitial is showing.
void WaitForInterstitial(content::WebContents* tab) {
  ASSERT_TRUE(IsShowingInterstitial(tab));
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetMainFrame()));
}

// Inject a script into every frame in the page. Used by tests that check for
// visible password fields to wait for notifications about these
// fields. Notifications about visible password fields are queued at the end of
// the event loop, so waiting for a dummy script to run ensures that these
// notifications have been sent.
void InjectScript(content::WebContents* contents) {
  // Any frame in the page might have a password field, so inject scripts into
  // all of them to ensure that notifications from all of them have been sent.
  for (auto* frame : contents->GetAllFrames()) {
    bool js_result = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        frame, "window.domAutomationController.send(true);", &js_result));
    EXPECT_TRUE(js_result);
  }
}

// A delegate class that allows emulating selection of a file for an
// INPUT TYPE=FILE form field.
class FileChooserDelegate : public content::WebContentsDelegate {
 public:
  // Constructs a WebContentsDelegate that mocks a file dialog.
  // The mocked file dialog will always reply that the user selected |file|.
  explicit FileChooserDelegate(const base::FilePath& file,
                               base::OnceClosure callback)
      : file_(file), callback_(std::move(callback)) {}

  // Copy of the params passed to RunFileChooser.
  const blink::mojom::FileChooserParams& params() const { return *params_; }

  // WebContentsDelegate:
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override {
    // Send the selected file to the renderer process.
    std::vector<blink::mojom::FileChooserFileInfoPtr> files;
    files.push_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
        blink::mojom::NativeFileInfo::New(file_, std::u16string())));
    listener->FileSelected(std::move(files), base::FilePath(),
                           blink::mojom::FileChooserParams::Mode::kOpen);

    params_ = params.Clone();
    std::move(callback_).Run();
  }

 private:
  base::FilePath file_;
  base::OnceClosure callback_;
  blink::mojom::FileChooserParamsPtr params_;

  DISALLOW_COPY_AND_ASSIGN(FileChooserDelegate);
};

// A WebContentsObserver useful for testing the DidChangeVisibleSecurityState()
// method: it keeps track of the latest security style and explanation that was
// fired.
class SecurityStyleTestObserver : public content::WebContentsObserver {
 public:
  explicit SecurityStyleTestObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        latest_security_style_(blink::SecurityStyle::kUnknown) {}
  ~SecurityStyleTestObserver() override {}

  void DidChangeVisibleSecurityState() override {
    content::SecurityStyleExplanations explanations;
    latest_security_style_ = web_contents()->GetDelegate()->GetSecurityStyle(
        web_contents(), &explanations);
    latest_explanations_ = explanations;
    run_loop_.Quit();
  }

  void WaitForDidChangeVisibleSecurityState() { run_loop_.Run(); }

  blink::SecurityStyle latest_security_style() const {
    return latest_security_style_;
  }

  const content::SecurityStyleExplanations& latest_explanations() const {
    return latest_explanations_;
  }

  void ClearLatestSecurityStyleAndExplanations() {
    latest_security_style_ = blink::SecurityStyle::kUnknown;
    latest_explanations_ = content::SecurityStyleExplanations();
  }

 private:
  blink::SecurityStyle latest_security_style_;
  content::SecurityStyleExplanations latest_explanations_;
  base::RunLoop run_loop_;
  DISALLOW_COPY_AND_ASSIGN(SecurityStyleTestObserver);
};

// Check that |observer|'s latest event was for an expired certificate
// and that it saw the proper SecurityStyle and explanations.
void CheckBrokenSecurityStyle(const SecurityStyleTestObserver& observer,
                              int error,
                              Browser* browser,
                              net::X509Certificate* expected_cert) {
  EXPECT_EQ(blink::SecurityStyle::kInsecureBroken,
            observer.latest_security_style());

  const content::SecurityStyleExplanations& expired_explanation =
      observer.latest_explanations();
  EXPECT_EQ(0u, expired_explanation.neutral_explanations.size());
  ASSERT_EQ(1u, expired_explanation.insecure_explanations.size());
  EXPECT_TRUE(expired_explanation.info_explanations.empty());

  // Check that the summary and description are as expected.
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_CERTIFICATE_CHAIN_ERROR),
            expired_explanation.insecure_explanations[0].summary);

  std::u16string error_string = base::UTF8ToUTF16(net::ErrorToString(error));
  EXPECT_EQ(l10n_util::GetStringFUTF8(
                IDS_CERTIFICATE_CHAIN_ERROR_DESCRIPTION_FORMAT, error_string),
            expired_explanation.insecure_explanations[0].description);

  // Check the associated certificate.
  net::X509Certificate* cert = browser->tab_strip_model()
                                   ->GetActiveWebContents()
                                   ->GetController()
                                   .GetVisibleEntry()
                                   ->GetSSL()
                                   .certificate.get();
  EXPECT_TRUE(cert->EqualsExcludingChain(expected_cert));
  EXPECT_TRUE(expired_explanation.insecure_explanations[0].certificate);
}

// Checks that the given |explanation| contains an appropriate
// explanation if the certificate status is valid.
void CheckSecureCertificateExplanation(
    const content::SecurityStyleExplanation& explanation,
    Browser* browser,
    net::X509Certificate* expected_cert) {
  ASSERT_EQ(kTestCertificateIssuerName,
            expected_cert->issuer().GetDisplayName());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_VALID_SERVER_CERTIFICATE),
            explanation.summary);
  EXPECT_EQ(
      l10n_util::GetStringFUTF8(IDS_VALID_SERVER_CERTIFICATE_DESCRIPTION,
                                base::UTF8ToUTF16(kTestCertificateIssuerName)),
      explanation.description);
  net::X509Certificate* cert = browser->tab_strip_model()
                                   ->GetActiveWebContents()
                                   ->GetController()
                                   .GetLastCommittedEntry()
                                   ->GetSSL()
                                   .certificate.get();
  EXPECT_TRUE(cert->EqualsExcludingChain(expected_cert));
  EXPECT_TRUE(explanation.certificate);
}

// Checks that the given |explanation| contains an appropriate
// explanation that the connection is secure.
void CheckSecureConnectionExplanation(
    const content::SecurityStyleExplanation& explanation,
    Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      SecurityStateTabHelper::FromWebContents(web_contents)
          ->GetVisibleSecurityState();

  const char *protocol, *key_exchange, *cipher, *mac;
  int ssl_version = net::SSLConnectionStatusToVersion(
      visible_security_state->connection_status);
  net::SSLVersionToString(&protocol, ssl_version);
  bool is_aead, is_tls13;
  uint16_t cipher_suite = net::SSLConnectionStatusToCipherSuite(
      visible_security_state->connection_status);
  net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead,
                               &is_tls13, cipher_suite);
  // Modern configurations are always AEADs and specify groups.
  EXPECT_TRUE(is_aead);
  EXPECT_EQ(nullptr, mac);
  ASSERT_NE(0, visible_security_state->key_exchange_group);
  const char* key_exchange_group =
      SSL_get_curve_name(visible_security_state->key_exchange_group);

  // The description should summarize the settings.
  EXPECT_NE(std::string::npos, explanation.description.find(protocol));
  if (key_exchange == nullptr)
    EXPECT_TRUE(is_tls13);
  else
    EXPECT_NE(std::string::npos, explanation.description.find(key_exchange));
  EXPECT_NE(std::string::npos,
            explanation.description.find(key_exchange_group));
  EXPECT_NE(std::string::npos, explanation.description.find(cipher));

  // There should be no recommendations to provide.
  EXPECT_EQ(0u, explanation.recommendations.size());
}

// Checks that the given |explanation| contains an appropriate
// explanation that the subresources are secure.
void CheckSecureSubresourcesExplanation(
    const content::SecurityStyleExplanation& explanation) {
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SECURE_RESOURCES_SUMMARY),
            explanation.summary);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SECURE_RESOURCES_DESCRIPTION),
            explanation.description);
}

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

  ~SecurityStateTabHelperTest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        base::nullopt);
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

    ui_test_utils::NavigateToURL(
        browser(),
        GetURLWithNonLocalHostname(
            use_secure_inner_origin ? &https_server_ : embedded_test_server(),
            "/empty.html"));

    // Create a URL and navigate to it.
    std::string blob_or_filesystem_url;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        contents, javascript, &blob_or_filesystem_url));
    EXPECT_TRUE(GURL(blob_or_filesystem_url).SchemeIs(scheme));

    ui_test_utils::NavigateToURL(browser(), GURL(blob_or_filesystem_url));
    InjectScript(contents);

    content::NavigationEntry* entry =
        contents->GetController().GetVisibleEntry();
    ASSERT_TRUE(entry);

    EXPECT_EQ(use_secure_inner_origin ? security_state::NONE
                                      : security_state::WARNING,
              helper->GetSecurityLevel());
  }

  net::EmbeddedTestServer https_server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityStateTabHelperTest);
};

// Same as SecurityStateTabHelperTest, but with Incognito enabled.
class SecurityStateTabHelperIncognitoTest : public SecurityStateTabHelperTest {
 public:
  SecurityStateTabHelperIncognitoTest() : SecurityStateTabHelperTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SecurityStateTabHelperTest::SetUpCommandLine(command_line);
    // Test should run Incognito.
    command_line->AppendSwitch(switches::kIncognito);
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(SecurityStateTabHelperIncognitoTest);
};

class DidChangeVisibleSecurityStateTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  DidChangeVisibleSecurityStateTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Browser will both run and display insecure content.
    command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  net::EmbeddedTestServer https_server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DidChangeVisibleSecurityStateTest);
};

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, HttpPage) {
  GURL http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/ssl/google.html");
  ui_test_utils::NavigateToURL(browser(), http_url);
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

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, false, false,
      false /* expect cert status error */);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, DevToolsPage) {
  GURL devtools_url("devtools://devtools/bundled/");
  ui_test_utils::NavigateToURL(browser(), devtools_url);
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
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
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
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
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
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, true, false, false,
      true /* expect cert status error */);

  const content::SecurityStyleExplanations& interstitial_explanation =
      observer.latest_explanations();
  ASSERT_EQ(2u, interstitial_explanation.insecure_explanations.size());
  ASSERT_EQ(0u, interstitial_explanation.neutral_explanations.size());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SHA1),
            interstitial_explanation.insecure_explanations[0].summary);

  ProceedThroughInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents());

  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, true, false, false,
      true /* expect cert status error */);

  const content::SecurityStyleExplanations& page_explanation =
      observer.latest_explanations();
  ASSERT_EQ(2u, page_explanation.insecure_explanations.size());
  ASSERT_EQ(0u, page_explanation.neutral_explanations.size());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SHA1),
            page_explanation.insecure_explanations[0].summary);
}

// Test security state for a SHA-1 certificate that is allowed by policy.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, SHA1CertificateWarning) {
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT,
                                      net::OK);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, true, false, false,
      false /* expect cert status error */);

  const content::SecurityStyleExplanations& explanation =
      observer.latest_explanations();

  ASSERT_EQ(0u, explanation.insecure_explanations.size());
  ASSERT_EQ(1u, explanation.neutral_explanations.size());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SHA1),
            explanation.neutral_explanations[0].summary);
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
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::WARNING, false, true, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that displays mixed content dynamically.
  replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html", replacement_pair);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, false, false,
      false /* expect cert status error */);
  // Load the insecure image.
  bool js_result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(), "loadBadImage();",
      &js_result));
  EXPECT_TRUE(js_result);
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::WARNING, false, true, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs mixed content.
  replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html", replacement_pair);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false, false, true,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs and displays mixed content.
  replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_and_displays_insecure_content.html", replacement_pair);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
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
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
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
// TODO(crbug.com/1026464): Once that launch is finished and other tests run
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
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::WARNING, false, true, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that displays mixed content dynamically.
  replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html", replacement_pair);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, false, false,
      false /* expect cert status error */);
  // Load the insecure image.
  bool js_result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(), "loadBadImage();",
      &js_result));
  EXPECT_TRUE(js_result);
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
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/title1.html"));
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
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/title1.html"));
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
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/title1.html"));
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
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, true, true, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that displays mixed content dynamically.
  replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html", replacement_pair);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, true, false, false,
      false /* expect cert status error */);
  // Load the insecure image.
  bool js_result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(), "loadBadImage();",
      &js_result));
  EXPECT_TRUE(js_result);
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, true, true, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs mixed content.
  replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html", replacement_pair);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, true, false, true,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs and displays mixed content.
  replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_and_displays_insecure_content.html", replacement_pair);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
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
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, false, false,
      false /* expect cert status error */);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTestWithAutoupgradesDisabled,
                       BrokenHTTPS) {
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_DATE_INVALID,
                                      net::ERR_CERT_DATE_INVALID);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
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
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false, true, false,
      true /* expect cert status error */);
}

// Tests that the security style of HTTP pages is set properly.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, SecurityStyleForHttpPage) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStyleTestObserver observer(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  GURL http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html");
  ui_test_utils::NavigateToURL(browser(), http_url);
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(blink::SecurityStyle::kInsecure, observer.latest_security_style());

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

  ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,<html></html>"));
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());

  // Ensure that WebContentsObservers don't show an incorrect Form Not Secure
  // explanation. Regression test for https://crbug.com/691412.
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(blink::SecurityStyle::kInsecure, observer.latest_security_style());

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

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
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

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
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

// TODO(https://crbug.com/333943): Remove these tests when FTP support is
// removed.
class SecurityStateTabHelperTestWithFtpEnabled
    : public SecurityStateTabHelperTest {
 public:
  SecurityStateTabHelperTestWithFtpEnabled() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kFtpProtocol);
  }
  ~SecurityStateTabHelperTestWithFtpEnabled() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the security level of ftp: URLs is always downgraded to
// WARNING.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTestWithFtpEnabled,
                       SecurityLevelDowngradedOnFtpUrl) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStyleTestObserver observer(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(browser(), GURL("ftp://example.test/"));
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());

  // Ensure that WebContentsObservers don't show an incorrect Form Not Secure
  // explanation. Regression test for https://crbug.com/691412.
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(blink::SecurityStyle::kInsecure, observer.latest_security_style());

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(content::SSLStatus::NORMAL_CONTENT, entry->GetSSL().content_status);
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
    content::GetNetworkService()->BindTestInterface(
        network_service_test.BindNewPipeAndPassReceiver());
    network_service_test->SetTransportSecurityStateSource(0);

    // This test class intentionally does not call the parent
    // TearDownOnMainThread.
  }

 private:
  void EnableStaticPins() {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;

    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterface(
        network_service_test.BindNewPipeAndPassReceiver());
    // The tests don't depend on reporting, so the port doesn't matter.
    network_service_test->SetTransportSecurityStateSource(80);

    content::StoragePartition* partition =
        content::BrowserContext::GetDefaultStoragePartition(
            browser()->profile());
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

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(kPKPHost, "/ssl/google.html"));

  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, false, false, false);

  const content::SecurityStyleExplanations& explanation =
      observer.latest_explanations();
  EXPECT_FALSE(explanation.info_explanations.empty());
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

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(kPKPHost, "/ssl/google.html"));
  CheckBrokenSecurityStyle(observer, net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN,
                           browser(), cert.get());
}

class SecurityStateLoadingTest : public SecurityStateTabHelperTest {
 public:
  SecurityStateLoadingTest() : SecurityStateTabHelperTest() {}
  ~SecurityStateLoadingTest() override {}

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityStateLoadingTest);
};

// Tests that navigation state changes cause the security state to be
// updated.
IN_PROC_BROWSER_TEST_F(SecurityStateLoadingTest, NavigationStateChanges) {
  ASSERT_TRUE(https_server_.Start());
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Navigate to an HTTPS page.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, false, false,
      false /* expect cert status error */);

  // Navigate to a page that doesn't finish loading. Test that the
  // security state is neutral while the page is loading.
  browser()->OpenURL(content::OpenURLParams(
      embedded_test_server()->GetURL("/title1.html"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
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

// Tests that the security level of a HTTP page is not downgraded when a form
// field is modified by JavaScript.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       SecurityLevelNotDowngradedAfterScriptModification) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithNonLocalHostname(embedded_test_server(),
                                 "/textinput/focus_input_on_load.html"));
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());

  // Verify a value set operation isn't treated as user-input.
  EXPECT_TRUE(content::ExecuteScript(
      contents, "document.getElementById('text_id').value='v';"));
  InjectScript(contents);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(security_state::WARNING, helper->GetSecurityLevel());

  // Verify an InsertText operation isn't treated as user-input.
  EXPECT_TRUE(content::ExecuteScript(
      contents, "document.execCommand('InsertText',false,'a');"));
  InjectScript(contents);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(security_state::WARNING, helper->GetSecurityLevel());
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
  SecurityStateTabHelper::CreateForWebContents(new_contents.get());
  CheckSecurityInfoForNonCommitted(new_contents.get());
  controller.LoadURL(https_server_.GetURL("/title1.html"), content::Referrer(),
                     ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(content::WaitForLoadStop(new_contents.get()));
  CheckSecurityInfoForSecure(new_contents.get(), security_state::SECURE, false,
                             false, false,
                             false /* expect cert status error */);

  content::WebContents* raw_new_contents = new_contents.get();
  browser()->tab_strip_model()->InsertWebContentsAt(0, std::move(new_contents),
                                                    TabStripModel::ADD_NONE);
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
// with the current style on HTTP, broken HTTPS, and valid HTTPS pages.
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
  ui_test_utils::NavigateToURL(browser(), http_url);
  EXPECT_EQ(blink::SecurityStyle::kNeutral, observer.latest_security_style());
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().insecure_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().secure_explanations.size());
  EXPECT_FALSE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  // Localhost is considered a secure origin, so we expect the summary to
  // reflect this.
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_NON_CRYPTO_SECURE_SUMMARY),
            observer.latest_explanations().summary);

  // Visit an (otherwise valid) HTTPS page that displays mixed content.
  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair());

  GURL mixed_content_url(https_server_.GetURL(replacement_path));
  ui_test_utils::NavigateToURL(browser(), mixed_content_url);

  EXPECT_EQ(blink::SecurityStyle::kInsecure, observer.latest_security_style());

  const content::SecurityStyleExplanations& mixed_content_explanation =
      observer.latest_explanations();
  ASSERT_EQ(1u, mixed_content_explanation.neutral_explanations.size());
  ASSERT_EQ(0u, mixed_content_explanation.insecure_explanations.size());
  ASSERT_EQ(2u, mixed_content_explanation.secure_explanations.size());
  CheckSecureCertificateExplanation(
      mixed_content_explanation.secure_explanations[0], browser(),
      https_server_.GetCertificate().get());
  CheckSecureConnectionExplanation(
      mixed_content_explanation.secure_explanations[1], browser());
  EXPECT_TRUE(mixed_content_explanation.scheme_is_cryptographic);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_TRUE(observer.latest_explanations().summary.empty());

  // Visit a broken HTTPS url.
  GURL expired_url(https_test_server_expired.GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), expired_url);

  // An interstitial should show, and an event for the lock icon on the
  // interstitial should fire.
  WaitForInterstitial(web_contents);
  CheckBrokenSecurityStyle(observer, net::ERR_CERT_DATE_INVALID, browser(),
                           https_test_server_expired.GetCertificate().get());
  ASSERT_EQ(2u, observer.latest_explanations().secure_explanations.size());
  CheckSecureConnectionExplanation(
      observer.latest_explanations().secure_explanations[0], browser());
  CheckSecureSubresourcesExplanation(
      observer.latest_explanations().secure_explanations[1]);
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_TRUE(observer.latest_explanations().summary.empty());

  // Before clicking through, navigate to a different page, and then go
  // back to the interstitial.
  GURL valid_https_url(https_server_.GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), valid_https_url);
  EXPECT_EQ(blink::SecurityStyle::kSecure, observer.latest_security_style());
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().insecure_explanations.size());
  ASSERT_EQ(3u, observer.latest_explanations().secure_explanations.size());
  CheckSecureCertificateExplanation(
      observer.latest_explanations().secure_explanations[0], browser(),
      https_server_.GetCertificate().get());
  CheckSecureConnectionExplanation(
      observer.latest_explanations().secure_explanations[1], browser());
  CheckSecureSubresourcesExplanation(
      observer.latest_explanations().secure_explanations[2]);
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_TRUE(observer.latest_explanations().summary.empty());

  // After going back to the interstitial, an event for a broken lock
  // icon should fire again.
  ui_test_utils::NavigateToURL(browser(), expired_url);
  WaitForInterstitial(web_contents);
  CheckBrokenSecurityStyle(observer, net::ERR_CERT_DATE_INVALID, browser(),
                           https_test_server_expired.GetCertificate().get());
  ASSERT_EQ(2u, observer.latest_explanations().secure_explanations.size());
  CheckSecureConnectionExplanation(
      observer.latest_explanations().secure_explanations[0], browser());
  CheckSecureSubresourcesExplanation(
      observer.latest_explanations().secure_explanations[1]);
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_TRUE(observer.latest_explanations().summary.empty());

  // Since the next expected style is the same as the previous, clear
  // the observer (to make sure that the event fires twice and we don't
  // just see the previous event's style).
  observer.ClearLatestSecurityStyleAndExplanations();

  // Other conditions cannot be tested on this host after clicking
  // through because once the interstitial is clicked through, all URLs
  // for this host will remain in a broken state.
  ProceedThroughInterstitial(web_contents);
  CheckBrokenSecurityStyle(observer, net::ERR_CERT_DATE_INVALID, browser(),
                           https_test_server_expired.GetCertificate().get());
  ASSERT_EQ(2u, observer.latest_explanations().secure_explanations.size());
  CheckSecureConnectionExplanation(
      observer.latest_explanations().secure_explanations[0], browser());
  CheckSecureSubresourcesExplanation(
      observer.latest_explanations().secure_explanations[1]);
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_TRUE(observer.latest_explanations().summary.empty());
}

// Visit a valid HTTPS page, then a broken HTTPS page, and then go back,
// and test that the observed security style matches.
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
  ui_test_utils::NavigateToURL(browser(), valid_https_url);
  EXPECT_EQ(blink::SecurityStyle::kSecure, observer.latest_security_style());
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().insecure_explanations.size());
  ASSERT_EQ(3u, observer.latest_explanations().secure_explanations.size());
  CheckSecureCertificateExplanation(
      observer.latest_explanations().secure_explanations[0], browser(),
      https_server_.GetCertificate().get());
  CheckSecureConnectionExplanation(
      observer.latest_explanations().secure_explanations[1], browser());
  CheckSecureSubresourcesExplanation(
      observer.latest_explanations().secure_explanations[2]);
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());

  // Navigate to a bad HTTPS page on a different host, and then click
  // Back to verify that the previous good security style is seen again.
  GURL expired_https_url(https_test_server_expired.GetURL("/title1.html"));
  GURL::Replacements replace_host;
  replace_host.SetHostStr("www.example_broken.test");
  GURL https_url_different_host =
      expired_https_url.ReplaceComponents(replace_host);

  ui_test_utils::NavigateToURL(browser(), https_url_different_host);

  WaitForInterstitial(web_contents);
  CheckBrokenSecurityStyle(observer, net::ERR_CERT_COMMON_NAME_INVALID,
                           browser(),
                           https_test_server_expired.GetCertificate().get());
  ProceedThroughInterstitial(web_contents);
  CheckBrokenSecurityStyle(observer, net::ERR_CERT_COMMON_NAME_INVALID,
                           browser(),
                           https_test_server_expired.GetCertificate().get());
  ASSERT_EQ(2u, observer.latest_explanations().secure_explanations.size());
  CheckSecureConnectionExplanation(
      observer.latest_explanations().secure_explanations[0], browser());
  CheckSecureSubresourcesExplanation(
      observer.latest_explanations().secure_explanations[1]);
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());

  content::WindowedNotificationObserver back_nav_load_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &web_contents->GetController()));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  back_nav_load_observer.Wait();

  EXPECT_EQ(blink::SecurityStyle::kSecure, observer.latest_security_style());
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().insecure_explanations.size());
  ASSERT_EQ(3u, observer.latest_explanations().secure_explanations.size());
  CheckSecureCertificateExplanation(
      observer.latest_explanations().secure_explanations[0], browser(),
      https_server_.GetCertificate().get());
  CheckSecureConnectionExplanation(
      observer.latest_explanations().secure_explanations[1], browser());
  CheckSecureSubresourcesExplanation(
      observer.latest_explanations().secure_explanations[2]);
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
}

// Tests that legacy TLS pages show the correct security descriptions.
class BrowserTestNonsecureURLRequestWithLegacyTLSWarnings
    : public InProcessBrowserTest {
 public:
  BrowserTestNonsecureURLRequestWithLegacyTLSWarnings() {}

  void SetUpOnMainThread() override {
    net::SSLServerConfig config;
    config.version_max = net::SSL_PROTOCOL_VERSION_TLS1_1;
    config.version_min = net::SSL_PROTOCOL_VERSION_TLS1_1;
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_OK, config);
    ASSERT_TRUE(https_server()->Start());
  }

 protected:
  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  net::EmbeddedTestServer https_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};

  DISALLOW_COPY_AND_ASSIGN(BrowserTestNonsecureURLRequestWithLegacyTLSWarnings);
};

// Tests that a connection with obsolete TLS settings does not get a
// secure connection explanation.
IN_PROC_BROWSER_TEST_F(
    BrowserTestNonsecureURLRequestWithLegacyTLSWarnings,
    DidChangeVisibleSecurityStateObserverObsoleteTLSSettings) {
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(tab);

  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("/ssl/google.html"));
  WaitForInterstitial(tab);
  ProceedThroughInterstitial(tab);

  EXPECT_EQ(blink::SecurityStyle::kInsecureBroken,
            observer.latest_security_style());

  for (const auto& explanation :
       observer.latest_explanations().secure_explanations) {
    EXPECT_NE(l10n_util::GetStringUTF8(IDS_SECURE_SSL_SUMMARY),
              explanation.summary);
  }

  // The description string should include the connection properties.
  const content::SecurityStyleExplanation& explanation =
      observer.latest_explanations().info_explanations[0];
  EXPECT_NE(std::string::npos, explanation.description.find("TLS 1.1"));
  EXPECT_NE(std::string::npos, explanation.description.find("ECDHE_RSA"));
  EXPECT_NE(std::string::npos, explanation.description.find("AES_128_CBC"));
  EXPECT_NE(std::string::npos, explanation.description.find("HMAC-SHA1"));

  // There should be recommendations to fix the issues.
  ASSERT_EQ(2u, explanation.recommendations.size());
  EXPECT_NE(std::string::npos, explanation.recommendations[0].find("TLS 1.2"));
  EXPECT_NE(std::string::npos, explanation.recommendations[1].find("GCM"));
}

// Tests that a connection with legacy TLS versions (TLS 1.0/1.1) gets
// downgraded to SecurityLevel WARNING and |connection_used_legacy_tls| is set.
IN_PROC_BROWSER_TEST_F(BrowserTestNonsecureURLRequestWithLegacyTLSWarnings,
                       LegacyTLSDowngradesSecurityLevel) {
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* helper = SecurityStateTabHelper::FromWebContents(tab);

  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("/ssl/google.html"));
  WaitForInterstitial(tab);
  ProceedThroughInterstitial(tab);

  EXPECT_EQ(security_state::DANGEROUS, helper->GetSecurityLevel());
}

// Tests that the Not Secure chip does not show for error pages on http:// URLs.
// Regression test for https://crbug.com/760647.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperIncognitoTest, HttpErrorPage) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to a URL that results in an error page. Even though the displayed
  // URL is http://, there shouldn't be a Not Secure warning because the browser
  // hasn't really navigated to an http:// page.
  ui_test_utils::NavigateToURL(browser(), GURL("http://nonexistent.test:17"));
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

// Tests that the histogram for security level is recorded correctly for HTTPS
// pages.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTestWithAutoupgradesDisabled,
                       HTTPSSecurityLevelHistogram) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  const char kHistogramName[] = "Security.SecurityLevel.CryptographicScheme";
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  {
    base::HistogramTester histograms;
    ui_test_utils::NavigateToURL(browser(),
                                 https_server_.GetURL("/title1.html"));
    histograms.ExpectUniqueSample(kHistogramName, security_state::SECURE, 1);
  }

  // Load mixed content and check that the histogram is recorded correctly.
  {
    base::HistogramTester histograms;
    SecurityStyleTestObserver observer(contents);
    EXPECT_TRUE(content::ExecuteScript(contents,
                                       "var i = document.createElement('img');"
                                       "i.src = 'http://example.test';"
                                       "document.body.appendChild(i);"));
    observer.WaitForDidChangeVisibleSecurityState();
    histograms.ExpectUniqueSample(kHistogramName, security_state::WARNING, 1);
  }

  // Navigate away and the histogram should be recorded exactly once again, when
  // the new navigation commits.
  {
    base::HistogramTester histograms;
    ui_test_utils::NavigateToURL(browser(),
                                 https_server_.GetURL("/title2.html"));
    histograms.ExpectUniqueSample(kHistogramName, security_state::SECURE, 1);
  }
}

// Tests that the Certificate Transparency compliance of the main resource is
// recorded in a histogram.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, CTComplianceHistogram) {
  const char kHistogramName[] =
      "Security.CertificateTransparency.MainFrameNavigationCompliance";
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  histograms.ExpectUniqueSample(
      kHistogramName, net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
      1);
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
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  content::TestNavigationObserver navigation_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('submit').click();"));
  navigation_observer.Wait();
  // Check that the histogram count logs the security level of the page
  // containing the form, not of the form target page.
  histograms.ExpectUniqueSample(kHistogramName, security_state::SECURE, 1);
}

// Tests that the Safety Tip form submission histogram is logged correctly.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, SafetyTipFormHistogram) {
  const char kHistogramName[] = "Security.SafetyTips.FormSubmission";
  net::EmbeddedTestServer server;
  server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(server.Start());

  // Create a server for the form to target.
  net::EmbeddedTestServer form_server;
  form_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(form_server.Start());

  for (bool flag_page : {false, true}) {
    base::HistogramTester histograms;
    if (flag_page) {
      // Set up a bad reputation Safety Tip on the page containing the form.
      reputation::SetSafetyTipBadRepPatterns({server.GetURL("/").host() + "/"});
    }

    // Use a different host for targeting the form so that a Safety Tip doesn't
    // trigger on the form submission navigation.
    net::HostPortPair host_port_pair = net::HostPortPair::FromURL(
        form_server.GetURL("example.test", "/ssl/google.html"));
    std::string replacement_path = GetFilePathWithHostAndPortReplacement(
        "/ssl/page_with_form_targeting_http_url.html", host_port_pair);
    ui_test_utils::NavigateToURL(browser(), server.GetURL(replacement_path));

    ReputationWebContentsObserver* rep_observer =
        ReputationWebContentsObserver::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents());
    ASSERT_TRUE(rep_observer);
    base::RunLoop run_loop;
    rep_observer->RegisterReputationCheckCallbackForTesting(
        run_loop.QuitClosure());

    ASSERT_TRUE(content::ExecuteScript(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "document.getElementById('submit').click();"));
    // Wait for the reputation check to finish.
    run_loop.Run();

    const security_state::SafetyTipInfo safety_tip_info =
        SecurityStateTabHelper::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents())
            ->GetVisibleSecurityState()
            ->safety_tip_info;
    ASSERT_EQ(security_state::SafetyTipStatus::kNone, safety_tip_info.status);
    ASSERT_EQ(GURL(), safety_tip_info.safe_url);

    // Check that the histogram count logs the safety tip status of the page
    // containing the form, not of the form target page.
    histograms.ExpectUniqueSample(
        kHistogramName,
        flag_page ? security_state::SafetyTipStatus::kBadReputation
                  : security_state::SafetyTipStatus::kNone,
        1);
  }
}

class SecurityStateTabHelperTestWithMixedFormsWarnings
    : public SecurityStateTabHelperTest {
 public:
  SecurityStateTabHelperTestWithMixedFormsWarnings() {
    std::vector<base::Feature> enabled_features = {
        security_interstitials::kInsecureFormSubmissionInterstitial,
        autofill::features::kAutofillPreventMixedFormsFilling};
    feature_list.InitWithFeatures(enabled_features, {});
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTestWithMixedFormsWarnings,
                       MixedFormsShowLockIfWarningsAreEnabled) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/page_displays_insecure_form.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, false, false,
      false /* expect cert status error */);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTestWithMixedFormsWarnings,
                       MixedFormsDontShowLockIfWarningsAreDisabledByPolicy) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kMixedFormsWarningsEnabled, false);

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/page_displays_insecure_form.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, false, false, false,
      false /* expect cert status error */);
}

class SecurityStateTabHelperTestWithMixedFormsWarningsDisabled
    : public SecurityStateTabHelperTest {
 public:
  SecurityStateTabHelperTestWithMixedFormsWarningsDisabled() {
    std::vector<base::Feature> disabled_features = {
        security_interstitials::kInsecureFormSubmissionInterstitial,
        autofill::features::kAutofillPreventMixedFormsFilling};
    feature_list.InitWithFeatures({}, disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTestWithMixedFormsWarningsDisabled,
                       MixedFormsDontShowLockIfWarningsAreDisabled) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/page_displays_insecure_form.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, false, false, false,
      false /* expect cert status error */);
}

class SignedExchangeSecurityStateTest
    : public CertVerifierBrowserTest,
      public testing::WithParamInterface<
          bool /* sxg_prefetch_cache_for_navigations_enabled */> {
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
    const bool sxg_prefetch_cache_for_navigations_enabled = GetParam();
    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;
    if (sxg_prefetch_cache_for_navigations_enabled) {
      enabled_features.push_back(
          features::kSignedExchangePrefetchCacheForNavigations);
    } else {
      disabled_features.push_back(
          features::kSignedExchangePrefetchCacheForNavigations);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);

    sxg_test_helper_.SetUp();

    CertVerifierBrowserTest::SetUp();
  }

  void TearDownOnMainThread() override {
    sxg_test_helper_.TearDownOnMainThread();
    CertVerifierBrowserTest::TearDownOnMainThread();
  }

  content::SignedExchangeBrowserTestHelper sxg_test_helper_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SignedExchangeSecurityStateTest, SecurityLevelIsSecure) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  const GURL inner_url("https://test.example.org/test/");
  const GURL sxg_url =
      embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");
  std::u16string expected_title = base::ASCIIToUTF16(inner_url.spec());
  content::TitleWatcher title_watcher(contents, expected_title);
  ui_test_utils::NavigateToURL(browser(), sxg_url);
  // The inner content of test.example.org_test.sxg has
  // "<script> document.title = document.location.href; </script>".
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  ASSERT_EQ(inner_url, contents->GetURL());

  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false /* expect_sha1_in_chain */,
      false /* expect_displayed_mixed_content */,
      false /* expect_ran_mixed_content */, false /* expect_cert_error */);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeSecurityStateTest,
                       SecurityLevelIsSecureAfterPrefetch) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  const GURL inner_url("https://test.example.org/test/");
  const GURL sxg_url =
      embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");

  // prefetch.html prefetches the signed exchange. And the signed exchange will
  // be served from HTTPCache (when SignedExchangePrefetchCacheForNavigations is
  // not enabled), or from PrefetchedSignedExchangeCache (when
  // SignedExchangePrefetchCacheForNavigations is enabled).
  const GURL prefetch_html_url = embedded_test_server()->GetURL(
      std::string("/sxg/prefetch.html#") + sxg_url.spec());
  {
    std::u16string expected_title = u"OK";
    content::TitleWatcher title_watcher(contents, expected_title);
    ui_test_utils::NavigateToURL(browser(), prefetch_html_url);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  {
    std::u16string expected_title = base::ASCIIToUTF16(inner_url.spec());
    content::TitleWatcher title_watcher(contents, expected_title);
    // Execute the JavaScript code to trigger the followup navigation from the
    // current page.
    EXPECT_TRUE(content::ExecuteScript(
        contents,
        base::StringPrintf("location.href = '%s';", sxg_url.spec().c_str())));
    // The inner content of test.example.org_test.sxg has
    // "<script> document.title = document.location.href; </script>".
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
    ASSERT_EQ(inner_url, contents->GetURL());
  }

  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false /* expect_sha1_in_chain */,
      false /* expect_displayed_mixed_content */,
      false /* expect_ran_mixed_content */, false /* expect_cert_error */);
}

INSTANTIATE_TEST_SUITE_P(, SignedExchangeSecurityStateTest, testing::Bool());

}  // namespace

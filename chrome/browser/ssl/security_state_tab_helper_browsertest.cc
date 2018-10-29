// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/security_state_tab_helper.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
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
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/features.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_state/content/ssl_status_input_event_data.h"
#include "components/security_state/core/features.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
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
#include "content/public/common/page_type.h"
#include "content/public/common/referrer.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/net_errors.h"
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
#include "net/test/url_request/url_request_failed_job.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/features.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

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

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

const char kTestCertificateIssuerName[] = "Test Root CA";

bool AreCommittedInterstitialsEnabled() {
  return base::FeatureList::IsEnabled(features::kSSLCommittedInterstitials);
}

bool IsShowingInterstitial(content::WebContents* tab) {
  if (AreCommittedInterstitialsEnabled()) {
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            tab);
    if (!helper) {
      return false;
    }
    return helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting() !=
           nullptr;
  }
  return tab->GetInterstitialPage() != nullptr;
}

// Waits until an interstitial is showing.
void WaitForInterstitial(content::WebContents* tab) {
  if (!AreCommittedInterstitialsEnabled()) {
    content::WaitForInterstitialAttach(tab);
    ASSERT_TRUE(IsShowingInterstitial(tab));
    ASSERT_TRUE(
        WaitForRenderFrameReady(tab->GetInterstitialPage()->GetMainFrame()));
  } else {
    ASSERT_TRUE(IsShowingInterstitial(tab));
    ASSERT_TRUE(WaitForRenderFrameReady(tab->GetMainFrame()));
  }
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

// Gets the Insecure Input Events from the entry's SSLStatus user data.
security_state::InsecureInputEventData GetInputEvents(
    content::NavigationEntry* entry) {
  security_state::SSLStatusInputEventData* input_events =
      static_cast<security_state::SSLStatusInputEventData*>(
          entry->GetSSL().user_data.get());
  if (input_events)
    return *input_events->input_events();

  return security_state::InsecureInputEventData();
}

// Stores the Insecure Input Events to the entry's SSLStatus user data.
void SetInputEvents(content::NavigationEntry* entry,
                    security_state::InsecureInputEventData events) {
  security_state::SSLStatus& ssl = entry->GetSSL();
  security_state::SSLStatusInputEventData* input_events =
      static_cast<security_state::SSLStatusInputEventData*>(
          ssl.user_data.get());
  if (!input_events) {
    ssl.user_data =
        std::make_unique<security_state::SSLStatusInputEventData>(events);
  } else {
    *input_events->input_events() = events;
  }
}

void SimulateCreditCardFieldEdit(content::WebContents* contents) {
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  security_state::InsecureInputEventData input_events = GetInputEvents(entry);
  input_events.credit_card_field_edited = true;
  SetInputEvents(entry, input_events);
  contents->DidChangeVisibleSecurityState();
}

void SimulatePasswordFieldShown(content::WebContents* contents) {
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  security_state::InsecureInputEventData input_events = GetInputEvents(entry);
  input_events.password_field_shown = true;
  SetInputEvents(entry, input_events);
  contents->DidChangeVisibleSecurityState();
}

// A delegate class that allows emulating selection of a file for an
// INPUT TYPE=FILE form field.
class FileChooserDelegate : public content::WebContentsDelegate {
 public:
  // Constructs a WebContentsDelegate that mocks a file dialog.
  // The mocked file dialog will always reply that the user selected |file|.
  explicit FileChooserDelegate(const base::FilePath& file)
      : file_(file), file_chosen_(false) {}

  // Whether the file dialog was shown.
  bool file_chosen() const { return file_chosen_; }

  // Copy of the params passed to RunFileChooser.
  const blink::mojom::FileChooserParams& params() const { return *params_; }

  // WebContentsDelegate:
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      std::unique_ptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override {
    // Send the selected file to the renderer process.
    std::vector<blink::mojom::FileChooserFileInfoPtr> files;
    files.push_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
        blink::mojom::NativeFileInfo::New(file_, base::string16())));
    listener->FileSelected(std::move(files),
                           blink::mojom::FileChooserParams::Mode::kOpen);

    file_chosen_ = true;
    params_ = params.Clone();
  }

 private:
  base::FilePath file_;
  bool file_chosen_;
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
        latest_security_style_(blink::kWebSecurityStyleUnknown) {}
  ~SecurityStyleTestObserver() override {}

  void DidChangeVisibleSecurityState() override {
    content::SecurityStyleExplanations explanations;
    latest_security_style_ = web_contents()->GetDelegate()->GetSecurityStyle(
        web_contents(), &explanations);
    latest_explanations_ = explanations;
    run_loop_.Quit();
  }

  void WaitForDidChangeVisibleSecurityState() { run_loop_.Run(); }

  blink::WebSecurityStyle latest_security_style() const {
    return latest_security_style_;
  }

  const content::SecurityStyleExplanations& latest_explanations() const {
    return latest_explanations_;
  }

  void ClearLatestSecurityStyleAndExplanations() {
    latest_security_style_ = blink::kWebSecurityStyleUnknown;
    latest_explanations_ = content::SecurityStyleExplanations();
  }

 private:
  blink::WebSecurityStyle latest_security_style_;
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
  EXPECT_EQ(blink::kWebSecurityStyleInsecure, observer.latest_security_style());

  const content::SecurityStyleExplanations& expired_explanation =
      observer.latest_explanations();
  EXPECT_EQ(0u, expired_explanation.neutral_explanations.size());
  ASSERT_EQ(1u, expired_explanation.insecure_explanations.size());
  EXPECT_FALSE(expired_explanation.pkp_bypassed);
  EXPECT_TRUE(expired_explanation.info_explanations.empty());

  // Check that the summary and description are as expected.
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_CERTIFICATE_CHAIN_ERROR),
            expired_explanation.insecure_explanations[0].summary);

  base::string16 error_string = base::UTF8ToUTF16(net::ErrorToString(error));
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
  EXPECT_TRUE(!!expired_explanation.insecure_explanations[0].certificate);
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
  EXPECT_TRUE(!!explanation.certificate);
}

// Checks that the given |explanation| contains an appropriate
// explanation that the connection is secure.
void CheckSecureConnectionExplanation(
    const content::SecurityStyleExplanation& explanation,
    Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  security_state::SecurityInfo security_info;
  SecurityStateTabHelper::FromWebContents(web_contents)
      ->GetSecurityInfo(&security_info);

  const char *protocol, *key_exchange, *cipher, *mac;
  int ssl_version =
      net::SSLConnectionStatusToVersion(security_info.connection_status);
  net::SSLVersionToString(&protocol, ssl_version);
  bool is_aead, is_tls13;
  uint16_t cipher_suite =
      net::SSLConnectionStatusToCipherSuite(security_info.connection_status);
  net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead,
                               &is_tls13, cipher_suite);
  // Modern configurations are always AEADs and specify groups.
  EXPECT_TRUE(is_aead);
  EXPECT_EQ(nullptr, mac);
  ASSERT_NE(0, security_info.key_exchange_group);
  const char* key_exchange_group =
      SSL_get_curve_name(security_info.key_exchange_group);

  // The description should summarize the settings.
  EXPECT_NE(std::string::npos, explanation.description.find(protocol));
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
    security_state::ContentStatus expect_mixed_content_status,
    bool pkp_bypassed,
    bool expect_cert_error) {
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(expect_security_level, security_info.security_level);
  EXPECT_EQ(expect_sha1_in_chain, security_info.sha1_in_chain);
  EXPECT_EQ(expect_mixed_content_status, security_info.mixed_content_status);
  EXPECT_TRUE(security_info.scheme_is_cryptographic);
  EXPECT_EQ(pkp_bypassed, security_info.pkp_bypassed);
  EXPECT_EQ(expect_cert_error,
            net::IsCertStatusError(security_info.cert_status));
  EXPECT_GT(security_info.security_bits, 0);
  EXPECT_TRUE(!!security_info.certificate);
}

void CheckSecurityInfoForNonSecure(content::WebContents* contents) {
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);
  EXPECT_FALSE(security_info.sha1_in_chain);
  EXPECT_EQ(security_state::CONTENT_STATUS_NONE,
            security_info.mixed_content_status);
  EXPECT_FALSE(security_info.scheme_is_cryptographic);
  EXPECT_FALSE(net::IsCertStatusError(security_info.cert_status));
  EXPECT_EQ(-1, security_info.security_bits);
  EXPECT_FALSE(!!security_info.certificate);
}

void ProceedThroughInterstitial(content::WebContents* tab) {
  if (AreCommittedInterstitialsEnabled()) {
    content::TestNavigationObserver nav_observer(tab, 1);
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            tab);
    helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
        ->CommandReceived(
            base::IntToString(security_interstitials::CMD_PROCEED));
    nav_observer.Wait();
  } else {
    content::InterstitialPage* interstitial_page = tab->GetInterstitialPage();
    ASSERT_TRUE(interstitial_page);
    ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
              interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(&tab->GetController()));
    interstitial_page->Proceed();
    observer.Wait();
  }
}

void GetFilePathWithHostAndPortReplacement(
    const std::string& original_file_path,
    const net::HostPortPair& host_port_pair,
    std::string* replacement_path) {
  base::StringPairs replacement_text;
  replacement_text.push_back(
      make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
  net::test_server::GetFilePathWithReplacements(
      original_file_path, replacement_text, replacement_path);
}

GURL GetURLWithNonLocalHostname(net::EmbeddedTestServer* server,
                                const std::string& path) {
  GURL::Replacements replace_host;
  replace_host.SetHostStr("example1.test");
  return server->GetURL(path).ReplaceComponents(replace_host);
}

class SecurityStateTabHelperTest : public CertVerifierBrowserTest,
                                   public testing::WithParamInterface<bool> {
 public:
  SecurityStateTabHelperTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
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
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kSSLCommittedInterstitials);
    }
  }

 protected:
  void SetUpMockCertVerifierForHttpsServer(net::CertStatus cert_status,
                                           int net_result) {
    scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.is_issued_by_known_root = true;
    verify_result.verified_cert = cert;
    verify_result.cert_status = cert_status;

    mock_cert_verifier()->AddResultForCert(cert, verify_result, net_result);
  }

  // Navigates to an empty page and runs |javascript| to create a URL with with
  // a scheme of |scheme|. Expects a security level of NONE if
  // |use_secure_inner_origin| is true and HTTP_SHOW_WARNING otherwise.
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
    security_state::SecurityInfo security_info;
    helper->GetSecurityInfo(&security_info);

    content::NavigationEntry* entry =
        contents->GetController().GetVisibleEntry();
    ASSERT_TRUE(entry);

    EXPECT_EQ(use_secure_inner_origin ? security_state::NONE
                                      : security_state::HTTP_SHOW_WARNING,
              security_info.security_level);
  }

  net::EmbeddedTestServer https_server_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(SecurityStateTabHelperTest);
};

INSTANTIATE_TEST_CASE_P(,
                        SecurityStateTabHelperTest,
                        ::testing::Values(false, true));

// Same as SecurityStateTabHelperTest, but with Incognito enabled.
class SecurityStateTabHelperIncognitoTest : public SecurityStateTabHelperTest {
 public:
  SecurityStateTabHelperIncognitoTest() : SecurityStateTabHelperTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SecurityStateTabHelperTest::SetUpCommandLine(command_line);
    // Test should run Incognito.
    command_line->AppendSwitch(switches::kIncognito);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityStateTabHelperIncognitoTest);
};

INSTANTIATE_TEST_CASE_P(,
                        SecurityStateTabHelperIncognitoTest,
                        ::testing::Values(false, true));

class DidChangeVisibleSecurityStateTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  DidChangeVisibleSecurityStateTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Browser will both run and display insecure content.
    command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kSSLCommittedInterstitials);
    }
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  net::EmbeddedTestServer https_server_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(DidChangeVisibleSecurityStateTest);
};

INSTANTIATE_TEST_CASE_P(,
                        DidChangeVisibleSecurityStateTest,
                        ::testing::Values(false, true));

IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest, HttpPage) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html"));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);
  EXPECT_FALSE(security_info.sha1_in_chain);
  EXPECT_EQ(security_state::CONTENT_STATUS_NONE,
            security_info.mixed_content_status);
  EXPECT_FALSE(security_info.scheme_is_cryptographic);
  EXPECT_FALSE(net::IsCertStatusError(security_info.cert_status));
  EXPECT_FALSE(!!security_info.certificate);
  EXPECT_EQ(-1, security_info.security_bits);
  EXPECT_EQ(0, security_info.connection_status);
}

IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest, HttpsPage) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, security_state::CONTENT_STATUS_NONE, false,
      false /* expect cert status error */);
}

// Tests that interstitial.ssl.visited_site_after_warning is being logged to
// correctly.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest, UMALogsVisitsAfterWarning) {
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

// Tests that interstitial.ssl.visited_site_after_warning is not being logged
// to on errors that do not trigger a full site interstitial.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest, UMADoesNotLogOnMinorError) {
  const char kHistogramName[] = "interstitial.ssl.visited_site_after_warning";
  base::HistogramTester histograms;
  SetUpMockCertVerifierForHttpsServer(
      net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION, net::OK);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  histograms.ExpectTotalCount(kHistogramName, 0);
}

// Test security state after clickthrough for a SHA-1 certificate that is
// blocked by default.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest, SHA1CertificateBlocked) {
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
      security_state::DANGEROUS, true, security_state::CONTENT_STATUS_NONE,
      false, true /* expect cert status error */);

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
      security_state::DANGEROUS, true, security_state::CONTENT_STATUS_NONE,
      false, true /* expect cert status error */);

  const content::SecurityStyleExplanations& page_explanation =
      observer.latest_explanations();
  ASSERT_EQ(2u, page_explanation.insecure_explanations.size());
  ASSERT_EQ(0u, page_explanation.neutral_explanations.size());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SHA1),
            page_explanation.insecure_explanations[0].summary);
}

// Test security state for a SHA-1 certificate that is allowed by policy.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest, SHA1CertificateWarning) {
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT,
                                      net::OK);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, true, security_state::CONTENT_STATUS_NONE, false,
      false /* expect cert status error */);

  const content::SecurityStyleExplanations& explanation =
      observer.latest_explanations();

  ASSERT_EQ(0u, explanation.insecure_explanations.size());
  ASSERT_EQ(1u, explanation.neutral_explanations.size());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SHA1),
            explanation.neutral_explanations[0].summary);
}

IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest, MixedContent) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  net::HostPortPair replacement_pair = embedded_test_server()->host_port_pair();
  replacement_pair.set_host("example.test");

  // Navigate to an HTTPS page that displays mixed content.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html", replacement_pair,
      &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, false, security_state::CONTENT_STATUS_DISPLAYED,
      false, false /* expect cert status error */);

  // Navigate to an HTTPS page that displays mixed content dynamically.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html", replacement_pair,
      &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, security_state::CONTENT_STATUS_NONE, false,
      false /* expect cert status error */);
  // Load the insecure image.
  bool js_result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(), "loadBadImage();",
      &js_result));
  EXPECT_TRUE(js_result);
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, false, security_state::CONTENT_STATUS_DISPLAYED,
      false, false /* expect cert status error */);

  // Navigate to an HTTPS page that runs mixed content.
  GetFilePathWithHostAndPortReplacement("/ssl/page_runs_insecure_content.html",
                                        replacement_pair, &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false, security_state::CONTENT_STATUS_RAN,
      false, false /* expect cert status error */);

  // Navigate to an HTTPS page that runs and displays mixed content.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_and_displays_insecure_content.html", replacement_pair,
      &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false,
      security_state::CONTENT_STATUS_DISPLAYED_AND_RAN, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs mixed content in an iframe.
  net::HostPortPair host_port_pair =
      net::HostPortPair::FromURL(https_server_.GetURL("/title1.html"));
  host_port_pair.set_host("different-host.test");
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content_in_iframe.html", host_port_pair,
      &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false, security_state::CONTENT_STATUS_RAN,
      false, false /* expect cert status error */);
}

IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
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
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);

  EXPECT_FALSE(net::IsCertStatusError(security_info.cert_status));
  EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  EXPECT_EQ(security_state::CONTENT_STATUS_RAN,
            security_info.content_with_cert_errors_status);
}

IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
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
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);

  EXPECT_FALSE(net::IsCertStatusError(security_info.cert_status));
  EXPECT_EQ(security_state::NONE, security_info.security_level);
  EXPECT_EQ(security_state::CONTENT_STATUS_DISPLAYED,
            security_info.content_with_cert_errors_status);
}

IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
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
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);

  EXPECT_FALSE(net::IsCertStatusError(security_info.cert_status));
  EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  EXPECT_EQ(security_state::CONTENT_STATUS_DISPLAYED_AND_RAN,
            security_info.content_with_cert_errors_status);
}

// Same as SecurityStateTabHelperTest.ActiveAndPassiveContentWithCertErrors but
// with a SHA1 cert.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest, MixedContentWithSHA1Cert) {
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT,
                                      net::OK);

  net::HostPortPair replacement_pair = embedded_test_server()->host_port_pair();
  replacement_pair.set_host("example.test");

  // Navigate to an HTTPS page that displays mixed content.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html", replacement_pair,
      &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, true, security_state::CONTENT_STATUS_DISPLAYED,
      false, false /* expect cert status error */);

  // Navigate to an HTTPS page that displays mixed content dynamically.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html", replacement_pair,
      &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, true, security_state::CONTENT_STATUS_NONE, false,
      false /* expect cert status error */);
  // Load the insecure image.
  bool js_result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(), "loadBadImage();",
      &js_result));
  EXPECT_TRUE(js_result);
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, true, security_state::CONTENT_STATUS_DISPLAYED,
      false, false /* expect cert status error */);

  // Navigate to an HTTPS page that runs mixed content.
  GetFilePathWithHostAndPortReplacement("/ssl/page_runs_insecure_content.html",
                                        replacement_pair, &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, true, security_state::CONTENT_STATUS_RAN,
      false, false /* expect cert status error */);

  // Navigate to an HTTPS page that runs and displays mixed content.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_and_displays_insecure_content.html", replacement_pair,
      &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, true,
      security_state::CONTENT_STATUS_DISPLAYED_AND_RAN, false,
      false /* expect cert status error */);
}

// Tests that the Content Security Policy block-all-mixed-content
// directive stops mixed content from running.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest, MixedContentStrictBlocking) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Navigate to an HTTPS page that tries to run mixed content in an
  // iframe, with strict mixed content blocking.
  std::string replacement_path;
  net::HostPortPair host_port_pair =
      net::HostPortPair::FromURL(https_server_.GetURL("/title1.html"));
  host_port_pair.set_host("different-host.test");
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content_in_iframe_with_strict_blocking.html",
      host_port_pair, &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, security_state::CONTENT_STATUS_NONE, false,
      false /* expect cert status error */);
}

IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest, BrokenHTTPS) {
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_DATE_INVALID,
                                      net::ERR_CERT_DATE_INVALID);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false, security_state::CONTENT_STATUS_NONE,
      false, true /* expect cert status error */);

  ProceedThroughInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents());

  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false, security_state::CONTENT_STATUS_NONE,
      false, true /* expect cert status error */);

  // Navigate to a broken HTTPS page that displays mixed content.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false,
      security_state::CONTENT_STATUS_DISPLAYED, false,
      true /* expect cert status error */);
}

// Tests that the security level of data: URLs is always downgraded to
// HTTP_SHOW_WARNING.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       SecurityLevelDowngradedOnDataUrl) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStyleTestObserver observer(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,<html></html>"));
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Ensure that WebContentsObservers don't show an incorrect Form Not Secure
  // explanation. Regression test for https://crbug.com/691412.
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(content::SSLStatus::NORMAL_CONTENT, entry->GetSSL().content_status);
}

// Tests the security level and malicious content status for sign-in password
// reuse threat type.
IN_PROC_BROWSER_TEST_P(
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
  // SB_THREAT_TYPE_SIGN_IN_PASSWORD_REUSE.
  safe_browsing::ChromePasswordProtectionService* service =
      safe_browsing::ChromePasswordProtectionService::
          GetPasswordProtectionService(browser()->profile());
  service->ShowModalWarning(contents, "unused-token",
                            safe_browsing::LoginReputationClientRequest::
                                PasswordReuseEvent::SIGN_IN_PASSWORD);
  observer.WaitForDidChangeVisibleSecurityState();

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SIGN_IN_PASSWORD_REUSE,
            security_info.malicious_content_status);

  // Simulates a Gaia password change, then malicious content status will
  // change to MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING.
  service->OnGaiaPasswordChanged();
  base::RunLoop().RunUntilIdle();
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING,
            security_info.malicious_content_status);
}

// Tests the security level and malicious content status for enterprise password
// reuse threat type.
IN_PROC_BROWSER_TEST_P(
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
  service->ShowModalWarning(contents, "unused-token",
                            safe_browsing::LoginReputationClientRequest::
                                PasswordReuseEvent::ENTERPRISE_PASSWORD);
  observer.WaitForDidChangeVisibleSecurityState();

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE,
            security_info.malicious_content_status);

  // Since these are non-Gaia enterprise passwords, Gaia password change won't
  // have any impact here.
}

// Tests that the security level of ftp: URLs is always downgraded to
// HTTP_SHOW_WARNING.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       SecurityLevelDowngradedOnFtpUrl) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStyleTestObserver observer(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(browser(), GURL("ftp://example.test/"));
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Ensure that WebContentsObservers don't show an incorrect Form Not Secure
  // explanation. Regression test for https://crbug.com/691412.
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());

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
    if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      mojo::ScopedAllowSyncCallForTesting allow_sync_call;

      network::mojom::NetworkServiceTestPtr network_service_test;
      content::ServiceManagerConnection::GetForProcess()
          ->GetConnector()
          ->BindInterface(content::mojom::kNetworkServiceName,
                          &network_service_test);
      network_service_test->SetTransportSecurityStateSource(0);
      return;
    }
    RunOnIOThreadBlocking(base::BindOnce(&PKPModelClientTest::CleanUpOnIOThread,
                                         base::Unretained(this)));

    // This test class intentionally does not call the parent
    // TearDownOnMainThread.
  }

 private:
  void EnableStaticPins() {
    if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      mojo::ScopedAllowSyncCallForTesting allow_sync_call;

      network::mojom::NetworkServiceTestPtr network_service_test;
      content::ServiceManagerConnection::GetForProcess()
          ->GetConnector()
          ->BindInterface(content::mojom::kNetworkServiceName,
                          &network_service_test);
      // The tests don't depend on reporting, so the port doesn't matter.
      network_service_test->SetTransportSecurityStateSource(80);

      content::StoragePartition* partition =
          content::BrowserContext::GetDefaultStoragePartition(
              browser()->profile());
      partition->GetNetworkContext()->EnableStaticKeyPinningForTesting();
      return;
    }
    RunOnIOThreadBlocking(base::BindOnce(
        &PKPModelClientTest::EnableStaticPinsOnIOThread, base::Unretained(this),
        base::RetainedRef(browser()->profile()->GetRequestContext())));
  }

  void RunOnIOThreadBlocking(base::OnceClosure task) {
    base::RunLoop run_loop;
    base::PostTaskWithTraitsAndReply(FROM_HERE, {content::BrowserThread::IO},
                                     std::move(task), run_loop.QuitClosure());
    run_loop.Run();
  }

  void EnableStaticPinsOnIOThread(
      scoped_refptr<net::URLRequestContextGetter> context_getter) {
    // The tests don't depend on reporting, so the port doesn't matter.
    transport_security_state_source_ =
        std::make_unique<net::ScopedTransportSecurityStateSource>();

    net::TransportSecurityState* state =
        context_getter->GetURLRequestContext()->transport_security_state();
    state->EnableStaticPinsForTesting();
  }

  void CleanUpOnIOThread() { transport_security_state_source_.reset(); }

  // Only used when NetworkService is disabled. Accessed on IO thread.
  std::unique_ptr<net::ScopedTransportSecurityStateSource>
      transport_security_state_source_;
};

INSTANTIATE_TEST_CASE_P(, PKPModelClientTest, ::testing::Values(false, true));

IN_PROC_BROWSER_TEST_P(PKPModelClientTest, PKPBypass) {
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
      security_state::SECURE, false, security_state::CONTENT_STATUS_NONE, true,
      false);

  const content::SecurityStyleExplanations& explanation =
      observer.latest_explanations();
  EXPECT_TRUE(explanation.pkp_bypassed);
  EXPECT_FALSE(explanation.info_explanations.empty());
}

IN_PROC_BROWSER_TEST_P(PKPModelClientTest, PKPEnforced) {
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

// Fails requests with ERR_IO_PENDING. Can be used to simulate a navigation
// that never stops loading.
class PendingJobInterceptor : public net::URLRequestInterceptor {
 public:
  PendingJobInterceptor() {}
  ~PendingJobInterceptor() override {}

  // URLRequestInterceptor implementation
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new net::URLRequestFailedJob(request, network_delegate,
                                        net::ERR_IO_PENDING);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PendingJobInterceptor);
};

void InstallLoadingInterceptor(const std::string& host) {
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddHostnameInterceptor(
      "http", host,
      std::unique_ptr<net::URLRequestInterceptor>(new PendingJobInterceptor()));
}

class SecurityStateLoadingTest : public SecurityStateTabHelperTest {
 public:
  SecurityStateLoadingTest() : SecurityStateTabHelperTest() {}
  ~SecurityStateLoadingTest() override {}

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&InstallLoadingInterceptor,
                       embedded_test_server()->GetURL("/title1.html").host()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityStateLoadingTest);
};

INSTANTIATE_TEST_CASE_P(,
                        SecurityStateLoadingTest,
                        ::testing::Values(false, true));

// Tests that navigation state changes cause the security state to be
// updated.
IN_PROC_BROWSER_TEST_P(SecurityStateLoadingTest, NavigationStateChanges) {
  ASSERT_TRUE(https_server_.Start());
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Navigate to an HTTPS page.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, security_state::CONTENT_STATUS_NONE, false,
      false /* expect cert status error */);

  // Navigate to a page that doesn't finish loading. Test that the
  // security state is neutral while the page is loading.
  browser()->OpenURL(content::OpenURLParams(
      embedded_test_server()->GetURL("/title1.html"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
  CheckSecurityInfoForNonSecure(
      browser()->tab_strip_model()->GetActiveWebContents());
}

// Tests that when a visible password field is detected on an HTTP page
// load, the security level is downgraded to HTTP_SHOW_WARNING.
//
// Disabled on Mac bots. See crbug.com/812960.
#if defined(OS_MACOSX)
#define MAYBE_PasswordSecurityLevelDowngraded \
  DISABLED_PasswordSecurityLevelDowngraded
#else
#define MAYBE_PasswordSecurityLevelDowngraded PasswordSecurityLevelDowngraded
#endif
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       MAYBE_PasswordSecurityLevelDowngraded) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(
      browser(), GetURLWithNonLocalHostname(embedded_test_server(),
                                            "/password/simple_password.html"));
  InjectScript(contents);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(GetInputEvents(entry).password_field_shown);
}

// Tests the default security level on blob URLs.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       DefaultSecurityLevelOnBlobUrl) {
  TestBlobOrFilesystemURL("blob", kCreateBlobUrlJavascript,
                          false /* use_secure_inner_origin */);
}

// Same as DefaultSecurityLevelOnBlobUrl, but instead of a blob URL,
// this creates a filesystem URL.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       DefaultSecurityLevelOnFilesystemUrl) {
  TestBlobOrFilesystemURL("filesystem", kCreateFilesystemUrlJavascript,
                          false /* use_secure_inner_origin */);
}

// Tests the default security level on blob URLs with a secure inner origin.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       DefaultSecurityLevelOnSecureBlobUrl) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  TestBlobOrFilesystemURL("blob", kCreateBlobUrlJavascript,
                          true /* use_secure_inner_origin */);
}

// Same as DefaultSecurityLevelOnBlobUrl, but instead of a blob URL,
// this creates a filesystem URL with a secure inner origin.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       DefaultSecurityLevelOnSecureFilesystemUrl) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  TestBlobOrFilesystemURL("filesystem", kCreateFilesystemUrlJavascript,
                          true /* use_secure_inner_origin */);
}

// Tests that when an invisible password field is present on an HTTP page load,
// the security level is *not* downgraded to HTTP_SHOW_WARNING.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       PasswordSecurityLevelNotDowngradedForInvisibleInput) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      security_state::features::kMarkHttpAsFeature);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithNonLocalHostname(embedded_test_server(),
                                 "/password/invisible_password.html"));
  InjectScript(contents);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_FALSE(GetInputEvents(entry).password_field_shown);
}

// Tests that when a visible password field is detected inside an iframe on an
// HTTP page load, the security level is downgraded to HTTP_SHOW_WARNING.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       PasswordSecurityLevelDowngradedFromIframe) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithNonLocalHostname(embedded_test_server(),
                                 "/password/simple_password_in_iframe.html"));
  InjectScript(contents);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(GetInputEvents(entry).password_field_shown);
}

// Tests that when a visible password field is detected inside an iframe on an
// HTTP page load, the security level is downgraded to HTTP_SHOW_WARNING, even
// if the iframe itself was loaded over HTTPS.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       PasswordSecurityLevelDowngradedFromHttpsIframe) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP URL, which loads an iframe using the host and port of
  // |https_server_|.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/password/simple_password_in_https_iframe.html",
      https_server_.host_port_pair(), &replacement_path);
  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithNonLocalHostname(embedded_test_server(), replacement_path));
  InjectScript(contents);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(GetInputEvents(entry).password_field_shown);
}

// Tests that when a visible password field is detected on an HTTPS page load,
// the security level is *not* downgraded to HTTP_SHOW_WARNING.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       PasswordSecurityLevelNotDowngradedOnHttps) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  GURL url = GetURLWithNonLocalHostname(&https_server_,
                                        "/password/simple_password.html");
  ui_test_utils::NavigateToURL(browser(), url);
  InjectScript(contents);
  // The security level should not be HTTP_SHOW_WARNING, because the page was
  // HTTPS instead of HTTP.
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::SECURE, security_info.security_level);

  // The SSLStatus flags should only be set if the top-level page load was HTTP,
  // which it was not in this case.
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_FALSE(GetInputEvents(entry).password_field_shown);
}

// Tests that the security level of a HTTP page is not downgraded when a form
// field is modified by JavaScript.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       SecurityLevelNotDowngradedAfterScriptModification) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      security_state::features::kMarkHttpAsFeature,
      {{security_state::features::kMarkHttpAsFeatureParameterName,
        security_state::features::
            kMarkHttpAsParameterWarningAndDangerousOnFormEdits}});

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
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Verify a value set operation isn't treated as user-input.
  EXPECT_TRUE(content::ExecuteScript(
      contents, "document.getElementById('text_id').value='v';"));
  InjectScript(contents);
  base::RunLoop().RunUntilIdle();
  helper->GetSecurityInfo(&security_info);
  ASSERT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  ASSERT_FALSE(security_info.field_edit_downgraded_security_level);

  // Verify an InsertText operation isn't treated as user-input.
  EXPECT_TRUE(content::ExecuteScript(
      contents, "document.execCommand('InsertText',false,'a');"));
  InjectScript(contents);
  base::RunLoop().RunUntilIdle();
  helper->GetSecurityInfo(&security_info);
  ASSERT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  ASSERT_FALSE(security_info.field_edit_downgraded_security_level);
}

// Tests that the security level of a HTTP page is downgraded to
// HTTP_SHOW_WARNING after editing a form field in the relevant configurations.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       SecurityLevelDowngradedAfterFileSelection) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      security_state::features::kMarkHttpAsFeature);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithNonLocalHostname(embedded_test_server(), "/file_input.html"));
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);

  // Prepare a file for the upload form.
  base::FilePath file_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_TEMP, &file_path));
  file_path = file_path.AppendASCII("bar");

  // Fill out the form to refer to the test file.
  SecurityStyleTestObserver observer(contents);
  std::unique_ptr<FileChooserDelegate> delegate(
      new FileChooserDelegate(file_path));
  contents->SetDelegate(delegate.get());
  EXPECT_TRUE(
      ExecuteScript(contents, "document.getElementById('fileinput').click();"));
  EXPECT_TRUE(delegate->file_chosen());
  observer.WaitForDidChangeVisibleSecurityState();

  // Verify that the security state degrades as expected.
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  EXPECT_TRUE(security_info.field_edit_downgraded_security_level);

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(GetInputEvents(entry).insecure_field_edited);

  // Verify that after a refresh, the HTTP_SHOW_WARNING state is cleared.
  contents->GetController().Reload(content::ReloadType::NORMAL, false);
  content::WaitForLoadStop(contents);
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);
  EXPECT_FALSE(security_info.field_edit_downgraded_security_level);
}

// A Browser subclass that keeps track of messages that have been
// added to the console. Messages can be retrieved or cleared with
// console_messages() and ClearConsoleMessages(). The user of this class
// can set a callback to run when the next console message notification
// arrives.
class ConsoleWebContentsDelegate : public Browser {
 public:
  explicit ConsoleWebContentsDelegate(const Browser::CreateParams& params)
      : Browser(params) {}
  ~ConsoleWebContentsDelegate() override {}

  const std::vector<base::string16>& console_messages() const {
    return console_messages_;
  }

  void set_console_message_callback(const base::Closure& callback) {
    console_message_callback_ = callback;
  }

  void ClearConsoleMessages() { console_messages_.clear(); }

  // content::WebContentsDelegate
  bool DidAddMessageToConsole(content::WebContents* source,
                              int32_t level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override {
    console_messages_.push_back(message);
    if (!console_message_callback_.is_null()) {
      console_message_callback_.Run();
      console_message_callback_.Reset();
    }
    return true;
  }

 private:
  std::vector<base::string16> console_messages_;
  base::Closure console_message_callback_;

  DISALLOW_COPY_AND_ASSIGN(ConsoleWebContentsDelegate);
};

// Checks that |delegate| has observed exactly one console message for
// HTTP_SHOW_WARNING. To avoid brittleness, this just looks for keywords
// in the string rather than the exact text.
void CheckForOneHttpWarningConsoleMessage(
    ConsoleWebContentsDelegate* delegate) {
  const std::vector<base::string16>& messages = delegate->console_messages();
  ASSERT_EQ(1u, messages.size());
  EXPECT_NE(base::string16::npos,
            messages[0].find(base::ASCIIToUTF16("warning has been added")));
}

// Tests that console messages are printed upon a call to
// GetSecurityInfo() on an HTTP_SHOW_WARNING page, exactly once per
// main-frame navigation.

// TODO(estark): add console messages for the |kMarkHttpAsParameterWarning|
// configuration of |kMarkHttpAsFeature| and update this test accordingly.
// https://crbug.com/802921
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest, ConsoleMessage) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      security_state::features::kMarkHttpAsFeature);
  ConsoleWebContentsDelegate* delegate = new ConsoleWebContentsDelegate(
      Browser::CreateParams(browser()->profile(), true));
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          original_contents->GetBrowserContext()));
  content::WebContents* raw_contents = contents.get();
  ASSERT_TRUE(raw_contents);
  raw_contents->SetDelegate(delegate);
  delegate->tab_strip_model()->AppendWebContents(std::move(contents), true);
  int index = delegate->tab_strip_model()->GetIndexOfWebContents(raw_contents);
  delegate->tab_strip_model()->ActivateTabAt(index, true);
  ASSERT_EQ(raw_contents, delegate->tab_strip_model()->GetActiveWebContents());

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  GURL http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html");
  ui_test_utils::NavigateToURL(delegate, http_url);
  content::NavigationEntry* entry =
      raw_contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(http_url, entry->GetURL());
  EXPECT_TRUE(delegate->console_messages().empty());

  // Trigger the HTTP_SHOW_WARNING state.
  base::RunLoop first_message;
  delegate->set_console_message_callback(first_message.QuitClosure());
  SimulatePasswordFieldShown(raw_contents);
  first_message.Run();

  // Check that the HTTP_SHOW_WARNING state was actually triggered.
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(raw_contents);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Check that the expected console message is present.
  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));
  delegate->ClearConsoleMessages();

  // Two subsequent triggers of VisibleSecurityStateChanged -- one on the
  // same navigation and one on another navigation -- should only result
  // in one additional console message.
  SimulateCreditCardFieldEdit(raw_contents);
  GURL second_http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title2.html");
  ui_test_utils::NavigateToURL(delegate, second_http_url);
  entry = raw_contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(second_http_url, entry->GetURL());

  base::RunLoop second_message;
  delegate->set_console_message_callback(second_message.QuitClosure());
  SimulatePasswordFieldShown(raw_contents);
  second_message.Run();

  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));
}

// Tests that additional HTTP_SHOW_WARNING console messages are not
// printed after subframe navigations.
//
// TODO(estark): add console messages for the |kMarkHttpAsParameterWarning|
// configuration of |kMarkHttpAsFeature| and update this test accordingly.
// https://crbug.com/802921
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       ConsoleMessageNotPrintedForFrameNavigation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      security_state::features::kMarkHttpAsFeature);

  ConsoleWebContentsDelegate* delegate = new ConsoleWebContentsDelegate(
      Browser::CreateParams(browser()->profile(), true));
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          original_contents->GetBrowserContext()));
  content::WebContents* raw_contents = contents.get();
  ASSERT_TRUE(raw_contents);
  raw_contents->SetDelegate(delegate);
  delegate->tab_strip_model()->AppendWebContents(std::move(contents), true);
  int index = delegate->tab_strip_model()->GetIndexOfWebContents(raw_contents);
  delegate->tab_strip_model()->ActivateTabAt(index, true);
  ASSERT_EQ(raw_contents, delegate->tab_strip_model()->GetActiveWebContents());

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  GURL http_url = GetURLWithNonLocalHostname(embedded_test_server(),
                                             "/ssl/page_with_frame.html");
  ui_test_utils::NavigateToURL(delegate, http_url);
  content::NavigationEntry* entry =
      raw_contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(http_url, entry->GetURL());
  EXPECT_TRUE(delegate->console_messages().empty());

  // Trigger the HTTP_SHOW_WARNING state.
  base::RunLoop first_message;
  delegate->set_console_message_callback(first_message.QuitClosure());
  SimulatePasswordFieldShown(raw_contents);
  first_message.Run();

  // Check that the HTTP_SHOW_WARNING state was actually triggered.
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(raw_contents);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Check that the expected console message is present.
  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));
  delegate->ClearConsoleMessages();

  // Navigate the subframe and trigger VisibleSecurityStateChanged
  // again. While the security level is still HTTP_SHOW_WARNING, an
  // additional console message should not be logged because there was
  // already a console message logged for the current main-frame
  // navigation.
  content::WindowedNotificationObserver subframe_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &raw_contents->GetController()));
  EXPECT_TRUE(content::ExecuteScript(
      raw_contents,
      "document.getElementById('navFrame').src = '/title2.html';"));
  subframe_observer.Wait();

  SimulateCreditCardFieldEdit(raw_contents);
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Do a main frame navigation and then trigger HTTP_SHOW_WARNING
  // again. From the above subframe navigation and this main-frame
  // navigation, exactly one console message is expected.
  GURL second_http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title2.html");
  ui_test_utils::NavigateToURL(delegate, second_http_url);
  entry = raw_contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(second_http_url, entry->GetURL());

  base::RunLoop second_message;
  delegate->set_console_message_callback(second_message.QuitClosure());
  SimulatePasswordFieldShown(raw_contents);
  second_message.Run();

  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));
}

// Tests that additional HTTP_SHOW_WARNING console messages are not
// printed after pushState navigations.
//
// TODO(estark): add console messages for the |kMarkHttpAsParameterWarning|
// configuration of |kMarkHttpAsFeature| and update this test accordingly.
// https://crbug.com/802921
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       ConsoleMessageNotPrintedForPushStateNavigation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      security_state::features::kMarkHttpAsFeature);

  ConsoleWebContentsDelegate* delegate = new ConsoleWebContentsDelegate(
      Browser::CreateParams(browser()->profile(), true));
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          original_contents->GetBrowserContext()));
  content::WebContents* raw_contents = contents.get();
  ASSERT_TRUE(raw_contents);
  raw_contents->SetDelegate(delegate);
  delegate->tab_strip_model()->AppendWebContents(std::move(contents), true);
  int index = delegate->tab_strip_model()->GetIndexOfWebContents(raw_contents);
  delegate->tab_strip_model()->ActivateTabAt(index, true);
  ASSERT_EQ(raw_contents, delegate->tab_strip_model()->GetActiveWebContents());

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  GURL http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html");
  ui_test_utils::NavigateToURL(delegate, http_url);
  content::NavigationEntry* entry =
      raw_contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(http_url, entry->GetURL());
  EXPECT_TRUE(delegate->console_messages().empty());

  // Trigger the HTTP_SHOW_WARNING state.
  base::RunLoop first_message;
  delegate->set_console_message_callback(first_message.QuitClosure());
  SimulatePasswordFieldShown(raw_contents);
  first_message.Run();

  // Check that the HTTP_SHOW_WARNING state was actually triggered.
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(raw_contents);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Check that the expected console message is present.
  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));
  delegate->ClearConsoleMessages();

  // Navigate with pushState and trigger VisibleSecurityStateChanged
  // again. While the security level is still HTTP_SHOW_WARNING, an
  // additional console message should not be logged because there was
  // already a console message logged for the current main-frame
  // navigation.
  EXPECT_TRUE(content::ExecuteScript(
      raw_contents, "history.pushState({ foo: 'bar' }, 'foo', 'bar');"));
  SimulateCreditCardFieldEdit(raw_contents);
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Do a main frame navigation and then trigger HTTP_SHOW_WARNING
  // again. From the above pushState navigation and this main-frame
  // navigation, exactly one console message is expected.
  GURL second_http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title2.html");
  ui_test_utils::NavigateToURL(delegate, second_http_url);
  entry = raw_contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(second_http_url, entry->GetURL());

  base::RunLoop second_message;
  delegate->set_console_message_callback(second_message.QuitClosure());
  SimulatePasswordFieldShown(raw_contents);
  second_message.Run();

  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));
}

// Tests that the security state for a WebContents is up to date when the
// WebContents is inserted into a Browser's TabStripModel.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest, AddedTab) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(tab->GetBrowserContext()));
  content::NavigationController& controller = new_contents->GetController();
  SecurityStateTabHelper::CreateForWebContents(new_contents.get());
  CheckSecurityInfoForNonSecure(new_contents.get());
  controller.LoadURL(https_server_.GetURL("/title1.html"), content::Referrer(),
                     ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(content::WaitForLoadStop(new_contents.get()));
  CheckSecurityInfoForSecure(new_contents.get(), security_state::SECURE, false,
                             security_state::CONTENT_STATUS_NONE, false,
                             false /* expect cert status error */);

  content::WebContents* raw_new_contents = new_contents.get();
  browser()->tab_strip_model()->InsertWebContentsAt(0, std::move(new_contents),
                                                    TabStripModel::ADD_NONE);
  CheckSecurityInfoForSecure(raw_new_contents, security_state::SECURE, false,
                             security_state::CONTENT_STATUS_NONE, false,
                             false /* expect cert status error */);
}

// Tests that the WebContentsObserver::DidChangeVisibleSecurityState event fires
// with the current style on HTTP, broken HTTPS, and valid HTTPS pages.
IN_PROC_BROWSER_TEST_P(DidChangeVisibleSecurityStateTest,
                       DidChangeVisibleSecurityStateObserver) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  net::EmbeddedTestServer https_test_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_test_server_expired.ServeFilesFromSourceDirectory(
      base::FilePath(kDocRoot));
  ASSERT_TRUE(https_test_server_expired.Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);

  // Visit an HTTP url.
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), http_url);
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().insecure_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().secure_explanations.size());
  EXPECT_FALSE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_TRUE(observer.latest_explanations().summary.empty());

  // Visit an (otherwise valid) HTTPS page that displays mixed content.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);

  GURL mixed_content_url(https_server_.GetURL(replacement_path));
  ui_test_utils::NavigateToURL(browser(), mixed_content_url);
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());

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
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_TRUE(observer.latest_explanations().summary.empty());
  EXPECT_TRUE(mixed_content_explanation.displayed_mixed_content);
  EXPECT_FALSE(mixed_content_explanation.ran_mixed_content);
  EXPECT_EQ(blink::kWebSecurityStyleNeutral,
            mixed_content_explanation.displayed_insecure_content_style);
  EXPECT_EQ(blink::kWebSecurityStyleInsecure,
            mixed_content_explanation.ran_insecure_content_style);

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
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);
  EXPECT_TRUE(observer.latest_explanations().summary.empty());

  // Before clicking through, navigate to a different page, and then go
  // back to the interstitial.
  GURL valid_https_url(https_server_.GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), valid_https_url);
  EXPECT_EQ(blink::kWebSecurityStyleSecure, observer.latest_security_style());
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
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);
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
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);
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
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);
  EXPECT_TRUE(observer.latest_explanations().summary.empty());
}

// Tests that the security level of a HTTP page in Incognito mode is downgraded
// to HTTP_SHOW_WARNING.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperIncognitoTest,
                       SecurityLevelDowngradedForHTTPInIncognito) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      security_state::features::kMarkHttpAsFeature);

  ConsoleWebContentsDelegate* delegate = new ConsoleWebContentsDelegate(
      Browser::CreateParams(browser()->profile(), true));
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          original_contents->GetBrowserContext()));
  content::WebContents* raw_contents = contents.get();
  ASSERT_TRUE(raw_contents);
  ASSERT_TRUE(raw_contents->GetBrowserContext()->IsOffTheRecord());
  raw_contents->SetDelegate(delegate);
  delegate->tab_strip_model()->AppendWebContents(std::move(contents), true);
  int index = delegate->tab_strip_model()->GetIndexOfWebContents(raw_contents);
  delegate->tab_strip_model()->ActivateTabAt(index, true);
  ASSERT_EQ(raw_contents, delegate->tab_strip_model()->GetActiveWebContents());

  SecurityStyleTestObserver observer(raw_contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(raw_contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  GURL http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html");
  ui_test_utils::NavigateToURL(delegate, http_url);
  content::NavigationEntry* entry =
      raw_contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(http_url, entry->GetURL());

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.incognito_downgraded_security_level);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  EXPECT_EQ(1u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());

  // Check that the expected console message is present.
  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));

  // Ensure that same-page pushstate does not add another notice.
  EXPECT_TRUE(content::ExecuteScript(
      raw_contents, "history.pushState({ foo: 'bar' }, 'foo', 'bar');"));
  EXPECT_EQ(1u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());
  // Check that no additional console message is present.
  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));
}

// Tests that additional HTTP_SHOW_WARNING console messages are not
// printed after aborted navigations.
//
// TODO(estark): add console messages for the |kMarkHttpAsParameterWarning|
// configuration of |kMarkHttpAsFeature| and update this test accordingly.
// https://crbug.com/802921
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperIncognitoTest,
                       ConsoleMessageNotPrintedForAbortedNavigation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      security_state::features::kMarkHttpAsFeature);

  ConsoleWebContentsDelegate* delegate = new ConsoleWebContentsDelegate(
      Browser::CreateParams(browser()->profile(), true));
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          original_contents->GetBrowserContext()));
  content::WebContents* raw_contents = contents.get();
  ASSERT_TRUE(raw_contents);
  ASSERT_TRUE(raw_contents->GetBrowserContext()->IsOffTheRecord());
  raw_contents->SetDelegate(delegate);
  delegate->tab_strip_model()->AppendWebContents(std::move(contents), true);
  int index = delegate->tab_strip_model()->GetIndexOfWebContents(raw_contents);
  delegate->tab_strip_model()->ActivateTabAt(index, true);
  ASSERT_EQ(raw_contents, delegate->tab_strip_model()->GetActiveWebContents());

  SecurityStyleTestObserver observer(raw_contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(raw_contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  GURL http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html");
  ui_test_utils::NavigateToURL(delegate, http_url);

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.incognito_downgraded_security_level);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());
  EXPECT_EQ(1u, observer.latest_explanations().neutral_explanations.size());

  // Check that the expected console message is present.
  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));
  delegate->ClearConsoleMessages();

  // Perform a navigation that does not commit.
  // The embedded test server returns a HTTP/204 only for local URLs, so
  // we cannot use GetURLWithNonLocalHostname() here.
  GURL http204_url = embedded_test_server()->GetURL("/nocontent");
  ui_test_utils::NavigateToURL(delegate, http204_url);

  // No change is expected in the security state.
  EXPECT_TRUE(security_info.incognito_downgraded_security_level);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());
  EXPECT_EQ(1u, observer.latest_explanations().neutral_explanations.size());

  // No additional console logging should occur.
  EXPECT_TRUE(delegate->console_messages().empty());
}

// Tests that the security level of a HTTP page in Guest mode is not downgraded
// to HTTP_SHOW_WARNING.
#if defined(OS_CHROMEOS)
// Guest mode cannot be readily browser-tested on ChromeOS.
#define MAYBE_SecurityLevelNotDowngradedForHTTPInGuestMode \
  DISABLED_SecurityLevelNotDowngradedForHTTPInGuestMode
#else
#define MAYBE_SecurityLevelNotDowngradedForHTTPInGuestMode \
  SecurityLevelNotDowngradedForHTTPInGuestMode
#endif
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       MAYBE_SecurityLevelNotDowngradedForHTTPInGuestMode) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      security_state::features::kMarkHttpAsFeature);

  // Create a new browser in Guest Mode.
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());
  content::WindowedNotificationObserver browser_creation_observer(
      chrome::NOTIFICATION_BROWSER_OPENED,
      content::NotificationService::AllSources());
  profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());
  browser_creation_observer.Wait();
  EXPECT_EQ(2U, BrowserList::GetInstance()->size());
  Profile* guest = g_browser_process->profile_manager()->GetProfileByPath(
      ProfileManager::GetGuestProfilePath());
  Browser* guest_browser = chrome::FindAnyBrowser(guest, true);
  ASSERT_TRUE(guest_browser);

  ConsoleWebContentsDelegate* delegate = new ConsoleWebContentsDelegate(
      Browser::CreateParams(guest_browser->profile(), true));
  content::WebContents* original_contents =
      guest_browser->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          original_contents->GetBrowserContext()));
  content::WebContents* raw_contents = contents.get();
  ASSERT_TRUE(raw_contents);
  ASSERT_TRUE(raw_contents->GetBrowserContext()->IsOffTheRecord());
  raw_contents->SetDelegate(delegate);
  delegate->tab_strip_model()->AppendWebContents(std::move(contents), true);
  int index = delegate->tab_strip_model()->GetIndexOfWebContents(raw_contents);
  delegate->tab_strip_model()->ActivateTabAt(index, true);
  ASSERT_EQ(raw_contents, delegate->tab_strip_model()->GetActiveWebContents());

  SecurityStyleTestObserver observer(raw_contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(raw_contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  GURL http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html");
  ui_test_utils::NavigateToURL(delegate, http_url);

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_FALSE(security_info.incognito_downgraded_security_level);
  EXPECT_EQ(security_state::NONE, security_info.security_level);
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());

  // No console notification should occur.
  EXPECT_TRUE(delegate->console_messages().empty());
}

// Tests that the security level of a HTTP page is downgraded to DANGEROUS when
// MarkHttpAsDangerous is enabled.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperIncognitoTest,
                       SecurityLevelDangerousWhenMarkHttpAsDangerous) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      security_state::features::kMarkHttpAsFeature,
      {{security_state::features::kMarkHttpAsFeatureParameterName,
        security_state::features::kMarkHttpAsParameterDangerous}});

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  ASSERT_TRUE(contents->GetBrowserContext()->IsOffTheRecord());

  SecurityStyleTestObserver observer(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  GURL http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html");
  ui_test_utils::NavigateToURL(browser(), http_url);

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_FALSE(security_info.incognito_downgraded_security_level);
  EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  EXPECT_EQ(blink::kWebSecurityStyleInsecure, observer.latest_security_style());
}

// Visit a valid HTTPS page, then a broken HTTPS page, and then go back,
// and test that the observed security style matches.
#if defined(OS_CHROMEOS)
// Flaky on Chrome OS. See https://crbug.com/638576.
#define MAYBE_DidChangeVisibleSecurityStateObserverGoBack \
  DISABLED_DidChangeVisibleSecurityStateObserverGoBack
#else
#define MAYBE_DidChangeVisibleSecurityStateObserverGoBack \
  DidChangeVisibleSecurityStateObserverGoBack
#endif
IN_PROC_BROWSER_TEST_P(DidChangeVisibleSecurityStateTest,
                       MAYBE_DidChangeVisibleSecurityStateObserverGoBack) {
  ASSERT_TRUE(https_server_.Start());

  net::EmbeddedTestServer https_test_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_test_server_expired.ServeFilesFromSourceDirectory(
      base::FilePath(kDocRoot));
  ASSERT_TRUE(https_test_server_expired.Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);

  // Visit a valid HTTPS url.
  GURL valid_https_url(https_server_.GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), valid_https_url);
  EXPECT_EQ(blink::kWebSecurityStyleSecure, observer.latest_security_style());
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
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);

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
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);

  content::WindowedNotificationObserver back_nav_load_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &web_contents->GetController()));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  back_nav_load_observer.Wait();

  EXPECT_EQ(blink::kWebSecurityStyleSecure, observer.latest_security_style());
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
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);
}

// After AddNonsecureUrlHandler() is called, requests to this hostname
// will use obsolete TLS settings.
const char kMockNonsecureHostname[] = "example-nonsecure.test";
const char kResponseFilePath[] = "chrome/test/data/title1.html";
const int kObsoleteTLSVersion = net::SSL_CONNECTION_VERSION_TLS1_1;
// ECDHE_RSA + AES_128_CBC with HMAC-SHA1
const uint16_t kObsoleteCipherSuite = 0xc013;

class BrowserTestNonsecureURLRequest : public InProcessBrowserTest {
 public:
  BrowserTestNonsecureURLRequest() : InProcessBrowserTest(), cert_(nullptr) {}

  void SetUpInProcessBrowserTestFixture() override {
    cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(cert_);
  }

  void SetUpOnMainThread() override {
    // Create URLLoaderInterceptor to mock a TLS connection with obsolete TLS
    // settings specified in kObsoleteTLSVersion and kObsoleteCipherSuite.
    url_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindLambdaForTesting(
            [&](content::URLLoaderInterceptor::RequestParams* params) {
              // Ignore non-test URLs.
              if (params->url_request.url.host() != kMockNonsecureHostname) {
                return false;
              }

              // Set SSLInfo to reflect an obsolete connection.
              base::Optional<net::SSLInfo> ssl_info;
              if (params->url_request.url.SchemeIsCryptographic()) {
                ssl_info = net::SSLInfo();
                net::SSLConnectionStatusSetVersion(
                    kObsoleteTLSVersion, &ssl_info->connection_status);
                net::SSLConnectionStatusSetCipherSuite(
                    kObsoleteCipherSuite, &ssl_info->connection_status);
                ssl_info->cert = cert_;
              }

              // Write the response.
              content::URLLoaderInterceptor::WriteResponse(
                  kResponseFilePath, params->client.get(), nullptr,
                  std::move(ssl_info));

              return true;
            }));
  }

  void TearDownOnMainThread() override { url_interceptor_.reset(); }

 private:
  scoped_refptr<net::X509Certificate> cert_;
  std::unique_ptr<content::URLLoaderInterceptor> url_interceptor_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTestNonsecureURLRequest);
};

// Tests that a connection with obsolete TLS settings does not get a
// secure connection explanation.
IN_PROC_BROWSER_TEST_F(
    BrowserTestNonsecureURLRequest,
    DidChangeVisibleSecurityStateObserverObsoleteTLSSettings) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);

  ui_test_utils::NavigateToURL(
      browser(), GURL(std::string("https://") + kMockNonsecureHostname));

  // The security style of the page doesn't get downgraded for obsolete
  // TLS settings, so it should remain at WebSecurityStyleSecure.
  EXPECT_EQ(blink::kWebSecurityStyleSecure, observer.latest_security_style());

  security_state::SecurityInfo security_info;
  SecurityStateTabHelper::FromWebContents(web_contents)
      ->GetSecurityInfo(&security_info);
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

// Tests that the Not Secure chip does not show for error pages on http:// URLs.
// Regression test for https://crbug.com/760647.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperIncognitoTest, HttpErrorPage) {
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

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);
}

IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest, MarkHttpAsWarning) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      security_state::features::kMarkHttpAsFeature,
      {{security_state::features::kMarkHttpAsFeatureParameterName,
        security_state::features::kMarkHttpAsParameterWarning}});

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html"));

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
}

IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       MarkHttpAsWarningAndDangerousOnFormEdits) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      security_state::features::kMarkHttpAsFeature,
      {{security_state::features::kMarkHttpAsFeatureParameterName,
        security_state::features::
            kMarkHttpAsParameterWarningAndDangerousOnFormEdits}});

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

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  {
    // Ensure that the security level remains Dangerous in the
    // kMarkHttpAsDangerous configuration.
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        security_state::features::kMarkHttpAsFeature,
        {{security_state::features::kMarkHttpAsFeatureParameterName,
          security_state::features::kMarkHttpAsParameterDangerous}});

    helper->GetSecurityInfo(&security_info);
    EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
    EXPECT_FALSE(security_info.field_edit_downgraded_security_level);
  }

  // Type one character into the focused input control and wait for a security
  // state change.
  SecurityStyleTestObserver observer(contents);
  content::SimulateKeyPress(contents, ui::DomKey::FromCharacter('A'),
                            ui::DomCode::US_A, ui::VKEY_A, false, false, false,
                            false);
  observer.WaitForDidChangeVisibleSecurityState();

  // Verify that the security state degrades as expected.
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);

  // Verify security state stays degraded after same-page navigation.
  ui_test_utils::NavigateToURL(
      browser(), GetURLWithNonLocalHostname(
                     embedded_test_server(),
                     "/textinput/focus_input_on_load.html#fragment"));
  content::WaitForLoadStop(contents);
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);

  // Verify that after a refresh, the DANGEROUS state is cleared.
  contents->GetController().Reload(content::ReloadType::NORMAL, false);
  content::WaitForLoadStop(contents);
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
}

IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       MarkHttpAsWarningAndDangerousOnFileInputEdits) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      security_state::features::kMarkHttpAsFeature,
      {{security_state::features::kMarkHttpAsFeatureParameterName,
        security_state::features::
            kMarkHttpAsParameterWarningAndDangerousOnFormEdits}});

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithNonLocalHostname(embedded_test_server(), "/file_input.html"));

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  SecurityStyleTestObserver observer(contents);

  // Prepare a file for the upload form.
  base::FilePath file_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_TEMP, &file_path));
  file_path = file_path.AppendASCII("bar");

  // Fill out the form to refer to the test file.
  std::unique_ptr<FileChooserDelegate> delegate(
      new FileChooserDelegate(file_path));
  contents->SetDelegate(delegate.get());
  EXPECT_TRUE(
      ExecuteScript(contents, "document.getElementById('fileinput').click();"));
  EXPECT_TRUE(delegate->file_chosen());

  observer.WaitForDidChangeVisibleSecurityState();

  // Verify that the security state degrades as expected.
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
}

IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
                       MarkHttpAsWarningAndDangerousOnPasswordsAndCreditCards) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      security_state::features::kMarkHttpAsFeature,
      {{security_state::features::kMarkHttpAsFeatureParameterName,
        security_state::features::
            kMarkHttpAsParameterWarningAndDangerousOnPasswordsAndCreditCards}});

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html"));

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Insert a password field into the page and wait for a security state change.
  {
    SecurityStyleTestObserver observer(contents);
    EXPECT_TRUE(
        content::ExecuteScript(contents,
                               "var i = document.createElement('input');"
                               "i.type = 'password';"
                               "i.id = 'password-field';"
                               "document.body.appendChild(i);"));
    observer.WaitForDidChangeVisibleSecurityState();

    // Verify that the security state degrades as expected.
    helper->GetSecurityInfo(&security_info);
    EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  }

  // Remove the password field and verify that the security level returns to
  // HTTP_SHOW_WARNING.
  {
    SecurityStyleTestObserver observer(contents);
    EXPECT_TRUE(content::ExecuteScript(contents,
                                       "document.body.removeChild(document."
                                       "getElementById('password-field'));"));
    observer.WaitForDidChangeVisibleSecurityState();
    helper->GetSecurityInfo(&security_info);
    EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  }
}

// Tests that the histogram for security level is recorded correctly for HTTP
// pages.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest, HTTPSecurityLevelHistogram) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      security_state::features::kMarkHttpAsFeature,
      {{security_state::features::kMarkHttpAsFeatureParameterName,
        security_state::features::
            kMarkHttpAsParameterWarningAndDangerousOnPasswordsAndCreditCards}});

  const char kHistogramName[] = "Security.SecurityLevel.NoncryptographicScheme";
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  {
    base::HistogramTester histograms;
    // Use a non-local hostname so that password fields (added below) downgrade
    // the security level.
    ui_test_utils::NavigateToURL(
        browser(),
        GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html"));
    histograms.ExpectUniqueSample(kHistogramName,
                                  security_state::HTTP_SHOW_WARNING, 1);
  }

  // Add a password field and check that the histogram is recorded correctly.
  {
    base::HistogramTester histograms;
    SecurityStyleTestObserver observer(contents);
    EXPECT_TRUE(
        content::ExecuteScript(contents,
                               "var i = document.createElement('input');"
                               "i.type = 'password';"
                               "document.body.appendChild(i);"));
    observer.WaitForDidChangeVisibleSecurityState();
    histograms.ExpectUniqueSample(kHistogramName, security_state::DANGEROUS, 1);
  }
}

// Tests that the histogram for security level is recorded correctly for HTTPS
// pages.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest,
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
    histograms.ExpectUniqueSample(kHistogramName, security_state::NONE, 1);
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
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest, CTComplianceHistogram) {
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

// Tests that the Form submission histogram is logged correctly.
IN_PROC_BROWSER_TEST_P(SecurityStateTabHelperTest, FormSecurityLevelHistogram) {
  const char kHistogramName[] = "Security.SecurityLevel.FormSubmission";
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  base::HistogramTester histograms;
  // Create a server with an expired certificate for the form to target.
  net::EmbeddedTestServer broken_https_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  broken_https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  broken_https_server.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
  ASSERT_TRUE(broken_https_server.Start());

  // Make the form target the expired certificate server.
  std::string replacement_path;
  net::HostPortPair host_port_pair =
      net::HostPortPair::FromURL(broken_https_server.GetURL("/google.html"));
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_form_targeting_insecure_url.html", host_port_pair,
      &replacement_path);
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

}  // namespace

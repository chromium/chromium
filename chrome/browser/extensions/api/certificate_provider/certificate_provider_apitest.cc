// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/hash/sha1.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/certificate_provider/certificate_provider.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/certificate_provider/test_certificate_provider_extension.h"
#include "chrome/browser/extensions/api/certificate_provider/certificate_provider_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/notifications/request_pin_view_chromeos.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "crypto/rsa_private_key.h"
#include "extensions/browser/api/test/test_api_observer.h"
#include "extensions/browser/api/test/test_api_observer_registry.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_status_code.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/cert/cert_db_initializer_factory.h"
#endif

using testing::Return;
using testing::_;

namespace {

bool RsaSignRawData(uint16_t openssl_signature_algorithm,
                    const std::vector<uint8_t>& input,
                    crypto::RSAPrivateKey* key,
                    std::vector<uint8_t>* signature) {
  const EVP_MD* const digest_algorithm =
      SSL_get_signature_algorithm_digest(openssl_signature_algorithm);
  bssl::ScopedEVP_MD_CTX ctx;
  EVP_PKEY_CTX* pkey_ctx = nullptr;
  if (!EVP_DigestSignInit(ctx.get(), &pkey_ctx, digest_algorithm,
                          /*ENGINE* e=*/nullptr, key->key()))
    return false;
  if (SSL_is_signature_algorithm_rsa_pss(openssl_signature_algorithm)) {
    // For RSA-PSS, configure the special padding and set the salt length to be
    // equal to the hash size.
    if (!EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) ||
        !EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, /*salt_len=*/-1)) {
      return false;
    }
  }
  size_t sig_len = 0;
  // Determine the signature length for the buffer.
  if (!EVP_DigestSign(ctx.get(), /*out_sig=*/nullptr, &sig_len, input.data(),
                      input.size()))
    return false;
  signature->resize(sig_len);
  return EVP_DigestSign(ctx.get(), signature->data(), &sig_len, input.data(),
                        input.size()) != 0;
}

bool RsaSignPrehashed(uint16_t openssl_signature_algorithm,
                      const std::vector<uint8_t>& digest,
                      crypto::RSAPrivateKey* key,
                      std::vector<uint8_t>* signature) {
  // RSA-PSS is not supported for prehashed data.
  EXPECT_FALSE(SSL_is_signature_algorithm_rsa_pss(openssl_signature_algorithm));
  RSA* rsa_key = EVP_PKEY_get0_RSA(key->key());
  if (!rsa_key)
    return false;
  const int digest_algorithm_nid = EVP_MD_type(
      SSL_get_signature_algorithm_digest(openssl_signature_algorithm));
  unsigned len = 0;
  signature->resize(RSA_size(rsa_key));
  if (!RSA_sign(digest_algorithm_nid, digest.data(), digest.size(),
                signature->data(), &len, rsa_key)) {
    signature->clear();
    return false;
  }
  signature->resize(len);
  return true;
}

// Create a string that if evaluated in JavaScript returns a Uint8Array with
// |bytes| as content.
std::string JsUint8Array(const std::vector<uint8_t>& bytes) {
  std::string res = "new Uint8Array([";
  for (const uint8_t byte : bytes) {
    res += base::NumberToString(byte);
    res += ", ";
  }
  res += "])";
  return res;
}

std::string GetPageTextContent(content::WebContents* web_contents) {
  return content::EvalJs(web_contents->GetPrimaryMainFrame(),
                         "document.body.textContent;")
      .ExtractString();
  ;
}

std::string GetCertFingerprint1(const net::X509Certificate& cert) {
  return base::ToLowerASCII(base::HexEncode(base::SHA1Hash(cert.cert_span())));
}

// Generates a gtest failure whenever extension JS reports failure.
class JsFailureObserver : public extensions::TestApiObserver {
 public:
  JsFailureObserver() {
    test_api_observation_.Observe(
        extensions::TestApiObserverRegistry::GetInstance());
  }
  ~JsFailureObserver() override = default;

  void OnTestFailed(content::BrowserContext* browser_context,
                    const std::string& message) override {
    ADD_FAILURE() << "Received failure notification from the JS side: "
                  << message;
  }

 private:
  base::ScopedObservation<extensions::TestApiObserverRegistry,
                          extensions::TestApiObserver>
      test_api_observation_{this};
};

class CertificateProviderApiTest : public extensions::ExtensionApiTest {
 public:
  CertificateProviderApiTest() {}

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();

    // Observe all assertion failures in the JS code, even those that happen
    // when there's no active `ResultCatcher`.
    js_failure_observer_ = std::make_unique<JsFailureObserver>();

    // Set up the AutoSelectCertificateForUrls policy to avoid the client
    // certificate selection dialog.
    const std::string autoselect_pattern = R"({"pattern": "*", "filter": {}})";

    base::Value::List autoselect_policy;
    autoselect_policy.Append(autoselect_pattern);

    policy_map_.Set(policy::key::kAutoSelectCertificateForUrls,
                    policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                    policy::POLICY_SOURCE_CLOUD,
                    base::Value(std::move(autoselect_policy)), nullptr);
    provider_.UpdateChromePolicy(policy_map_);

    content::RunAllPendingInMessageLoop();

    cert_provider_service_ =
        chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
            profile());
  }

  // Starts an HTTPS test server that requests a client certificate.
  bool StartHttpsServer(uint16_t ssl_protocol_version) {
    net::SSLServerConfig ssl_server_config;
    ssl_server_config.client_cert_type =
        net::SSLServerConfig::REQUIRE_CLIENT_CERT;
    ssl_server_config.version_max = ssl_protocol_version;
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK,
                                ssl_server_config);
    https_server_->RegisterRequestHandler(
        base::BindRepeating(&CertificateProviderApiTest::OnHttpsServerRequested,
                            base::Unretained(this)));
    return https_server_->Start();
  }

  void CheckCertificateProvidedByExtension(
      const net::X509Certificate& certificate,
      const extensions::Extension& extension) {
    bool is_currently_provided = false;
    std::string provider_extension_id;
    cert_provider_service_->LookUpCertificate(
        certificate, &is_currently_provided, &provider_extension_id);
    EXPECT_TRUE(is_currently_provided);
    EXPECT_EQ(provider_extension_id, extension.id());
  }

  void CheckCertificateAbsent(const net::X509Certificate& certificate) {
    bool is_currently_provided = true;
    std::string provider_extension_id;
    cert_provider_service_->LookUpCertificate(
        certificate, &is_currently_provided, &provider_extension_id);
    EXPECT_FALSE(is_currently_provided);
  }

  std::vector<scoped_refptr<net::X509Certificate>>
  GetAllProvidedCertificates() {
    std::unique_ptr<chromeos::CertificateProvider> cert_provider =
        cert_provider_service_->CreateCertificateProvider();

    base::test::TestFuture<net::ClientCertIdentityList> get_certificates_future;
    cert_provider->GetCertificates(get_certificates_future.GetCallback());

    std::vector<scoped_refptr<net::X509Certificate>> all_provided_certificates;
    for (const auto& cert_identity : get_certificates_future.Get()) {
      all_provided_certificates.push_back(cert_identity->certificate());
    }

    return all_provided_certificates;
  }

  GURL GetHttpsClientCertUrl() const {
    return https_server_->GetURL(kClientCertUrl);
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  raw_ptr<chromeos::CertificateProviderService, AcrossTasksDanglingUntriaged>
      cert_provider_service_ = nullptr;
  policy::PolicyMap policy_map_;

 private:
  const char* const kClientCertUrl = "/client-cert";

  std::unique_ptr<net::test_server::HttpResponse> OnHttpsServerRequested(
      const net::test_server::HttpRequest& request) const {
    if (request.relative_url != kClientCertUrl)
      return nullptr;
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (!request.ssl_info || !request.ssl_info->cert) {
      response->set_code(net::HTTP_FORBIDDEN);
      return response;
    }
    response->set_content("got client cert with fingerprint: " +
                          GetCertFingerprint1(*request.ssl_info->cert));
    response->set_content_type("text/plain");
    return response;
  }

  std::unique_ptr<JsFailureObserver> js_failure_observer_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

// Tests the API with a test extension in place. Tests can cause the extension
// to trigger API calls.
class CertificateProviderApiMockedExtensionTest
    : public CertificateProviderApiTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
#if BUILDFLAG(IS_CHROMEOS_LACROS)  // Needed for ClientCertStoreLacros
    CertDbInitializerFactory::GetInstance()
        ->SetCreateWithBrowserContextForTesting(
            /*should_create=*/true);
#endif
    CertificateProviderApiTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    CertificateProviderApiTest::SetUpOnMainThread();

    extension_path_ = test_data_dir_.AppendASCII("certificate_provider");
    extension_ = LoadExtension(extension_path_);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), extension_->GetResourceURL("basic.html")));

    extension_contents_ = browser()->tab_strip_model()->GetActiveWebContents();

    std::string raw_certificate = GetCertificateData();
    std::vector<uint8_t> certificate_bytes(raw_certificate.begin(),
                                           raw_certificate.end());
    ExecuteJavascript("initialize(" + JsUint8Array(certificate_bytes) + ");");
  }

  content::RenderFrameHost* GetExtensionMainFrame() const {
    return extension_contents_->GetPrimaryMainFrame();
  }

  void ExecuteJavascript(const std::string& function) const {
    ASSERT_TRUE(content::ExecJs(GetExtensionMainFrame(), function));
  }

  // Calls |function| in the extension. |function| needs to return a bool or a
  // Promise<bool>. If it returns a Promise<bool>, this waits for the promise to
  // resolve.
  void ExecuteJavascriptAndWaitForCallback(const std::string& function) const {
    ASSERT_EQ(true, content::EvalJs(GetExtensionMainFrame(), function));
  }

  const extensions::Extension* extension() const { return extension_; }

  std::string GetKeyPk8() const {
    std::string key_pk8;
    base::ScopedAllowBlockingForTesting allow_io;
    EXPECT_TRUE(base::ReadFileToString(
        extension_path_.AppendASCII("l1_leaf.pk8"), &key_pk8));
    return key_pk8;
  }

  // Returns the certificate stored in
  // chrome/test/data/extensions/api_test/certificate_provider
  scoped_refptr<net::X509Certificate> GetCertificate() const {
    std::string raw_certificate = GetCertificateData();
    return net::X509Certificate::CreateFromBytes(
        base::as_bytes(base::make_span(raw_certificate)));
  }

  // Tests the api by navigating to a webpage that requests to perform a
  // signature operation with the available certificate.
  // This signs the request using the algorithm specified by
  // `openssl_signature_algorithm`, with additionally hashing it if
  // `is_raw_data` is true, and replies to the page.
  void TestNavigationToCertificateRequestingWebPage(
      const std::string& expected_request_signature_algorithm,
      uint16_t openssl_signature_algorithm,
      bool is_raw_data) {
    content::TestNavigationObserver navigation_observer(
        nullptr /* no WebContents */);
    navigation_observer.StartWatchingNewWebContents();
    ExtensionTestMessageListener sign_digest_listener(
        "signature request received");

    // Navigate to a page which triggers a sign request. Navigation is blocked
    // by completion of this request, so we don't wait for navigation to finish.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GetHttpsClientCertUrl(),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_NO_WAIT);

    content::WebContents* const https_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Wait for the extension to receive the sign request.
    ASSERT_TRUE(sign_digest_listener.WaitUntilSatisfied());
    EXPECT_GT(cert_provider_service_->pin_dialog_manager()
                  ->StoredSignRequestsForTesting(),
              0);

    // Check that the certificate is available.
    scoped_refptr<net::X509Certificate> certificate = GetCertificate();
    CheckCertificateProvidedByExtension(*certificate, *extension());

    // Fetch the data from the sign request.
    EXPECT_EQ(
        expected_request_signature_algorithm,
        content::EvalJs(GetExtensionMainFrame(), "signatureRequestAlgorithm;"));

    base::test::TestFuture<base::Value> exec_js_future;
    GetExtensionMainFrame()->ExecuteJavaScriptForTests(
        u"signatureRequestData;", exec_js_future.GetCallback(),
        content::ISOLATED_WORLD_ID_GLOBAL);
    std::vector<uint8_t> request_data(exec_js_future.Get().GetBlob());

    // Load the private key.
    std::string key_pk8 = GetKeyPk8();
    std::unique_ptr<crypto::RSAPrivateKey> key(
        crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(
            base::as_bytes(base::make_span(key_pk8))));
    ASSERT_TRUE(key);

    // Sign using the private key.
    std::vector<uint8_t> signature;
    if (is_raw_data) {
      EXPECT_TRUE(RsaSignRawData(openssl_signature_algorithm, request_data,
                                 key.get(), &signature));
    } else {
      EXPECT_TRUE(RsaSignPrehashed(openssl_signature_algorithm, request_data,
                                   key.get(), &signature));
    }

    // Inject the signature back to the extension and let it reply.
    ExecuteJavascript("replyWithSignature(" + JsUint8Array(signature) + ");");

    // Wait for the https navigation to finish.
    navigation_observer.Wait();

    // Make sure that sign request is removed from pin dialog manager.
    EXPECT_EQ(cert_provider_service_->pin_dialog_manager()
                  ->StoredSignRequestsForTesting(),
              0);

    // Check whether the server acknowledged that a client certificate was
    // presented.
    const std::string client_cert_fingerprint =
        GetCertFingerprint1(*certificate);
    EXPECT_EQ(GetPageTextContent(https_contents),
              "got client cert with fingerprint: " + client_cert_fingerprint);
  }

  void SetInterstitialBypass() {
    // Navigate to the test server in a new tab (to not clobber the test
    // fixture setup.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GetHttpsClientCertUrl(),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

    // Proceed through the interstitial to set an SSL bypass for this host.
    content::TestNavigationObserver nav_observer(tab,
                                                 /*number_of_navigations=*/1);
    ASSERT_TRUE(content::ExecJs(
        tab, "window.certificateErrorPageController.proceed();"));
    nav_observer.Wait();

    // Close the new tab to go back to the state set up by SetUpOnMainThread().
    tab->Close();
  }

 private:
  std::string GetCertificateData() const {
    const base::FilePath certificate_path =
        test_data_dir_.AppendASCII("certificate_provider")
            .AppendASCII("l1_leaf.der");
    std::string certificate_data;
    base::ScopedAllowBlockingForTesting allow_io;
    EXPECT_TRUE(base::ReadFileToString(certificate_path, &certificate_data));
    return certificate_data;
  }

  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged>
      extension_contents_ = nullptr;
  raw_ptr<const extensions::Extension, AcrossTasksDanglingUntriaged>
      extension_ = nullptr;
  base::FilePath extension_path_;
};

class CertificateProviderRequestPinTest : public CertificateProviderApiTest {
 protected:
  static constexpr int kFakeSignRequestId = 123;
  static constexpr int kWrongPinAttemptsLimit = 3;
  static constexpr const char* kCorrectPin = "1234";
  static constexpr const char* kWrongPin = "567";

  void SetUpOnMainThread() override {
    CertificateProviderApiTest::SetUpOnMainThread();
    command_request_listener_ = std::make_unique<ExtensionTestMessageListener>(
        "GetCommand", ReplyBehavior::kWillReply);
    LoadRequestPinExtension();
  }

  void TearDownOnMainThread() override {
    if (command_request_listener_->was_satisfied()) {
      // Avoid destroying a non-replied extension function without.
      command_request_listener_->Reply(/*message=*/std::string());
    }
    command_request_listener_.reset();
    CertificateProviderApiTest::TearDownOnMainThread();
  }

  std::string pin_request_extension_id() const { return extension_->id(); }

  void AddFakeSignRequest(int sign_request_id) {
    cert_provider_service_->pin_dialog_manager()->AddSignRequestId(
        extension_->id(), sign_request_id, {});
  }

  void NavigateTo(const std::string& test_page_file_name) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), extension_->GetResourceURL(test_page_file_name)));
  }

  RequestPinView* GetActivePinDialogView() {
    return cert_provider_service_->pin_dialog_manager()
        ->default_dialog_host_for_testing()
        ->active_view_for_testing();
  }

  views::Widget* GetActivePinDialogWindow() {
    return cert_provider_service_->pin_dialog_manager()
        ->default_dialog_host_for_testing()
        ->active_window_for_testing();
  }

  // Enters the code in the ShowPinDialog window and pushes the OK event.
  void EnterCode(const std::string& code) {
    GetActivePinDialogView()->textfield_for_testing()->SetText(
        base::ASCIIToUTF16(code));
    GetActivePinDialogView()->Accept();
    base::RunLoop().RunUntilIdle();
  }

  // Enters the valid code for extensions from local example folders, in the
  // ShowPinDialog window and waits for the window to close. The extension code
  // is expected to send "Success" message after the validation and request to
  // stopPinRequest is done.
  void EnterCorrectPinAndWaitForMessage() {
    ExtensionTestMessageListener listener("Success");
    EnterCode(kCorrectPin);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  // Enters an invalid code for extensions from local example folders, in the
  // ShowPinDialog window and waits for the window to update with the error. The
  // extension code is expected to send "Invalid PIN" message after the
  // validation and the new requestPin (with the error) is done.
  void EnterWrongPinAndWaitForMessage() {
    ExtensionTestMessageListener listener("Invalid PIN");
    EnterCode(kWrongPin);
    ASSERT_TRUE(listener.WaitUntilSatisfied());

    // Check that we have an error message displayed.
    EXPECT_TRUE(
        GetActivePinDialogView()->IsTextStyleOfErrorLabelCorrectForTesting());
  }

  bool SendCommand(const std::string& command) {
    if (!command_request_listener_->WaitUntilSatisfied())
      return false;
    command_request_listener_->Reply(command);
    command_request_listener_->Reset();
    return true;
  }

  bool SendCommandAndWaitForMessage(const std::string& command,
                                    const std::string& expected_message) {
    ExtensionTestMessageListener listener(expected_message);
    if (!SendCommand(command))
      return false;
    return listener.WaitUntilSatisfied();
  }

 private:
  void LoadRequestPinExtension() {
    const base::FilePath extension_path =
        test_data_dir_.AppendASCII("certificate_provider/request_pin");
    extension_ = LoadExtension(extension_path);
  }

  raw_ptr<const extensions::Extension, AcrossTasksDanglingUntriaged>
      extension_ = nullptr;
  std::unique_ptr<ExtensionTestMessageListener> command_request_listener_;
};

}  // namespace

// Tests an extension that only provides certificates in response to the
// onCertificatesUpdateRequested event.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest,
                       ResponsiveExtension) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_2));
  ExecuteJavascript("registerAsCertificateProvider();");
  ExecuteJavascript("registerForSignatureRequests();");

  TestNavigationToCertificateRequestingWebPage(
      "RSASSA_PKCS1_v1_5_SHA1", SSL_SIGN_RSA_PKCS1_SHA1, /*is_raw_data=*/true);
}

// Tests an extension that only provides certificates in response to the
// legacy onCertificatesRequested event.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest,
                       LegacyResponsiveExtension) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_2));
  ExecuteJavascript("registerAsLegacyCertificateProvider();");
  ExecuteJavascript("registerForLegacySignatureRequests();");

  TestNavigationToCertificateRequestingWebPage("SHA1", SSL_SIGN_RSA_PKCS1_SHA1,
                                               /*is_raw_data=*/false);
}

// Tests that signing a request twice in response to the legacy
// onSignDigestRequested event will fail.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest,
                       LegacyExtensionSigningTwice) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_2));
  ExecuteJavascript("registerAsLegacyCertificateProvider();");
  ExecuteJavascript("registerForLegacySignatureRequests();");

  // This causes a signature request that will be replied to.
  TestNavigationToCertificateRequestingWebPage("SHA1", SSL_SIGN_RSA_PKCS1_SHA1,
                                               /*is_raw_data=*/false);

  // Replying to the signature request a second time must fail.
  ASSERT_EQ(false, content::EvalJs(GetExtensionMainFrame(),
                                   "replyWithSignatureSecondTime();"));
}

// Tests an extension that provides certificates both proactively with
// setCertificates() and in response to onCertificatesUpdateRequested.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest,
                       ProactiveAndResponsiveExtension) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_2));
  ExecuteJavascript("registerAsCertificateProvider();");
  ExecuteJavascript("registerForSignatureRequests();");
  ExecuteJavascriptAndWaitForCallback("setCertificates();");

  scoped_refptr<net::X509Certificate> certificate = GetCertificate();
  CheckCertificateProvidedByExtension(*certificate, *extension());

  TestNavigationToCertificateRequestingWebPage(
      "RSASSA_PKCS1_v1_5_SHA1", SSL_SIGN_RSA_PKCS1_SHA1, /*is_raw_data=*/true);

  // Remove the certificate.
  ExecuteJavascriptAndWaitForCallback("unsetCertificates();");
  CheckCertificateAbsent(*certificate);
}

// Tests an extension that provides certificates both proactively with
// setCertificates() and in response to the legacy onCertificatesRequested.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest,
                       ProactiveAndLegacyResponsiveExtension) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_2));
  ExecuteJavascript("registerAsLegacyCertificateProvider();");
  ExecuteJavascript("registerForLegacySignatureRequests();");
  ExecuteJavascriptAndWaitForCallback("setCertificates();");

  scoped_refptr<net::X509Certificate> certificate = GetCertificate();
  CheckCertificateProvidedByExtension(*certificate, *extension());

  TestNavigationToCertificateRequestingWebPage("SHA1", SSL_SIGN_RSA_PKCS1_SHA1,
                                               /*is_raw_data=*/false);

  // Remove the certificate.
  ExecuteJavascriptAndWaitForCallback("unsetCertificates();");
  CheckCertificateAbsent(*certificate);
}

// Tests an extension that provides certificates both proactively with
// setCertificates() and in response to both events:
// onCertificatesUpdateRequested and legacy onCertificatesRequested. Verify that
// the non-legacy signature event is used.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest,
                       ProactiveAndRedundantLegacyResponsiveExtension) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_2));
  ExecuteJavascript("registerAsCertificateProvider();");
  ExecuteJavascript("registerAsLegacyCertificateProvider();");
  ExecuteJavascript("registerForSignatureRequests();");
  ExecuteJavascript("registerForLegacySignatureRequests();");
  ExecuteJavascriptAndWaitForCallback("setCertificates();");

  scoped_refptr<net::X509Certificate> certificate = GetCertificate();
  CheckCertificateProvidedByExtension(*certificate, *extension());

  TestNavigationToCertificateRequestingWebPage(
      "RSASSA_PKCS1_v1_5_SHA1", SSL_SIGN_RSA_PKCS1_SHA1, /*is_raw_data=*/true);

  // Remove the certificate.
  ExecuteJavascriptAndWaitForCallback("unsetCertificates();");
  CheckCertificateAbsent(*certificate);
}

// Tests an extension that only provides certificates proactively via
// setCertificates().
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest,
                       ProactiveExtension) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_2));
  ExecuteJavascript("registerForSignatureRequests();");
  ExecuteJavascriptAndWaitForCallback("setCertificates();");

  scoped_refptr<net::X509Certificate> certificate = GetCertificate();
  CheckCertificateProvidedByExtension(*certificate, *extension());
  EXPECT_EQ(GetAllProvidedCertificates().size(), 1U);

  TestNavigationToCertificateRequestingWebPage(
      "RSASSA_PKCS1_v1_5_SHA1", SSL_SIGN_RSA_PKCS1_SHA1, /*is_raw_data=*/true);

  // Remove the certificate.
  ExecuteJavascriptAndWaitForCallback("unsetCertificates();");
  CheckCertificateAbsent(*certificate);
  EXPECT_TRUE(GetAllProvidedCertificates().empty());
}

// Tests that all of invalid certificates are rejected.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest,
                       OnlyInvalidCertificates) {
  ExecuteJavascriptAndWaitForCallback("setInvalidCertificates();");
  EXPECT_TRUE(GetAllProvidedCertificates().empty());
}

// Tests the RSA SHA-1 signature algorithm.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest, RsaSha1) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_2));
  ExecuteJavascript("supportedAlgorithms = ['RSASSA_PKCS1_v1_5_SHA1'];");
  ExecuteJavascript("registerForSignatureRequests();");
  ExecuteJavascriptAndWaitForCallback("setCertificates();");
  TestNavigationToCertificateRequestingWebPage("RSASSA_PKCS1_v1_5_SHA1",
                                               SSL_SIGN_RSA_PKCS1_SHA1,
                                               /*is_raw_data=*/true);
}

// Tests the RSA SHA-1 signature algorithm using the legacy version of the API.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest,
                       LegacyRsaSha1) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_2));
  ExecuteJavascript("supportedLegacyHashes = ['SHA1'];");
  ExecuteJavascript("registerAsLegacyCertificateProvider();");
  ExecuteJavascript("registerForLegacySignatureRequests();");
  TestNavigationToCertificateRequestingWebPage("SHA1", SSL_SIGN_RSA_PKCS1_SHA1,
                                               /*is_raw_data=*/false);
}

// Tests the RSA SHA-256 signature algorithm.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest, RsaSha256) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_2));
  ExecuteJavascript("supportedAlgorithms = ['RSASSA_PKCS1_v1_5_SHA256'];");
  ExecuteJavascript("registerForSignatureRequests();");
  ExecuteJavascriptAndWaitForCallback("setCertificates();");
  TestNavigationToCertificateRequestingWebPage("RSASSA_PKCS1_v1_5_SHA256",
                                               SSL_SIGN_RSA_PKCS1_SHA256,
                                               /*is_raw_data=*/true);
}

// Tests the RSA SHA-256 signature algorithm using the legacy version of the
// API.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest,
                       LegacyRsaSha256) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_2));
  ExecuteJavascript("supportedLegacyHashes = ['SHA256'];");
  ExecuteJavascript("registerAsLegacyCertificateProvider();");
  ExecuteJavascript("registerForLegacySignatureRequests();");
  TestNavigationToCertificateRequestingWebPage("SHA256",
                                               SSL_SIGN_RSA_PKCS1_SHA256,
                                               /*is_raw_data=*/false);
}

// Tests the RSA SHA-384 signature algorithm.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest, RsaSha384) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_2));
  ExecuteJavascript("supportedAlgorithms = ['RSASSA_PKCS1_v1_5_SHA384'];");
  ExecuteJavascript("registerForSignatureRequests();");
  ExecuteJavascriptAndWaitForCallback("setCertificates();");
  TestNavigationToCertificateRequestingWebPage("RSASSA_PKCS1_v1_5_SHA384",
                                               SSL_SIGN_RSA_PKCS1_SHA384,
                                               /*is_raw_data=*/true);
}

// Tests the RSA SHA-384 signature algorithm using the legacy version of the
// API.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest,
                       LegacyRsaSha384) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_2));
  ExecuteJavascript("supportedLegacyHashes = ['SHA384'];");
  ExecuteJavascript("registerAsLegacyCertificateProvider();");
  ExecuteJavascript("registerForLegacySignatureRequests();");
  TestNavigationToCertificateRequestingWebPage("SHA384",
                                               SSL_SIGN_RSA_PKCS1_SHA384,
                                               /*is_raw_data=*/false);
}

// Tests the RSA SHA-512 signature algorithm.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest, RsaSha512) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_2));
  ExecuteJavascript("supportedAlgorithms = ['RSASSA_PKCS1_v1_5_SHA512'];");
  ExecuteJavascript("registerForSignatureRequests();");
  ExecuteJavascriptAndWaitForCallback("setCertificates();");
  TestNavigationToCertificateRequestingWebPage("RSASSA_PKCS1_v1_5_SHA512",
                                               SSL_SIGN_RSA_PKCS1_SHA512,
                                               /*is_raw_data=*/true);
}

// Tests the RSA SHA-512 signature algorithm using the legacy version of the
// API.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest,
                       LegacyRsaSha512) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_2));
  ExecuteJavascript("supportedLegacyHashes = ['SHA512'];");
  ExecuteJavascript("registerAsLegacyCertificateProvider();");
  ExecuteJavascript("registerForLegacySignatureRequests();");
  TestNavigationToCertificateRequestingWebPage("SHA512",
                                               SSL_SIGN_RSA_PKCS1_SHA512,
                                               /*is_raw_data=*/false);
}

// Tests that the RSA SHA-512 signature algorithm is still used when there are
// other, less strong, algorithms specified after it.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest,
                       RsaSha512AndOthers) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_2));
  ExecuteJavascript(
      "supportedAlgorithms = ['RSASSA_PKCS1_v1_5_SHA512', "
      "'RSASSA_PKCS1_v1_5_SHA1', 'RSASSA_PKCS1_v1_5_MD5_SHA1'];");
  ExecuteJavascript("registerForSignatureRequests();");
  ExecuteJavascriptAndWaitForCallback("setCertificates();");
  TestNavigationToCertificateRequestingWebPage("RSASSA_PKCS1_v1_5_SHA512",
                                               SSL_SIGN_RSA_PKCS1_SHA512,
                                               /*is_raw_data=*/true);
}

// Tests the RSA-PSS SHA-256 signature algorithm.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest,
                       RsaPssSha256) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_3));
  ExecuteJavascript("supportedAlgorithms = ['RSASSA_PSS_SHA256'];");
  ExecuteJavascript("registerForSignatureRequests();");
  ExecuteJavascriptAndWaitForCallback("setCertificates();");
  TestNavigationToCertificateRequestingWebPage("RSASSA_PSS_SHA256",
                                               SSL_SIGN_RSA_PSS_RSAE_SHA256,
                                               /*is_raw_data=*/true);
}

// Tests the RSA-PSS SHA-384 signature algorithm.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest,
                       RsaPssSha384) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_3));
  ExecuteJavascript("supportedAlgorithms = ['RSASSA_PSS_SHA384'];");
  ExecuteJavascript("registerForSignatureRequests();");
  ExecuteJavascriptAndWaitForCallback("setCertificates();");
  TestNavigationToCertificateRequestingWebPage("RSASSA_PSS_SHA384",
                                               SSL_SIGN_RSA_PSS_RSAE_SHA384,
                                               /*is_raw_data=*/true);
}

// Tests the RSA-PSS SHA-512 signature algorithm.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiMockedExtensionTest,
                       RsaPssSha512) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_3));
  ExecuteJavascript("supportedAlgorithms = ['RSASSA_PSS_SHA512'];");
  ExecuteJavascript("registerForSignatureRequests();");
  ExecuteJavascriptAndWaitForCallback("setCertificates();");
  TestNavigationToCertificateRequestingWebPage("RSASSA_PSS_SHA512",
                                               SSL_SIGN_RSA_PSS_RSAE_SHA512,
                                               /*is_raw_data=*/true);
}

// Test that the certificateProvider events are delivered correctly in the
// scenario when the event listener is in a lazy background page that gets idle.
// Disabled due to flakiness - https://crbug.com/1279724
IN_PROC_BROWSER_TEST_F(CertificateProviderApiTest,
                       DISABLED_LazyBackgroundPage) {
  ASSERT_TRUE(StartHttpsServer(net::SSL_PROTOCOL_VERSION_TLS1_2));
  // Make extension background pages idle immediately.
  extensions::ProcessManager::SetEventPageIdleTimeForTesting(1);
  extensions::ProcessManager::SetEventPageSuspendingTimeForTesting(1);

  // Load the test extension.
  ash::TestCertificateProviderExtension test_certificate_provider_extension(
      profile());
  extensions::ExtensionHostTestHelper host_helper(
      profile(), ash::TestCertificateProviderExtension::extension_id());
  host_helper.RestrictToType(
      extensions::mojom::ViewType::kExtensionBackgroundPage);
  const extensions::Extension* const extension =
      LoadExtension(base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
                        .AppendASCII("extensions")
                        .AppendASCII("test_certificate_provider")
                        .AppendASCII("extension"));
  ASSERT_TRUE(extension);
  EXPECT_EQ(extension->id(),
            ash::TestCertificateProviderExtension::extension_id());
  host_helper.WaitForHostCompletedFirstLoad();

  // Navigate to the page that requests the client authentication. Use the
  // incognito profile in order to force re-authentication in the later request
  // made by the test.
  const std::string client_cert_fingerprint = GetCertFingerprint1(
      *ash::TestCertificateProviderExtension::GetCertificate());
  Browser* const incognito_browser = CreateIncognitoBrowser(profile());
  ASSERT_TRUE(incognito_browser);
  ui_test_utils::NavigateToURLWithDisposition(
      incognito_browser, GetHttpsClientCertUrl(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(test_certificate_provider_extension.certificate_request_count(), 1);
  EXPECT_EQ(GetPageTextContent(
                incognito_browser->tab_strip_model()->GetActiveWebContents()),
            "got client cert with fingerprint: " + client_cert_fingerprint);
  CheckCertificateProvidedByExtension(
      *ash::TestCertificateProviderExtension::GetCertificate(), *extension);

  // Let the extension's background page become idle.
  WaitForExtensionIdle(extension->id());

  // Navigate again to the page with the client authentication. The extension
  // gets awakened and handles the request.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetHttpsClientCertUrl(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(test_certificate_provider_extension.certificate_request_count(), 2);
  EXPECT_EQ(
      GetPageTextContent(browser()->tab_strip_model()->GetActiveWebContents()),
      "got client cert with fingerprint: " + client_cert_fingerprint);
}

// User enters the correct PIN.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, ShowPinDialogAccept) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("basic.html");

  // Enter the valid PIN.
  EnterCorrectPinAndWaitForMessage();

  // The view should be set to nullptr when the window is closed.
  EXPECT_FALSE(GetActivePinDialogView());
}

// User closes the dialog kMaxClosedDialogsPerMinute times, and the extension
// should be blocked from showing it again.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, ShowPinDialogClose) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("basic.html");

  for (int i = 0;
       i < extensions::api::certificate_provider::kMaxClosedDialogsPerMinute;
       i++) {
    ExtensionTestMessageListener listener("User closed the dialog");
    GetActivePinDialogWindow()->Close();
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  ExtensionTestMessageListener close_listener("User closed the dialog",
                                              ReplyBehavior::kWillReply);
  GetActivePinDialogWindow()->Close();
  ASSERT_TRUE(close_listener.WaitUntilSatisfied());
  close_listener.Reply("GetLastError");
  ExtensionTestMessageListener last_error_listener(
      "This request exceeds the MAX_PIN_DIALOGS_CLOSED_PER_MINUTE quota.");
  ASSERT_TRUE(last_error_listener.WaitUntilSatisfied());
  EXPECT_FALSE(GetActivePinDialogView());
}

// User enters a wrong PIN first and a correct PIN on the second try.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest,
                       ShowPinDialogWrongPin) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("basic.html");
  EnterWrongPinAndWaitForMessage();

  // The window should be active.
  EXPECT_TRUE(GetActivePinDialogWindow()->IsVisible());
  EXPECT_TRUE(GetActivePinDialogView());

  // Enter the valid PIN.
  EnterCorrectPinAndWaitForMessage();

  // The view should be set to nullptr when the window is closed.
  EXPECT_FALSE(GetActivePinDialogView());
}

// User enters wrong PIN three times.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest,
                       ShowPinDialogWrongPinThreeTimes) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("basic.html");
  for (int i = 0; i < kWrongPinAttemptsLimit; i++)
    EnterWrongPinAndWaitForMessage();

  // The textfield has to be disabled, as extension does not allow input now.
  EXPECT_FALSE(GetActivePinDialogView()->textfield_for_testing()->GetEnabled());

  // Close the dialog.
  ExtensionTestMessageListener listener("No attempt left");
  GetActivePinDialogWindow()->Close();
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_FALSE(GetActivePinDialogView());
}

// User closes the dialog while the extension is processing the request.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest,
                       ShowPinDialogCloseWhileProcessing) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage("Request", "request1:begun"));
  ExtensionTestMessageListener listener(
      base::StringPrintf("request1:success:%s", kWrongPin));
  EnterCode(kWrongPin);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  GetActivePinDialogWindow()->Close();
  base::RunLoop().RunUntilIdle();

  // The view should be set to nullptr when the window is closed.
  EXPECT_FALSE(GetActivePinDialogView());
}

// Extension closes the dialog kMaxClosedDialogsPerMinute times after the user
// inputs some value, and it should be blocked from showing it again.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest,
                       RepeatedProgrammaticCloseAfterInput) {
  NavigateTo("operated.html");

  for (int i = 0;
       i <
       extensions::api::certificate_provider::kMaxClosedDialogsPerMinute + 1;
       i++) {
    AddFakeSignRequest(kFakeSignRequestId);
    EXPECT_TRUE(SendCommandAndWaitForMessage(
        "Request", base::StringPrintf("request%d:begun", i + 1)));

    EnterCode(kCorrectPin);
    EXPECT_TRUE(SendCommandAndWaitForMessage(
        "Stop", base::StringPrintf("stop%d:success", i + 1)));
    EXPECT_FALSE(GetActivePinDialogView());
  }

  AddFakeSignRequest(kFakeSignRequestId);
  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "Request",
      base::StringPrintf(
          "request%d:error:This request exceeds the "
          "MAX_PIN_DIALOGS_CLOSED_PER_MINUTE quota.",
          extensions::api::certificate_provider::kMaxClosedDialogsPerMinute +
              2)));
  EXPECT_FALSE(GetActivePinDialogView());
}

// Extension erroneously attempts to close the PIN dialog twice.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, DoubleClose) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommand("Request"));
  EXPECT_TRUE(SendCommandAndWaitForMessage("Stop", "stop1:success"));
  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "Stop", "stop2:error:No active dialog from extension."));
  EXPECT_FALSE(GetActivePinDialogView());
}

// Extension closes the dialog kMaxClosedDialogsPerMinute times before the user
// inputs anything, and it should be blocked from showing it again.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest,
                       RepeatedProgrammaticCloseBeforeInput) {
  NavigateTo("operated.html");

  for (int i = 0;
       i <
       extensions::api::certificate_provider::kMaxClosedDialogsPerMinute + 1;
       i++) {
    AddFakeSignRequest(kFakeSignRequestId);
    EXPECT_TRUE(SendCommand("Request"));
    EXPECT_TRUE(SendCommandAndWaitForMessage(
        "Stop", base::StringPrintf("stop%d:success", i + 1)));
    EXPECT_FALSE(GetActivePinDialogView());
  }

  AddFakeSignRequest(kFakeSignRequestId);
  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "Request",
      base::StringPrintf(
          "request%d:error:This request exceeds the "
          "MAX_PIN_DIALOGS_CLOSED_PER_MINUTE quota.",
          extensions::api::certificate_provider::kMaxClosedDialogsPerMinute +
              2)));
  EXPECT_FALSE(GetActivePinDialogView());
}

// Extension erroneously attempts to stop the PIN request with an error before
// the user provided any input.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest,
                       StopWithErrorBeforeInput) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommand("Request"));
  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "StopWithUnknownError", "stop1:error:No user input received"));
  EXPECT_TRUE(GetActivePinDialogView()->textfield_for_testing()->GetEnabled());
}

// Extension erroneously uses an invalid sign request ID.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, InvalidRequestId) {
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "Request", "request1:error:Invalid signRequestId"));
  EXPECT_FALSE(GetActivePinDialogView());
}

// Extension specifies zero left attempts in the very first PIN request.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, ZeroAttemptsAtStart) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage("RequestWithZeroAttempts",
                                           "request1:begun"));

  // The textfield has to be disabled, as there are no attempts left.
  EXPECT_FALSE(GetActivePinDialogView()->textfield_for_testing()->GetEnabled());

  ExtensionTestMessageListener listener("request1:empty");
  GetActivePinDialogWindow()->Close();
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

// Extension erroneously passes a negative attempts left count.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, NegativeAttempts) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "RequestWithNegativeAttempts", "request1:error:Invalid attemptsLeft"));
  EXPECT_FALSE(GetActivePinDialogView());
}

// Extension erroneously attempts to close a non-existing dialog.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, CloseNonExisting) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "Stop", "stop1:error:No active dialog from extension."));
  EXPECT_FALSE(GetActivePinDialogView());
}

// Extension erroneously attempts to stop a non-existing dialog with an error.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, StopNonExisting) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "StopWithUnknownError", "stop1:error:No active dialog from extension."));
  EXPECT_FALSE(GetActivePinDialogView());
}

// Extension erroneously attempts to start or stop the PIN request before the
// user closed the previously stopped with an error PIN request.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest,
                       UpdateAlreadyStopped) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage("Request", "request1:begun"));
  EnterCode(kWrongPin);
  EXPECT_TRUE(SendCommand("StopWithUnknownError"));

  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "StopWithUnknownError", "stop2:error:No user input received"));
  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "Request", "request2:error:Previous request not finished"));
  EXPECT_FALSE(GetActivePinDialogView()->textfield_for_testing()->GetEnabled());
}

// Extension starts a new PIN request after it stopped the previous one with an
// error.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, StartAfterStop) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage("Request", "request1:begun"));
  EnterCode(kWrongPin);
  EXPECT_TRUE(SendCommandAndWaitForMessage("Stop", "stop1:success"));
  EXPECT_FALSE(GetActivePinDialogView());

  EXPECT_TRUE(SendCommandAndWaitForMessage("Request", "request2:begun"));
  ExtensionTestMessageListener listener(
      base::StringPrintf("request2:success:%s", kCorrectPin));
  EnterCode(kCorrectPin);
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_FALSE(GetActivePinDialogView()->textfield_for_testing()->GetEnabled());
}

// Test that no quota is applied to the first PIN requests for each requestId.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest,
                       RepeatedCloseWithDifferentIds) {
  NavigateTo("operated.html");

  for (int i = 0;
       i <
       extensions::api::certificate_provider::kMaxClosedDialogsPer10Minutes + 2;
       i++) {
    AddFakeSignRequest(kFakeSignRequestId + i);
    EXPECT_TRUE(SendCommandAndWaitForMessage(
        "Request", base::StringPrintf("request%d:begun", i + 1)));

    ExtensionTestMessageListener listener(
        base::StringPrintf("request%d:empty", i + 1));
    ASSERT_TRUE(GetActivePinDialogView());
    GetActivePinDialogView()->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_FALSE(GetActivePinDialogView());

    EXPECT_TRUE(SendCommand("IncrementRequestId"));
  }
}

// Test that disabling the extension closes its PIN dialog.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, ExtensionDisable) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage("Request", "request1:begun"));
  EXPECT_TRUE(GetActivePinDialogView());

  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile()),
      pin_request_extension_id());
  extensions::ExtensionSystem::Get(profile())
      ->extension_service()
      ->DisableExtension(pin_request_extension_id(),
                         extensions::disable_reason::DISABLE_USER_ACTION);
  registry_observer.WaitForExtensionUnloaded();
  // Let the events from the extensions subsystem propagate to the code that
  // manages the PIN dialog.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(GetActivePinDialogView());
}

// Test that reloading the extension closes its PIN dialog.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, ExtensionReload) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage("Request", "request1:begun"));
  EXPECT_TRUE(GetActivePinDialogView());

  // Create a second browser, in order to suppress Chrome shutdown logic when
  // reloading the extension (as the tab with the extension's file gets closed).
  CreateBrowser(profile());

  // Trigger the chrome.runtime.reload() call from the extension.
  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile()),
      pin_request_extension_id());
  EXPECT_TRUE(SendCommand("Reload"));
  registry_observer.WaitForExtensionUnloaded();
  registry_observer.WaitForExtensionLoaded();

  EXPECT_FALSE(GetActivePinDialogView());
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/qwac_web_contents_observer.h"

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "net/base/features.h"
#include "net/cert/internal/trust_store_chrome.h"
#include "net/cert/root_store_proto_lite/root_store.pb.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/cert_builder.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/two_qwac_cert_binding_builder.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

namespace {

class ResponseHandlerBase {
 public:
  ResponseHandlerBase(net::EmbeddedTestServer& server, std::string_view path)
      : expected_path_(path) {
    server.RegisterRequestHandler(base::BindRepeating(
        &ResponseHandlerBase::ServeResponse, base::Unretained(this)));
  }

  std::unique_ptr<net::test_server::HttpResponse> ServeResponse(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().GetPath() != expected_path_) {
      return nullptr;
    }

    ++request_count_;
    return ServeResponseImpl(request);
  }

  virtual std::unique_ptr<net::test_server::HttpResponse> ServeResponseImpl(
      const net::test_server::HttpRequest& request) = 0;

  int request_count() const { return request_count_; }

 private:
  std::atomic_int request_count_ = 0;
  std::string expected_path_;
};

class ServePageWithQwacHeaderHandler : public ResponseHandlerBase {
 public:
  ServePageWithQwacHeaderHandler(net::EmbeddedTestServer& server,
                                 std::string_view path,
                                 std::string_view qwac_link_url)
      : ResponseHandlerBase(server, path), qwac_link_url_(qwac_link_url) {}

  std::unique_ptr<net::test_server::HttpResponse> ServeResponseImpl(
      const net::test_server::HttpRequest& request) override {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content("");
    http_response->AddCustomHeader(
        "Link", base::StringPrintf("<%s>;rel=\"tls-certificate-binding\"",
                                   qwac_link_url_));
    http_response->AddCustomHeader("Cache-Control", "max-age=3600");
    return std::move(http_response);
  }

 private:
  std::string qwac_link_url_;
};

class ServeResponseHandler : public ResponseHandlerBase {
 public:
  ServeResponseHandler(net::EmbeddedTestServer& server,
                       std::string_view path,
                       std::string_view content,
                       std::string_view content_type,
                       net::HttpStatusCode status_code = net::HTTP_OK)
      : ResponseHandlerBase(server, path),
        content_(content),
        content_type_(content_type),
        status_code_(status_code) {}

  std::unique_ptr<net::test_server::HttpResponse> ServeResponseImpl(
      const net::test_server::HttpRequest& request) override {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_content(content_);
    http_response->set_content_type(content_type_);
    http_response->set_code(status_code_);
    http_response->AddCustomHeader("Cache-Control", "max-age=3600");
    return http_response;
  }

 private:
  std::string content_;
  std::string content_type_;
  net::HttpStatusCode status_code_;
};

std::unique_ptr<net::test_server::HttpResponse> ServeRedirectWithQwacHeader(
    const std::string& expected_path,
    const std::string& redirect_url,
    const std::string& qwac_link_url,
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().GetPath() != expected_path) {
    return nullptr;
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url);
  http_response->AddCustomHeader(
      "Link", base::StringPrintf("<%s>;rel=\"tls-certificate-binding\"",
                                 qwac_link_url));
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse> FailTestIfPathRequested(
    const std::string& expected_path,
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().GetPath() != expected_path) {
    return nullptr;
  }

  ADD_FAILURE() << "FailTestIfPathRequested " << request.relative_url;

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_BAD_REQUEST);
  return http_response;
}

bool WaitForStatusFinished(QwacWebContentsObserver::QwacStatus* status) {
  if (status->is_finished()) {
    return true;
  }

  base::test::TestFuture<void> future;
  auto subscription = status->RegisterCallback(future.GetCallback());
  return future.Wait();
}

class QwacWebContentsObserverTestBase : public PlatformBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    PlatformBrowserTest::SetUpOnMainThread();
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  QwacWebContentsObserver::QwacStatus* GetCurrentPageQwacStatus() {
    return QwacWebContentsObserver::QwacStatus::GetForPage(
        web_contents()->GetPrimaryPage());
  }

  scoped_refptr<net::X509Certificate> VisibleSecurityStateTwoQwac() {
    SecurityStateTabHelper* helper =
        SecurityStateTabHelper::FromWebContents(web_contents());
    if (!helper) {
      ADD_FAILURE() << "Failed to load SecurityStateTabHelper";
      return nullptr;
    }
    std::unique_ptr<security_state::VisibleSecurityState>
        visible_security_state = helper->GetVisibleSecurityState();
    if (!visible_security_state) {
      ADD_FAILURE() << "SecurityStateTabHelper has no VisibleSecurityState";
      return nullptr;
    }
    return visible_security_state->two_qwac;
  }

  void ExpectHistogramSample(
      const base::HistogramTester& histograms,
      QwacWebContentsObserver::QwacLinkProcessingResult result) {
    histograms.ExpectUniqueSample("Net.CertVerifier.Qwac.2QwacLinkProcessing",
                                  result, 1u);
  }

  void ExpectNoHistogramSample(const base::HistogramTester& histograms) {
    histograms.ExpectTotalCount("Net.CertVerifier.Qwac.2QwacLinkProcessing", 0);
  }
};

class QwacWebContentsObserverDisabledBrowserTest
    : public QwacWebContentsObserverTestBase {
 public:
  QwacWebContentsObserverDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(net::features::kVerifyQWACs);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverDisabledBrowserTest,
                       TestFeatureDisabled) {
  ServePageWithQwacHeaderHandler main_page_handler(embedded_https_test_server(),
                                                   "/main", "/qwac");
  embedded_https_test_server().RegisterRequestHandler(
      base::BindRepeating(&FailTestIfPathRequested, "/qwac"));
  auto test_server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  base::HistogramTester histograms;
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/main")));

  EXPECT_FALSE(GetCurrentPageQwacStatus());
  EXPECT_FALSE(VisibleSecurityStateTwoQwac());
  ExpectNoHistogramSample(histograms);
}

class QwacWebContentsObserverBrowserTest
    : public QwacWebContentsObserverTestBase {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    // Disable certificate transparency so that adding test roots as known
    // roots works without having to add test CT setup as well.
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        false);
    PlatformBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  // Installs a CRS proto update that trusts `root_cert` for TLS and adds
  // `eutl_cert` as an EUTL.
  //
  // Technically the TLS cert bound by a 2-QWAC doesn't need to be trusted
  // by a root that is in the CRS, but the CRS update will fail if
  // `trust_anchors()` is empty. To satisfy this we put the test root in the
  // CRS anchors list. Because it doesn't matter for the test whether the root
  // is trusted by the CRS or by TestRootCerts, we don't bother with creating
  // a unique test root or clearing TestRootCerts.
  void InstallCRSUpdate(net::X509Certificate* root_cert,
                        net::X509Certificate* eutl_cert) {
    ASSERT_TRUE(root_cert);
    ASSERT_TRUE(eutl_cert);

    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version_);

    auto* anchor = root_store_proto.add_trust_anchors();
    anchor->set_der(base::as_string_view(root_cert->cert_span()));

    auto* additional_cert = root_store_proto.add_additional_certs();
    additional_cert->set_der(base::as_string_view(eutl_cert->cert_span()));
    additional_cert->set_eutl(true);

    base::RunLoop update_run_loop;
    content::GetCertVerifierServiceFactory()->UpdateChromeRootStore(
        mojo_base::ProtoWrapper(root_store_proto),
        update_run_loop.QuitClosure());
    update_run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      net::features::kVerifyQWACs};

  int64_t crs_version_ = net::CompiledChromeRootStoreVersion();
};

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       TestSuccessAndCachedGoBack) {
  ASSERT_TRUE(embedded_https_test_server().InitializeAndListen());

  net::TwoQwacCertBindingBuilder binding_builder;
  auto tls_leaf = embedded_https_test_server().GetCertificate();
  ASSERT_TRUE(tls_leaf);
  binding_builder.SetBoundCerts(
      {std::string(base::as_string_view(tls_leaf->cert_span()))});
  ServeResponseHandler qwac_handler(embedded_https_test_server(), "/qwac",
                                    binding_builder.GetJWS(),
                                    "application/jose");

  ServePageWithQwacHeaderHandler main_page_handler(embedded_https_test_server(),
                                                   "/main", "/qwac");
  ServeResponseHandler nonqwac_page_handler(embedded_https_test_server(),
                                            "/main_noqwac", "", "text/html");
  auto test_server_handle =
      embedded_https_test_server().StartAcceptingConnectionsAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  InstallCRSUpdate(
      /*root_cert=*/embedded_https_test_server().GetRoot().get(),
      /*eutl_cert=*/
      binding_builder.GetRootBuilder()->GetX509Certificate().get());

  {
    base::HistogramTester histograms;
    EXPECT_TRUE(content::NavigateToURL(
        web_contents(),
        embedded_https_test_server().GetURL("www.example.com", "/main")));

    auto* status = GetCurrentPageQwacStatus();
    ASSERT_TRUE(status);
    ASSERT_TRUE(WaitForStatusFinished(status));
    ASSERT_TRUE(status->verified_2qwac_cert());
    EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
        binding_builder.GetLeafBuilder()->GetX509CertificateFullChain().get()));
    ASSERT_TRUE(status->tls_cert());
    EXPECT_TRUE(status->tls_cert()->EqualsExcludingChain(tls_leaf.get()));
    EXPECT_EQ(1, main_page_handler.request_count());
    EXPECT_EQ(1, qwac_handler.request_count());

    auto visible_security_state_two_qwac = VisibleSecurityStateTwoQwac();
    EXPECT_TRUE(visible_security_state_two_qwac);
    EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
        visible_security_state_two_qwac.get()));

    ExpectHistogramSample(
        histograms,
        QwacWebContentsObserver::QwacLinkProcessingResult::kValid2Qwac);
  }

  {
    base::HistogramTester histograms;
    // Navigate to a different page without a qwac.
    EXPECT_TRUE(content::NavigateToURL(
        web_contents(), embedded_https_test_server().GetURL("www.example.com",
                                                            "/main_noqwac")));
    EXPECT_FALSE(GetCurrentPageQwacStatus());
    EXPECT_FALSE(VisibleSecurityStateTwoQwac());
    ExpectHistogramSample(
        histograms,
        QwacWebContentsObserver::QwacLinkProcessingResult::kNoQwacLinkHeader);
  }

  {
    base::HistogramTester histograms;
    // Now go back to the previous page. If BFCache is enabled, qwac status
    // should still be available and not need to be re-fetched, otherwise it
    // should be re-fetched successfully.
    ASSERT_TRUE(HistoryGoBack(web_contents()));

    auto* status = GetCurrentPageQwacStatus();
    ASSERT_TRUE(status);
    if (base::FeatureList::IsEnabled(features::kBackForwardCache)) {
      ASSERT_TRUE(status->is_finished());
      ExpectHistogramSample(histograms,
                            QwacWebContentsObserver::QwacLinkProcessingResult::
                                kQwacStatusAlreadyPresent);
    } else {
      ASSERT_TRUE(WaitForStatusFinished(status));
      ExpectHistogramSample(
          histograms,
          QwacWebContentsObserver::QwacLinkProcessingResult::kValid2Qwac);
    }
    ASSERT_TRUE(status->verified_2qwac_cert());
    EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
        binding_builder.GetLeafBuilder()->GetX509CertificateFullChain().get()));
    ASSERT_TRUE(status->tls_cert());
    EXPECT_TRUE(status->tls_cert()->EqualsExcludingChain(tls_leaf.get()));
    EXPECT_EQ(1, main_page_handler.request_count());
    EXPECT_EQ(1, qwac_handler.request_count());

    auto visible_security_state_two_qwac = VisibleSecurityStateTwoQwac();
    EXPECT_TRUE(visible_security_state_two_qwac);
    EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
        visible_security_state_two_qwac.get()));
  }
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       TestSuccessAndCachedNavigateSamePage) {
  ASSERT_TRUE(embedded_https_test_server().InitializeAndListen());

  net::TwoQwacCertBindingBuilder binding_builder;
  auto tls_leaf = embedded_https_test_server().GetCertificate();
  ASSERT_TRUE(tls_leaf);
  binding_builder.SetBoundCerts(
      {std::string(base::as_string_view(tls_leaf->cert_span()))});
  ServeResponseHandler qwac_handler(embedded_https_test_server(), "/qwac",
                                    binding_builder.GetJWS(),
                                    "application/jose");

  ServePageWithQwacHeaderHandler main_page_handler(embedded_https_test_server(),
                                                   "/main", "/qwac");
  ServeResponseHandler nonqwac_page_handler(embedded_https_test_server(),
                                            "/main_noqwac", "", "text/html");
  auto test_server_handle =
      embedded_https_test_server().StartAcceptingConnectionsAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  InstallCRSUpdate(
      /*root_cert=*/embedded_https_test_server().GetRoot().get(),
      /*eutl_cert=*/
      binding_builder.GetRootBuilder()->GetX509Certificate().get());

  {
    base::HistogramTester histograms;
    EXPECT_TRUE(content::NavigateToURL(
        web_contents(),
        embedded_https_test_server().GetURL("www.example.com", "/main")));

    auto* status = GetCurrentPageQwacStatus();
    ASSERT_TRUE(status);
    ASSERT_TRUE(WaitForStatusFinished(status));
    ASSERT_TRUE(status->verified_2qwac_cert());
    EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
        binding_builder.GetLeafBuilder()->GetX509CertificateFullChain().get()));
    ASSERT_TRUE(status->tls_cert());
    EXPECT_TRUE(status->tls_cert()->EqualsExcludingChain(tls_leaf.get()));
    EXPECT_EQ(1, main_page_handler.request_count());
    EXPECT_EQ(1, qwac_handler.request_count());

    auto visible_security_state_two_qwac = VisibleSecurityStateTwoQwac();
    EXPECT_TRUE(visible_security_state_two_qwac);
    EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
        visible_security_state_two_qwac.get()));

    ExpectHistogramSample(
        histograms,
        QwacWebContentsObserver::QwacLinkProcessingResult::kValid2Qwac);
  }

  // Navigate to a different page without a qwac.
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("www.example.com", "/main_noqwac")));
  EXPECT_FALSE(GetCurrentPageQwacStatus());
  EXPECT_FALSE(VisibleSecurityStateTwoQwac());

  {
    base::HistogramTester histograms;
    // Now navigate to the original page URL again.
    // The page and the 2-QWAC should be in HTTP cache, so the page and qwac
    // status should be loaded without reloading either from the http server.
    EXPECT_TRUE(content::NavigateToURL(
        web_contents(),
        embedded_https_test_server().GetURL("www.example.com", "/main")));

    auto* status = GetCurrentPageQwacStatus();
    ASSERT_TRUE(status);
    ASSERT_TRUE(WaitForStatusFinished(status));
    ASSERT_TRUE(status->verified_2qwac_cert());
    EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
        binding_builder.GetLeafBuilder()->GetX509CertificateFullChain().get()));
    ASSERT_TRUE(status->tls_cert());
    EXPECT_TRUE(status->tls_cert()->EqualsExcludingChain(tls_leaf.get()));
    EXPECT_EQ(1, main_page_handler.request_count());
    EXPECT_EQ(1, qwac_handler.request_count());

    auto visible_security_state_two_qwac = VisibleSecurityStateTwoQwac();
    EXPECT_TRUE(visible_security_state_two_qwac);
    EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
        visible_security_state_two_qwac.get()));

    ExpectHistogramSample(
        histograms,
        QwacWebContentsObserver::QwacLinkProcessingResult::kValid2Qwac);
  }
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       TestReloadDifferentCertAndQwac) {
  int port;
  {
    ASSERT_TRUE(embedded_https_test_server().InitializeAndListen());
    net::TwoQwacCertBindingBuilder binding_builder;
    auto tls_leaf = embedded_https_test_server().GetCertificate();
    ASSERT_TRUE(tls_leaf);
    binding_builder.SetBoundCerts(
        {std::string(base::as_string_view(tls_leaf->cert_span()))});
    ServeResponseHandler qwac_handler(embedded_https_test_server(), "/qwac",
                                      binding_builder.GetJWS(),
                                      "application/jose");
    ServePageWithQwacHeaderHandler main_page_handler(
        embedded_https_test_server(), "/main", "/qwac");
    auto test_server_handle =
        embedded_https_test_server().StartAcceptingConnectionsAndReturnHandle();
    ASSERT_TRUE(test_server_handle.is_valid());

    InstallCRSUpdate(
        /*root_cert=*/embedded_https_test_server().GetRoot().get(),
        /*eutl_cert=*/
        binding_builder.GetRootBuilder()->GetX509Certificate().get());

    base::HistogramTester histograms;
    EXPECT_TRUE(content::NavigateToURL(
        web_contents(),
        embedded_https_test_server().GetURL("www.example.com", "/main")));

    auto* status = GetCurrentPageQwacStatus();
    ASSERT_TRUE(status);
    ASSERT_TRUE(WaitForStatusFinished(status));
    ASSERT_TRUE(status->verified_2qwac_cert());
    EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
        binding_builder.GetLeafBuilder()->GetX509CertificateFullChain().get()));
    ASSERT_TRUE(status->tls_cert());
    EXPECT_TRUE(status->tls_cert()->EqualsExcludingChain(tls_leaf.get()));

    auto visible_security_state_two_qwac = VisibleSecurityStateTwoQwac();
    EXPECT_TRUE(visible_security_state_two_qwac);
    EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
        visible_security_state_two_qwac.get()));

    ExpectHistogramSample(
        histograms,
        QwacWebContentsObserver::QwacLinkProcessingResult::kValid2Qwac);

    // Save the port number and release the `test_server_handle`, shutting down
    // the existing test server.
    port = embedded_https_test_server().port();
  }

  // Start a new test server on the same port with a different certificate and
  // different 2-QWAC.
  // This might be flaky if the test can't open the same port again, but
  // there's not really any other way to test this.
  net::EmbeddedTestServer https_test_server_2(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server_2.SetCertHostnames({"*.example.com"});
  ASSERT_TRUE(https_test_server_2.InitializeAndListen(port));
  net::TwoQwacCertBindingBuilder binding_builder_2;
  auto tls_leaf_2 = https_test_server_2.GetCertificate();
  ASSERT_TRUE(tls_leaf_2);
  binding_builder_2.SetBoundCerts(
      {std::string(base::as_string_view(tls_leaf_2->cert_span()))});
  ServeResponseHandler qwac_handler(https_test_server_2, "/qwac2",
                                    binding_builder_2.GetJWS(),
                                    "application/jose");
  ServePageWithQwacHeaderHandler main_page_handler(https_test_server_2, "/main",
                                                   "/qwac2");
  auto test_server_handle =
      https_test_server_2.StartAcceptingConnectionsAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  InstallCRSUpdate(
      /*root_cert=*/https_test_server_2.GetRoot().get(),
      /*eutl_cert=*/
      binding_builder_2.GetRootBuilder()->GetX509Certificate().get());

  {
    base::HistogramTester histograms;
    // Reload the page.
    content::TestNavigationObserver reload_observer(
        web_contents(),
        /*expected_number_of_navigations=*/1);
    web_contents()->GetController().Reload(content::ReloadType::BYPASSING_CACHE,
                                           /*check_for_repost=*/false);
    reload_observer.Wait();

    auto* status = GetCurrentPageQwacStatus();
    ASSERT_TRUE(status);
    ASSERT_TRUE(WaitForStatusFinished(status));
    // Should have gotten the new 2-QWAC response.
    ASSERT_TRUE(status->verified_2qwac_cert());
    EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
        binding_builder_2.GetLeafBuilder()
            ->GetX509CertificateFullChain()
            .get()));
    ASSERT_TRUE(status->tls_cert());
    EXPECT_TRUE(status->tls_cert()->EqualsExcludingChain(tls_leaf_2.get()));

    auto visible_security_state_two_qwac = VisibleSecurityStateTwoQwac();
    EXPECT_TRUE(visible_security_state_two_qwac);
    EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
        visible_security_state_two_qwac.get()));

    ExpectHistogramSample(
        histograms,
        QwacWebContentsObserver::QwacLinkProcessingResult::kValid2Qwac);
  }
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       SameDocumentNavigation) {
  ASSERT_TRUE(embedded_https_test_server().InitializeAndListen());

  net::TwoQwacCertBindingBuilder binding_builder;
  auto tls_leaf = embedded_https_test_server().GetCertificate();
  ASSERT_TRUE(tls_leaf);
  binding_builder.SetBoundCerts(
      {std::string(base::as_string_view(tls_leaf->cert_span()))});
  ServeResponseHandler qwac_handler(embedded_https_test_server(), "/qwac",
                                    binding_builder.GetJWS(),
                                    "application/jose");

  ServePageWithQwacHeaderHandler main_page_handler(embedded_https_test_server(),
                                                   "/main", "/qwac");
  ServeResponseHandler nonqwac_page_handler(embedded_https_test_server(),
                                            "/main_noqwac", "", "text/html");
  auto test_server_handle =
      embedded_https_test_server().StartAcceptingConnectionsAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  InstallCRSUpdate(
      /*root_cert=*/embedded_https_test_server().GetRoot().get(),
      /*eutl_cert=*/
      binding_builder.GetRootBuilder()->GetX509Certificate().get());

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("www.example.com", "/main")));

  {
    auto* status = GetCurrentPageQwacStatus();
    ASSERT_TRUE(status);
    ASSERT_TRUE(WaitForStatusFinished(status));
    ASSERT_TRUE(status->verified_2qwac_cert());
    EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
        binding_builder.GetLeafBuilder()->GetX509CertificateFullChain().get()));
    ASSERT_TRUE(status->tls_cert());
    EXPECT_TRUE(status->tls_cert()->EqualsExcludingChain(tls_leaf.get()));
    EXPECT_EQ(1, main_page_handler.request_count());
    EXPECT_EQ(1, qwac_handler.request_count());

    auto visible_security_state_two_qwac = VisibleSecurityStateTwoQwac();
    EXPECT_TRUE(visible_security_state_two_qwac);
    EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
        visible_security_state_two_qwac.get()));
  }

  {
    base::HistogramTester histograms;
    // Navigate to a different URL with the same page.
    // Since the page didn't change, the qwac status should still be available
    // and not need to be re-fetched.
    EXPECT_TRUE(content::NavigateToURL(
        web_contents(), embedded_https_test_server().GetURL("www.example.com",
                                                            "/main#anchor")));

    auto* status = GetCurrentPageQwacStatus();
    ASSERT_TRUE(status);
    ASSERT_TRUE(status->is_finished());
    ASSERT_TRUE(status->verified_2qwac_cert());
    EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
        binding_builder.GetLeafBuilder()->GetX509CertificateFullChain().get()));
    ASSERT_TRUE(status->tls_cert());
    EXPECT_TRUE(status->tls_cert()->EqualsExcludingChain(tls_leaf.get()));
    EXPECT_EQ(1, main_page_handler.request_count());
    EXPECT_EQ(1, qwac_handler.request_count());

    auto visible_security_state_two_qwac = VisibleSecurityStateTwoQwac();
    EXPECT_TRUE(visible_security_state_two_qwac);
    EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
        visible_security_state_two_qwac.get()));

    // Same-page navigations are not histogrammed.
    ExpectNoHistogramSample(histograms);
  }
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       SameOriginRedirectAllowed) {
  ASSERT_TRUE(embedded_https_test_server().InitializeAndListen());

  net::TwoQwacCertBindingBuilder binding_builder;
  auto tls_leaf = embedded_https_test_server().GetCertificate();
  ASSERT_TRUE(tls_leaf);
  binding_builder.SetBoundCerts(
      {std::string(base::as_string_view(tls_leaf->cert_span()))});
  ServeResponseHandler qwac_handler(embedded_https_test_server(), "/qwac",
                                    binding_builder.GetJWS(),
                                    "application/jose");

  ServePageWithQwacHeaderHandler main_page_handler(
      embedded_https_test_server(), "/main", "/server-redirect?/qwac");
  auto test_server_handle =
      embedded_https_test_server().StartAcceptingConnectionsAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  InstallCRSUpdate(
      /*root_cert=*/embedded_https_test_server().GetRoot().get(),
      /*eutl_cert=*/
      binding_builder.GetRootBuilder()->GetX509Certificate().get());

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("www.example.com", "/main")));

  auto* status = GetCurrentPageQwacStatus();
  ASSERT_TRUE(status);
  ASSERT_TRUE(WaitForStatusFinished(status));
  ASSERT_TRUE(status->verified_2qwac_cert());
  EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
      binding_builder.GetLeafBuilder()->GetX509CertificateFullChain().get()));
  ASSERT_TRUE(status->tls_cert());
  EXPECT_TRUE(status->tls_cert()->EqualsExcludingChain(tls_leaf.get()));

  auto visible_security_state_two_qwac = VisibleSecurityStateTwoQwac();
  EXPECT_TRUE(visible_security_state_two_qwac);
  EXPECT_TRUE(status->verified_2qwac_cert()->EqualsIncludingChain(
      visible_security_state_two_qwac.get()));
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       LinkHeaderFromMainPageRedirectIgnored) {
  embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
      &ServeRedirectWithQwacHeader, "/main", /*redirect_url=*/"/realmain",
      /*qwac_link_url=*/"/qwac"));
  ServeResponseHandler page_handler(embedded_https_test_server(), "/realmain",
                                    "", "text/html");
  embedded_https_test_server().RegisterRequestHandler(
      base::BindRepeating(&FailTestIfPathRequested, "/qwac"));
  auto test_server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  base::HistogramTester histograms;
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("www.example.com", "/main"),
      /*expected_commit_url=*/
      embedded_https_test_server().GetURL("www.example.com", "/realmain")));

  // The QWAC link header was present on a redirect, but not on the final page
  // load, so no QwacStatus should be created.
  EXPECT_FALSE(GetCurrentPageQwacStatus());
  EXPECT_FALSE(VisibleSecurityStateTwoQwac());
  ExpectHistogramSample(
      histograms,
      QwacWebContentsObserver::QwacLinkProcessingResult::kNoQwacLinkHeader);
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       CrossOriginRedirectNotAllowed) {
  net::EmbeddedTestServer other_https_test_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  other_https_test_server.RegisterRequestHandler(
      base::BindRepeating(&FailTestIfPathRequested, "/qwac"));
  auto other_test_server_handle =
      other_https_test_server.StartAndReturnHandle();
  ASSERT_TRUE(other_test_server_handle.is_valid());

  ServePageWithQwacHeaderHandler main_page_handler(
      embedded_https_test_server(), "/main",
      "/server-redirect?" +
          other_https_test_server.GetURL("foo.example.com", "/qwac").spec());
  auto test_server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  base::HistogramTester histograms;
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("www.example.com", "/main")));

  auto* status = GetCurrentPageQwacStatus();
  // The initial QWAC url is same-origin so a fetch request should be started
  // and a QwacStatus created, but the fetch should fail once it hits the
  // redirect to a different origin.
  ASSERT_TRUE(status);
  ASSERT_TRUE(WaitForStatusFinished(status));
  EXPECT_FALSE(status->verified_2qwac_cert());
  EXPECT_FALSE(VisibleSecurityStateTwoQwac());
  ExpectHistogramSample(
      histograms,
      QwacWebContentsObserver::QwacLinkProcessingResult::kDownloadFailed);
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       CrossOriginFetchNotAllowed) {
  net::EmbeddedTestServer other_https_test_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  other_https_test_server.RegisterRequestHandler(
      base::BindRepeating(&FailTestIfPathRequested, "/qwac"));
  auto other_test_server_handle =
      other_https_test_server.StartAndReturnHandle();
  ASSERT_TRUE(other_test_server_handle.is_valid());

  ServePageWithQwacHeaderHandler main_page_handler(
      embedded_https_test_server(), "/main",
      other_https_test_server.GetURL("foo.example.com", "/qwac").spec());
  auto test_server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  base::HistogramTester histograms;
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("www.example.com", "/main")));

  // Since the initial QWAC url was cross-origin, the fetch is never started
  // and no QwacStatus should be created.
  EXPECT_FALSE(GetCurrentPageQwacStatus());
  EXPECT_FALSE(VisibleSecurityStateTwoQwac());
  EXPECT_EQ(1, main_page_handler.request_count());
  ExpectHistogramSample(histograms,
                        QwacWebContentsObserver::QwacLinkProcessingResult::
                            kNonrelativeQwacLinkUrl);
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       TestQwacLoadHttpError) {
  ServePageWithQwacHeaderHandler main_page_handler(embedded_https_test_server(),
                                                   "/main", "/qwac");
  ServeResponseHandler qwac_handler(embedded_https_test_server(), "/qwac",
                                    "404 content", "text/html",
                                    net::HTTP_NOT_FOUND);
  auto test_server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  base::HistogramTester histograms;
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("www.example.com", "/main")));

  auto* status = GetCurrentPageQwacStatus();
  ASSERT_TRUE(status);
  ASSERT_TRUE(WaitForStatusFinished(status));
  EXPECT_FALSE(status->verified_2qwac_cert());
  EXPECT_EQ(1, qwac_handler.request_count());
  EXPECT_FALSE(VisibleSecurityStateTwoQwac());
  ExpectHistogramSample(
      histograms,
      QwacWebContentsObserver::QwacLinkProcessingResult::kDownloadFailed);
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest, NotFetchedForHttp) {
  ServePageWithQwacHeaderHandler main_page_handler(*embedded_test_server(),
                                                   "/main", "/qwac");
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&FailTestIfPathRequested, "/qwac"));
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  base::HistogramTester histograms;
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL("www.example.com", "/main")));

  EXPECT_FALSE(GetCurrentPageQwacStatus());
  EXPECT_FALSE(VisibleSecurityStateTwoQwac());
  EXPECT_EQ(1, main_page_handler.request_count());
  ExpectNoHistogramSample(histograms);
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest, TestInvalidBinding) {
  ASSERT_TRUE(embedded_https_test_server().InitializeAndListen());

  net::TwoQwacCertBindingBuilder binding_builder;
  auto tls_leaf = embedded_https_test_server().GetCertificate();
  ASSERT_TRUE(tls_leaf);
  binding_builder.SetBoundCerts(
      {std::string(base::as_string_view(tls_leaf->cert_span()))});
  // Serve a 2-QWAC with an invalid signature.
  ServeResponseHandler qwac_handler(
      embedded_https_test_server(), "/qwac",
      binding_builder.GetJWSWithInvalidSignature(), "application/jose");

  ServePageWithQwacHeaderHandler main_page_handler(embedded_https_test_server(),
                                                   "/main", "/qwac");
  auto test_server_handle =
      embedded_https_test_server().StartAcceptingConnectionsAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  InstallCRSUpdate(
      /*root_cert=*/embedded_https_test_server().GetRoot().get(),
      /*eutl_cert=*/
      binding_builder.GetRootBuilder()->GetX509Certificate().get());

  base::HistogramTester histograms;
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("www.example.com", "/main")));

  auto* status = GetCurrentPageQwacStatus();
  ASSERT_TRUE(status);
  ASSERT_TRUE(WaitForStatusFinished(status));
  EXPECT_FALSE(status->verified_2qwac_cert());
  ASSERT_TRUE(status->tls_cert());
  EXPECT_TRUE(status->tls_cert()->EqualsExcludingChain(tls_leaf.get()));
  EXPECT_EQ(1, main_page_handler.request_count());
  EXPECT_EQ(1, qwac_handler.request_count());
  ExpectHistogramSample(histograms,
                        QwacWebContentsObserver::QwacLinkProcessingResult::
                            k2QwacVerificationFailed);
}

// TODO(crbug.com/436274241): Test that qwac is not fetched after clicking
// through HTTPS error.
// TODO(crbug.com/436300891): Test that qwac requests shows up in netlog?

}  // namespace

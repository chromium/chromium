// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/http/transport_security_state.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"

namespace {

// A test fixture that allows tests to wait for an Expect-CT report to be
// received by a server.
class ExpectCTBrowserTest : public CertVerifierBrowserTest {
 public:
  ExpectCTBrowserTest() : CertVerifierBrowserTest() {
    feature_list_.InitWithFeatures(
        {network::features::kExpectCTReporting,
         net::TransportSecurityState::kDynamicExpectCTFeature},
        {});

    // Expect-CT reporting depends on actually enforcing Certificate
    // Transparency.
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        true);
  }

  ~ExpectCTBrowserTest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        base::nullopt);
  }

  void SetUpOnMainThread() override {
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void TearDown() override { run_loop_.reset(nullptr); }

  std::unique_ptr<net::test_server::HttpResponse> ExpectCTHeaderRequestHandler(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    http_response->AddCustomHeader(
        "Expect-CT", "max-age=100, report-uri=" + report_uri_.spec());
    return http_response;
  }

  std::unique_ptr<net::test_server::HttpResponse> ReportRequestHandler(
      const net::test_server::HttpRequest& request) {
    EXPECT_TRUE(request.method == net::test_server::METHOD_POST ||
                request.method == net::test_server::METHOD_OPTIONS)
        << "Request method must be POST or OPTIONS. It is " << request.method
        << ".";
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);

    // Respond properly to CORS preflights.
    if (request.method == net::test_server::METHOD_OPTIONS) {
      http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
      http_response->AddCustomHeader("Access-Control-Allow-Methods", "POST");
      http_response->AddCustomHeader("Access-Control-Allow-Headers",
                                     "content-type");
    } else if (request.method == net::test_server::METHOD_POST) {
      auto it = request.headers.find("Content-Type");
      EXPECT_NE(it, request.headers.end());
      // The above EXPECT_NE is really an ASSERT_NE in spirit, but can't ASSERT
      // because a response must be returned.
      if (it != request.headers.end()) {
        EXPECT_EQ("application/expect-ct-report+json; charset=utf-8",
                  it->second);
      }
      run_loop_->Quit();
    }

    return http_response;
  }

  std::unique_ptr<net::test_server::HttpResponse> TestRequestHandler(
      const GURL& report_url,
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    std::string header_value = "report-uri=\"";
    header_value += report_url.spec() + "\", enforce, max-age=3600";
    http_response->AddCustomHeader("Expect-CT", header_value);
    return http_response;
  }

 protected:
  void WaitForReport() { run_loop_->Run(); }

  // Set the report-uri value to be used in the Expect-CT header for requests
  // handled by ExpectCTHeaderRequestHandler.
  void set_report_uri(const GURL& report_uri) { report_uri_ = report_uri; }

 private:
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<base::RunLoop> run_loop_;
  // The report-uri value to use in the Expect-CT header for requests handled by
  // ExpectCTHeaderRequestHandler.
  GURL report_uri_;

  DISALLOW_COPY_AND_ASSIGN(ExpectCTBrowserTest);
};

// Tests that an Expect-CT reporter is properly set up and used for violations
// of Expect-CT HTTP headers.
IN_PROC_BROWSER_TEST_F(ExpectCTBrowserTest, TestDynamicExpectCTReporting) {
  net::EmbeddedTestServer report_server;
  report_server.RegisterRequestHandler(base::Bind(
      &ExpectCTBrowserTest::ReportRequestHandler, base::Unretained(this)));
  ASSERT_TRUE(report_server.Start());
  GURL report_url = report_server.GetURL("/");

  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.RegisterRequestHandler(
      base::Bind(&ExpectCTBrowserTest::TestRequestHandler,
                 base::Unretained(this), report_url));
  ASSERT_TRUE(test_server.Start());

  // Set up the mock cert verifier to accept |test_server|'s certificate as
  // valid and as if it is issued by a known root. (CT checks are skipped for
  // private roots.)
  scoped_refptr<net::X509Certificate> cert(test_server.GetCertificate());
  net::CertVerifyResult verify_result;
  verify_result.is_issued_by_known_root = true;
  verify_result.verified_cert = cert;
  verify_result.cert_status = 0;
  mock_cert_verifier()->AddResultForCert(cert, verify_result, net::OK);

  // Fire off a request so that |test_server| sets a valid Expect-CT header.
  ui_test_utils::NavigateToURL(browser(), test_server.GetURL("/"));

  // Navigate to a test server URL, which should trigger an Expect-CT report
  // because the test server doesn't serve SCTs.
  ui_test_utils::NavigateToURL(browser(), test_server.GetURL("/"));
  WaitForReport();
  // WaitForReport() does not return util ReportRequestHandler runs, and the
  // handler does all the assertions, so there are no more assertions needed
  // here.
}

// Tests that Expect-CT HTTP headers are processed correctly.
IN_PROC_BROWSER_TEST_F(ExpectCTBrowserTest,
                       TestDynamicExpectCTHeaderProcessing) {
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.RegisterRequestHandler(
      base::Bind(&ExpectCTBrowserTest::ExpectCTHeaderRequestHandler,
                 base::Unretained(this)));
  ASSERT_TRUE(test_server.Start());

  net::EmbeddedTestServer report_server;
  report_server.RegisterRequestHandler(base::Bind(
      &ExpectCTBrowserTest::ReportRequestHandler, base::Unretained(this)));
  ASSERT_TRUE(report_server.Start());

  // Set up ExpectCTRequestHandler() to set Expect-CT headers that report to
  // |report_server|.
  set_report_uri(report_server.GetURL("/"));

  // Set up the mock cert verifier to accept |test_server|'s certificate as
  // valid and as if it is issued by a known root. (CT checks are skipped for
  // private roots.)
  scoped_refptr<net::X509Certificate> cert(test_server.GetCertificate());
  net::CertVerifyResult verify_result;
  verify_result.is_issued_by_known_root = true;
  verify_result.verified_cert = cert;
  verify_result.cert_status = 0;
  mock_cert_verifier()->AddResultForCert(cert, verify_result, net::OK);

  // Navigate to a test server URL, whose header should trigger an Expect-CT
  // report because the test server doesn't serve SCTs.
  ui_test_utils::NavigateToURL(browser(), test_server.GetURL("/"));
  WaitForReport();
  // WaitForReport() does not return util ReportRequestHandler runs, and the
  // handler does all the assertions, so there are no more assertions needed
  // here.
}

}  // namespace

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ssl/sct_reporting_service.h"
#include "chrome/browser/ssl/sct_reporting_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/network_service_test_helper.h"
#include "net/cert/cert_verify_result.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "services/network/test/test_url_loader_factory.h"

class SCTReportingServiceBrowserTest : public CertVerifierBrowserTest {
 public:
  SCTReportingServiceBrowserTest() {
    // Set sampling rate to 1.0 to ensure deterministic behavior.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kSCTAuditing,
          {{features::kSCTAuditingSamplingRate.name, "1.0"}}}},
        {});
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        true);
    // The report server must be initialized here so the reporting URL can be
    // set before the network service is initialized.
    ignore_result(report_server()->InitializeAndListen());
    SCTReportingService::GetReportURLInstance() = report_server()->GetURL("/");
  }
  ~SCTReportingServiceBrowserTest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        base::nullopt);
  }

  SCTReportingServiceBrowserTest(const SCTReportingServiceBrowserTest&) =
      delete;
  const SCTReportingServiceBrowserTest& operator=(
      const SCTReportingServiceBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    report_server()->RegisterRequestHandler(base::BindRepeating(
        &SCTReportingServiceBrowserTest::HandleReportRequest,
        base::Unretained(this)));
    report_server()->StartAcceptingConnections();
    ASSERT_TRUE(https_server()->Start());

    // Set up two test hosts as using publicly-issued certificates for testing.
    net::CertVerifyResult verify_result;
    verify_result.verified_cert = https_server_.GetCertificate().get();
    verify_result.is_issued_by_known_root = true;
    mock_cert_verifier()->AddResultForCertAndHost(
        https_server()->GetCertificate().get(), "a.test", verify_result,
        net::OK);
    mock_cert_verifier()->AddResultForCertAndHost(
        https_server()->GetCertificate().get(), "b.test", verify_result,
        net::OK);

    // Set up a third (internal) test host for FlushAndCheckZeroReports().
    mock_cert_verifier()->AddResultForCertAndHost(
        https_server()->GetCertificate().get(),
        "flush-and-check-zero-reports.test", verify_result, net::OK);

    CertVerifierBrowserTest::SetUpOnMainThread();
  }

 protected:
  void SetExtendedReportingEnabled(bool enabled) {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kSafeBrowsingScoutReportingEnabled, enabled);
  }
  void SetSafeBrowsingEnabled(bool enabled) {
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                                 enabled);
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }
  net::EmbeddedTestServer* report_server() { return &report_server_; }

  void WaitForRequests(size_t num_requests) {
    if (requests_seen_ >= num_requests)
      return;

    requests_expected_ = num_requests;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  size_t requests_seen() { return requests_seen_; }

  std::string GetLastSeenHostname() {
    if (!last_seen_request_.has_content)
      return std::string();
    sct_auditing::TLSConnectionReport auditing_report;
    auditing_report.ParseFromString(last_seen_request_.content);
    return auditing_report.context().origin().hostname();
  }

  // Checks that no reports have been sent. To do this, opt-in the profile,
  // make a new navigation, and check that there is only a single report and it
  // was for this new navigation specifically. This should be used at the end of
  // any negative tests to reduce the chance of false successes.
  bool FlushAndCheckZeroReports() {
    SetSafeBrowsingEnabled(true);
    SetExtendedReportingEnabled(true);
    ui_test_utils::NavigateToURL(
        browser(),
        https_server()->GetURL("flush-and-check-zero-reports.test", "/"));
    WaitForRequests(1);
    return (1u == requests_seen() &&
            "flush-and-check-zero-reports.test" == GetLastSeenHostname());
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleReportRequest(
      const net::test_server::HttpRequest& request) {
    last_seen_request_ = request;
    ++requests_seen_;
    if (!quit_closure_.is_null() && requests_seen_ >= requests_expected_) {
      std::move(quit_closure_).Run();
    }

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    return http_response;
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  net::EmbeddedTestServer report_server_;
  base::test::ScopedFeatureList scoped_feature_list_;

  net::test_server::HttpRequest last_seen_request_;
  size_t requests_seen_ = 0;
  size_t requests_expected_ = 0;
  base::OnceClosure quit_closure_;
};

// Tests that reports should not be sent when extended reporting is not opted
// in.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       NotOptedIn_ShouldNotEnqueueReport) {
  SetExtendedReportingEnabled(false);

  // Visit an HTTPS page.
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("a.test", "/"));

  // Check that no reports are sent.
  EXPECT_EQ(0u, requests_seen());
  EXPECT_TRUE(FlushAndCheckZeroReports());
}

// Tests that reports should be sent when extended reporting is opted in.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       OptedIn_ShouldEnqueueReport) {
  SetExtendedReportingEnabled(true);

  // Visit an HTTPS page and wait for the report to be sent.
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("a.test", "/"));
  WaitForRequests(1);

  // Check that one report was sent and contains the expected details.
  EXPECT_EQ(1u, requests_seen());
  EXPECT_EQ("a.test", GetLastSeenHostname());
}

// Tests that disabling Safe Browsing entirely should cause reports to not get
// sent.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest, DisableSafebrowsing) {
  SetSafeBrowsingEnabled(false);
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("a.test", "/"));
  EXPECT_EQ(0u, requests_seen());
  EXPECT_TRUE(FlushAndCheckZeroReports());
}

// Tests that we don't send a report for a navigation with a cert error.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       CertErrorDoesNotEnqueueReport) {
  SetExtendedReportingEnabled(true);

  // Visit a page with an invalid cert.
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("invalid.test", "/"));

  EXPECT_EQ(0u, requests_seen());
  EXPECT_TRUE(FlushAndCheckZeroReports());
}

// Tests that reports aren't sent for Incognito windows.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       IncognitoWindow_ShouldNotEnqueueReport) {
  // Enable SBER in the main profile.
  SetExtendedReportingEnabled(true);

  // Create a new Incognito window.
  auto* incognito = CreateIncognitoBrowser();

  ui_test_utils::NavigateToURL(incognito, https_server()->GetURL("/"));

  EXPECT_EQ(0u, requests_seen());
  EXPECT_TRUE(FlushAndCheckZeroReports());
}

// Tests that disabling Extended Reporting causes the cache to be cleared.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       OptingOutClearsSCTAuditingCache) {
  // Enable SCT auditing and enqueue a report.
  SetExtendedReportingEnabled(true);

  // Visit an HTTPS page and wait for a report to be sent.
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("a.test", "/"));
  WaitForRequests(1);

  // Check that one report was sent.
  EXPECT_EQ(1u, requests_seen());
  EXPECT_EQ("a.test", GetLastSeenHostname());

  // Disable Extended Reporting which should clear the underlying cache.
  SetExtendedReportingEnabled(false);

  // We can check that the same report gets cached again instead of being
  // deduplicated (i.e., another report should be sent).
  SetExtendedReportingEnabled(true);
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("a.test", "/"));
  WaitForRequests(2);
  EXPECT_EQ(2u, requests_seen());
  EXPECT_EQ("a.test", GetLastSeenHostname());
}

// Tests that reports are still sent for opted-in profiles after the network
// service crashes and is restarted.
// Disabled due to high flake rate; see https://crbug.com/1131803.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       DISABLED_ReportsSentAfterNetworkServiceRestart) {
  // This test is only applicable to out-of-process network service because it
  // tests what happens when the network service crashes and restarts.
  if (content::IsInProcessNetworkService()) {
    return;
  }

  SetExtendedReportingEnabled(true);

  // Crash the NetworkService to force it to restart.
  SimulateNetworkServiceCrash();

  // Visit an HTTPS page and wait for the report to be sent.
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("a.test", "/"));
  WaitForRequests(1);

  // Check that one report was enqueued.
  EXPECT_EQ(1u, requests_seen());
  EXPECT_EQ("a.test", GetLastSeenHostname());
}

// Tests that certificates that aren't issued from publicly known roots don't
// get reported.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       PrivateCertsNotReported) {
  // Set up a hostname that uses a non-publicly issued cert.
  net::CertVerifyResult verify_result;
  verify_result.verified_cert = https_server()->GetCertificate().get();
  verify_result.is_issued_by_known_root = false;
  mock_cert_verifier()->AddResultForCertAndHost(
      https_server()->GetCertificate().get(), "private.test", verify_result,
      net::OK);

  SetExtendedReportingEnabled(true);
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("private.test", "/"));

  EXPECT_EQ(0u, requests_seen());
  EXPECT_TRUE(FlushAndCheckZeroReports());
}

// TODO(crbug.com/1107975): Add test for "invalid SCTs should not get reported".
// This is blocked on https://crrev.com/c/1188845 to allow us to use the
// MockCertVerifier to mock CT results.

class SCTReportingServiceZeroSamplingRateBrowserTest
    : public SCTReportingServiceBrowserTest {
 public:
  SCTReportingServiceZeroSamplingRateBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kSCTAuditing,
          {{features::kSCTAuditingSamplingRate.name, "0.0"}}}},
        {});
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        true);
  }
  ~SCTReportingServiceZeroSamplingRateBrowserTest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        base::nullopt);
  }

  SCTReportingServiceZeroSamplingRateBrowserTest(
      const SCTReportingServiceZeroSamplingRateBrowserTest&) = delete;
  const SCTReportingServiceZeroSamplingRateBrowserTest& operator=(
      const SCTReportingServiceZeroSamplingRateBrowserTest&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the embedder is not notified when the sampling rate is zero.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceZeroSamplingRateBrowserTest,
                       EmbedderNotNotified) {
  SetExtendedReportingEnabled(true);

  // Visit an HTTPS page.
  ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));

  // Check that no reports are observed.
  EXPECT_EQ(0u, requests_seen());
}

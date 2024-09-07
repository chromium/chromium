// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/sct_reporting_service.h"

#include <memory>
#include <tuple>

#include "base/base64.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ssl/sct_reporting_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/network_service_test_helper.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_test_util.h"
#include "net/test/ct_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/simple_connection_listener.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/ct_log_info.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "services/network/test/test_url_loader_factory.h"

namespace {

// These LogId constants allow test cases to specify SCTs from both Google and
// non-Google logs, allowing tests to vary how they meet (or don't meet) the
// Chrome CT policy. To be compliant, the cert used by the embedded test server
// currently requires three embedded SCTs, including at least one from a Google
// log and one from a non-Google log.
//
// Google's "Argon2023" log ("6D7Q2j71BjUy51covIlryQPTy9ERa+zraeF3fW0GvW4="):
const uint8_t kTestGoogleLogId[] = {
    0xe8, 0x3e, 0xd0, 0xda, 0x3e, 0xf5, 0x06, 0x35, 0x32, 0xe7, 0x57,
    0x28, 0xbc, 0x89, 0x6b, 0xc9, 0x03, 0xd3, 0xcb, 0xd1, 0x11, 0x6b,
    0xec, 0xeb, 0x69, 0xe1, 0x77, 0x7d, 0x6d, 0x06, 0xbd, 0x6e};
// Cloudflare's "Nimbus2023" log
// ("ejKMVNi3LbYg6jjgUh7phBZwMhOFTTvSK8E6V6NS61I="):
const uint8_t kTestNonGoogleLogId1[] = {
    0x7a, 0x32, 0x8c, 0x54, 0xd8, 0xb7, 0x2d, 0xb6, 0x20, 0xea, 0x38,
    0xe0, 0x52, 0x1e, 0xe9, 0x84, 0x16, 0x70, 0x32, 0x13, 0x85, 0x4d,
    0x3b, 0xd2, 0x2b, 0xc1, 0x3a, 0x57, 0xa3, 0x52, 0xeb, 0x52};
// DigiCert's "Yeti2023" log ("Nc8ZG7+xbFe/D61MbULLu7YnICZR6j/hKu+oA8M71kw="):
const uint8_t kTestNonGoogleLogId2[] = {
    0x35, 0xcf, 0x19, 0x1b, 0xbf, 0xb1, 0x6c, 0x57, 0xbf, 0x0f, 0xad,
    0x4c, 0x6d, 0x42, 0xcb, 0xbb, 0xb6, 0x27, 0x20, 0x26, 0x51, 0xea,
    0x3f, 0xe1, 0x2a, 0xef, 0xa8, 0x03, 0xc3, 0x3b, 0xd6, 0x4c};

// Constructs a net::SignedCertificateTimestampAndStatus with the given
// information and appends it to |sct_list|.
void MakeTestSCTAndStatus(
    net::ct::SignedCertificateTimestamp::Origin origin,
    const std::string& extensions,
    const std::string& signature_data,
    const base::Time& timestamp,
    const std::string& log_id,
    net::ct::SCTVerifyStatus status,
    net::SignedCertificateTimestampAndStatusList* sct_list) {
  scoped_refptr<net::ct::SignedCertificateTimestamp> sct(
      new net::ct::SignedCertificateTimestamp());
  sct->version = net::ct::SignedCertificateTimestamp::V1;
  sct->log_id = log_id;
  sct->extensions = extensions;
  sct->timestamp = timestamp;
  sct->signature.signature_data = signature_data;
  sct->origin = origin;
  sct_list->push_back(net::SignedCertificateTimestampAndStatus(sct, status));
}

std::string ExtractRESTURLParameter(std::string url, std::string param) {
  size_t length_start = url.find(param) + param.size() + 1;
  size_t length_end = url.find('/', length_start);
  return url.substr(length_start, length_end - length_start);
}

std::string HexToString(const char* hex) {
  std::string result;
  bool ok = base::HexStringToString(hex, &result);
  DCHECK(ok);
  return result;
}

}  // namespace

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
    std::ignore = report_server()->InitializeAndListen();
    SCTReportingService::GetReportURLInstance() = report_server()->GetURL("/");
    SCTReportingService::GetHashdanceLookupQueryURLInstance() =
        report_server()->GetURL("/hashdance/length/$1/prefix/$2");
  }
  ~SCTReportingServiceBrowserTest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        std::nullopt);
  }

  SCTReportingServiceBrowserTest(const SCTReportingServiceBrowserTest&) =
      delete;
  const SCTReportingServiceBrowserTest& operator=(
      const SCTReportingServiceBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // ConnectionListener must be set before the report server is started. Lets
    // tests wait for one connection to be made to the report server (e.g. a
    // failed connection due to the cert error that won't trigger the
    // WaitForRequests() helper from the parent class).
    report_connection_listener_ =
        std::make_unique<net::test_server::SimpleConnectionListener>(
            1, net::test_server::SimpleConnectionListener::
                   ALLOW_ADDITIONAL_CONNECTIONS);
    report_server()->SetConnectionListener(report_connection_listener());

    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    report_server()->RegisterRequestHandler(base::BindRepeating(
        &SCTReportingServiceBrowserTest::HandleReportRequest,
        base::Unretained(this)));
    report_server()->StartAcceptingConnections();
    ASSERT_TRUE(https_server()->Start());

    mock_cert_verifier()->set_default_result(net::OK);

    // Mock the cert verify results so that it has valid CT verification
    // results.
    cert_with_precert_ = CreateCertificateChainFromFile(
        net::GetTestCertsDirectory(), "ct-test-embedded-cert.pem",
        net::X509Certificate::FORMAT_AUTO);
    ASSERT_TRUE(cert_with_precert_);
    ASSERT_EQ(1u, cert_with_precert_->intermediate_buffers().size());

    net::CertVerifyResult verify_result;
    verify_result.verified_cert = cert_with_precert_;
    verify_result.is_issued_by_known_root = true;
    // Add three "valid" SCTs and mark the certificate as compliant.
    // The default test set up is embedded SCTs where one SCT is from a Google
    // log and two are from non-Google logs (to meet the Chrome CT policy).
    MakeTestSCTAndStatus(
        net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions1",
        "signature1", base::Time::Now(),
        std::string(reinterpret_cast<const char*>(kTestGoogleLogId),
                    std::size(kTestGoogleLogId)),
        net::ct::SCT_STATUS_OK, &verify_result.scts);
    MakeTestSCTAndStatus(
        net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions2",
        "signature2", base::Time::Now(),
        std::string(reinterpret_cast<const char*>(kTestNonGoogleLogId1),
                    std::size(kTestNonGoogleLogId1)),
        net::ct::SCT_STATUS_OK, &verify_result.scts);
    MakeTestSCTAndStatus(
        net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions3",
        "signature3", base::Time::Now(),
        std::string(reinterpret_cast<const char*>(kTestNonGoogleLogId2),
                    std::size(kTestNonGoogleLogId2)),
        net::ct::SCT_STATUS_OK, &verify_result.scts);

    // Set up two test hosts as using publicly-issued certificates for testing.
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

    // Set up NetworkServiceTest once.
    content::GetNetworkService()->BindTestInterfaceForTesting(
        network_service_test_.BindNewPipeAndPassReceiver());

    // Override the retry delay to 0 so that retries happen immediately.
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    network_service_test_->SetSCTAuditingRetryDelay(base::TimeDelta());
  }

  void TearDownOnMainThread() override {
    // Reset the retry delay override.
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    network_service_test_->SetSCTAuditingRetryDelay(std::nullopt);

    CertVerifierBrowserTest::TearDownOnMainThread();
  }

  net::test_server::SimpleConnectionListener* report_connection_listener() {
    return report_connection_listener_.get();
  }

  mojo::Remote<network::mojom::NetworkServiceTest>& network_service_test() {
    return network_service_test_;
  }

 protected:
  void SetEnhancedProtectionEnabled(bool enabled) {
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced,
                                                 enabled);
  }
  void SetExtendedReportingEnabled(bool enabled) {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kSafeBrowsingScoutReportingEnabled, enabled);
  }
  void SetSafeBrowsingEnabled(bool enabled) {
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                                 enabled);
  }
  // |suffix_list| must be sorted lexicographically.
  void SetHashdanceSuffixList(std::vector<std::string> suffix_list) {
    suffix_list_ = std::move(suffix_list);
  }

  void SetExtendedReportingOrEnhancecProtectionEnabled(bool enabled) {
    if (base::FeatureList::IsEnabled(
            safe_browsing::kExtendedReportingRemovePrefDependency)) {
      // Currently, the SCT reporting functionality depends on the to-be
      // deprecated SBER (Extended Reporting) pref value,
      // "prefs::kSafeBrowsingScoutReportingEnabled", and the ESB (Enhanced Safe
      // Browsing) pref value, "prefs::kSafeBrowsingEnhanced".

      // After the dependency on "prefs::kSafeBrowsingScoutReportingEnabled" is
      // removed, SCT reporting should solely rely on the ESB pref
      // "prefs::kSafeBrowsingEnhanced". This test ensures that the SCT
      // functions as expected after this change.

      // Please refer to the IsExtendedReportingEnabled function in
      // safe_browsing_prefs.cc file and its original CL for details.
      SetEnhancedProtectionEnabled(true);
    } else {
      SetExtendedReportingEnabled(true);
    }
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }
  net::EmbeddedTestServer* report_server() { return &report_server_; }

  void WaitForRequests(size_t num_requests) {
    // Each loop iteration will account for one request being processed. (This
    // simplifies the request handler code below, and reduces the state that
    // must be tracked and handled under locks.)
    while (true) {
      base::RunLoop run_loop;
      {
        base::AutoLock auto_lock(requests_lock_);
        if (requests_seen_ >= num_requests)
          return;
        requests_closure_ = run_loop.QuitClosure();
      }
      run_loop.Run();
    }
  }

  size_t requests_seen() {
    base::AutoLock auto_lock(requests_lock_);
    return requests_seen_;
  }

  sct_auditing::SCTClientReport GetLastSeenReport() {
    base::AutoLock auto_lock(requests_lock_);
    sct_auditing::SCTClientReport auditing_report;
    if (last_seen_request_.has_content)
      auditing_report.ParseFromString(last_seen_request_.content);
    return auditing_report;
  }

  // Checks that no reports have been sent. To do this, opt-in the profile,
  // make a new navigation, and check that there is only a single report and it
  // was for this new navigation specifically. This should be used at the end of
  // any negative tests to reduce the chance of false successes.
  bool FlushAndCheckZeroReports(size_t requests_so_far = 0) {
    SetSafeBrowsingEnabled(true);
    if (base::FeatureList::IsEnabled(
            safe_browsing::kExtendedReportingRemovePrefDependency)) {
      // Currently, the SCT reporting functionality depends on the to-be
      // deprecated SBER (Extended Reporting) pref value,
      // "prefs::kSafeBrowsingScoutReportingEnabled", and the ESB (Enhanced Safe
      // Browsing) pref value, "prefs::kSafeBrowsingEnhanced".

      // After the dependency on "prefs::kSafeBrowsingScoutReportingEnabled" is
      // removed, SCT reporting should solely rely on the ESB pref
      // "prefs::kSafeBrowsingEnhanced". This test ensures that the SCT
      // functions as expected after this change.

      // Please refer to the IsExtendedReportingEnabled function in
      // safe_browsing_prefs.cc file and its original CL for details.
      SetEnhancedProtectionEnabled(true);
    } else {
      SetExtendedReportingEnabled(true);
    }
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        https_server()->GetURL("flush-and-check-zero-reports.test", "/")));
    WaitForRequests(1);
    return (requests_so_far + 1 == requests_seen() &&
            "flush-and-check-zero-reports.test" == GetLastSeenReport()
                                                       .certificate_report(0)
                                                       .context()
                                                       .origin()
                                                       .hostname());
  }

  void set_error_count(int error_count) { error_count_ = error_count; }

  const std::string& last_seen_length() { return last_seen_length_; }
  const std::string& last_seen_prefix() { return last_seen_prefix_; }
  const scoped_refptr<net::X509Certificate> certificate() {
    return cert_with_precert_;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleReportRequest(
      const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(requests_lock_);
    last_seen_request_ = request;
    ++requests_seen_;
    if (requests_closure_)
      std::move(requests_closure_).Run();

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();

    if (request.relative_url.find("hashdance") == std::string::npos) {
      // Request is a report.

      // Check if the server should just return an error for the full report
      // request, otherwise just return OK.
      if (error_count_ > 0) {
        http_response->set_code(net::HTTP_TOO_MANY_REQUESTS);
        --error_count_;
      } else {
        http_response->set_code(net::HTTP_OK);
      }
      return http_response;
    }

    // Request is a hashdance lookup query.
    // Parse the URL.
    DCHECK(!request.has_content);
    last_seen_length_ = ExtractRESTURLParameter(request.relative_url, "length");
    last_seen_prefix_ = ExtractRESTURLParameter(request.relative_url, "prefix");

    // Create a response.
    // 2022-01-01 00:00:00 GMT.
    base::Time server_time =
        base::Time::UnixEpoch() + base::Seconds(1640995200);
    base::Value::Dict response;
    response.Set("responseStatus", "OK");
    response.Set("now", base::TimeFormatAsIso8601(server_time));

    base::Value::List suffixes;
    for (const auto& suffix : suffix_list_) {
      suffixes.Append(
          base::Base64Encode(base::as_bytes(base::make_span(suffix))));
    }
    response.Set("hashSuffix", std::move(suffixes));

    base::Value::List log_list;
    {
      base::Value::Dict log_status;
      log_status.Set("logId", base::Base64Encode(kTestGoogleLogId));
      log_status.Set("ingestedUntil", base::TimeFormatAsIso8601(server_time));
      log_list.Append(std::move(log_status));
    }
    {
      base::Value::Dict log_status;
      log_status.Set("logId", base::Base64Encode(kTestNonGoogleLogId1));
      log_status.Set("ingestedUntil", base::TimeFormatAsIso8601(server_time));
      log_list.Append(std::move(log_status));
    }
    {
      base::Value::Dict log_status;
      log_status.Set("logId", base::Base64Encode(kTestNonGoogleLogId2));
      log_status.Set("ingestedUntil", base::TimeFormatAsIso8601(server_time));
      log_list.Append(std::move(log_status));
    }
    response.Set("logStatus", std::move(log_list));

    std::string json;
    bool ok = base::JSONWriter::Write(response, &json);
    DCHECK(ok);
    http_response->set_content(std::move(json));
    http_response->set_content_type("application/octet-stream");
    return http_response;
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  net::EmbeddedTestServer report_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::test::ScopedFeatureList scoped_feature_list_;

  scoped_refptr<net::X509Certificate> cert_with_precert_;
  std::unique_ptr<net::test_server::SimpleConnectionListener>
      report_connection_listener_;
  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test_;

  // `requests_lock_` is used to force sequential access to these variables to
  // avoid races that can cause test flakes.
  base::Lock requests_lock_;
  net::test_server::HttpRequest last_seen_request_;
  size_t requests_seen_ = 0;

  // Lookup query received parameters.
  std::string last_seen_length_;
  std::string last_seen_prefix_;

  // Lookup query settings.
  std::vector<std::string> suffix_list_;

  base::OnceClosure requests_closure_;

  // How many times the report server should return an error before succeeding,
  // specific to full report requests.
  size_t error_count_ = 0;
};

// Tests that reports should be sent when extended reporting is opted in.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       OptedIn_ShouldEnqueueReport) {
  SetExtendedReportingOrEnhancecProtectionEnabled(true);
  // Visit an HTTPS page and wait for the report to be sent.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("a.test", "/")));
  WaitForRequests(1);

  // Check that one report was sent and contains the expected details.
  EXPECT_EQ(1u, requests_seen());
  EXPECT_EQ(
      "a.test",
      GetLastSeenReport().certificate_report(0).context().origin().hostname());
}

// Tests that disabling Safe Browsing entirely should cause reports to not get
// sent.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest, DisableSafebrowsing) {
  SetSafeBrowsingEnabled(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("a.test", "/")));
  EXPECT_EQ(0u, requests_seen());
  EXPECT_TRUE(FlushAndCheckZeroReports());
}

// Tests that we don't send a report for a navigation with a cert error.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       CertErrorDoesNotEnqueueReport) {
  SetExtendedReportingOrEnhancecProtectionEnabled(true);

  // Visit a page with an invalid cert.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("invalid.test", "/")));

  EXPECT_EQ(0u, requests_seen());
  EXPECT_TRUE(FlushAndCheckZeroReports());
}

// Tests that reports aren't sent for Incognito windows.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       IncognitoWindow_ShouldNotEnqueueReport) {
  // Enable SBER in the main profile.
  SetExtendedReportingOrEnhancecProtectionEnabled(true);

  // Create a new Incognito window.
  auto* incognito = CreateIncognitoBrowser();

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(incognito, https_server()->GetURL("/")));

  EXPECT_EQ(0u, requests_seen());
  EXPECT_TRUE(FlushAndCheckZeroReports());
}

// Tests that disabling Extended Reporting causes the cache to be cleared.
// TODO(crbug.com/40749747): Reenable. Flakes heavily on all platforms.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       DISABLED_OptingOutClearsSCTAuditingCache) {
  // Enable SCT auditing and enqueue a report.
  SetExtendedReportingOrEnhancecProtectionEnabled(true);

  // Visit an HTTPS page and wait for a report to be sent.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("a.test", "/")));
  WaitForRequests(1);

  // Check that one report was sent.
  EXPECT_EQ(1u, requests_seen());
  EXPECT_EQ(
      "a.test",
      GetLastSeenReport().certificate_report(0).context().origin().hostname());

  // Disable Extended Reporting which should clear the underlying cache.
  SetExtendedReportingEnabled(false);

  // We can check that the same report gets cached again instead of being
  // deduplicated (i.e., another report should be sent).
  SetExtendedReportingOrEnhancecProtectionEnabled(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("a.test", "/")));
  WaitForRequests(2);
  EXPECT_EQ(2u, requests_seen());
  EXPECT_EQ(
      "a.test",
      GetLastSeenReport().certificate_report(0).context().origin().hostname());
}

// Tests that reports are still sent for opted-in profiles after the network
// service crashes and is restarted.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       ReportsSentAfterNetworkServiceRestart) {
  // This test is only applicable to out-of-process network service because it
  // tests what happens when the network service crashes and restarts.
  if (content::IsInProcessNetworkService()) {
    return;
  }

  SetExtendedReportingOrEnhancecProtectionEnabled(true);

  // Crash the NetworkService to force it to restart.
  SimulateNetworkServiceCrash();
  // Flush the network interface to make sure it notices the crash.
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();
  g_browser_process->system_network_context_manager()
      ->FlushNetworkInterfaceForTesting();

  // The mock cert verify result will be lost when the network service restarts,
  // so set back up the necessary rules.
  mock_cert_verifier()->set_default_result(net::OK);

  // The retry delay override will be reset when the network service restarts,
  // so set back up a retry delay of zero to avoid test timeouts.
  {
    network_service_test().reset();
    content::GetNetworkService()->BindTestInterfaceForTesting(
        network_service_test().BindNewPipeAndPassReceiver());

    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    network_service_test()->SetSCTAuditingRetryDelay(base::TimeDelta());
    // Default test fixture teardown will reset the delay back to the default.
  }

  net::CertVerifyResult verify_result;
  verify_result.verified_cert = https_server()->GetCertificate().get();
  verify_result.is_issued_by_known_root = true;
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions1",
      "signature1", base::Time::Now(),
      std::string(reinterpret_cast<const char*>(kTestGoogleLogId),
                  std::size(kTestGoogleLogId)),
      net::ct::SCT_STATUS_OK, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions2",
      "signature2", base::Time::Now(),
      std::string(reinterpret_cast<const char*>(kTestNonGoogleLogId1),
                  std::size(kTestNonGoogleLogId1)),
      net::ct::SCT_STATUS_OK, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions3",
      "signature3", base::Time::Now(),
      std::string(reinterpret_cast<const char*>(kTestNonGoogleLogId2),
                  std::size(kTestNonGoogleLogId2)),
      net::ct::SCT_STATUS_OK, &verify_result.scts);
  mock_cert_verifier()->AddResultForCertAndHost(
      https_server()->GetCertificate().get(), "a.test", verify_result, net::OK);

  // Visit an HTTPS page and wait for the report to be sent.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("a.test", "/")));
  WaitForRequests(1);

  // Check that one report was enqueued.
  EXPECT_EQ(1u, requests_seen());
  EXPECT_EQ(
      "a.test",
      GetLastSeenReport().certificate_report(0).context().origin().hostname());
}

// Tests that invalid SCTs don't get reported when the overall result is
// compliant with CT policy.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       CTCompliantInvalidSCTsNotReported) {
  // Set up a mocked CertVerifyResult that includes both valid and invalid SCTs.
  net::CertVerifyResult verify_result;
  verify_result.verified_cert = https_server()->GetCertificate().get();
  verify_result.is_issued_by_known_root = true;
  // Add three valid SCTs and one invalid SCT. The three valid SCTs meet the
  // Chrome CT policy.
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions1",
      "signature1", base::Time::Now(),
      std::string(reinterpret_cast<const char*>(kTestGoogleLogId),
                  sizeof(kTestGoogleLogId)),
      net::ct::SCT_STATUS_OK, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions2",
      "signature2", base::Time::Now(),
      std::string(reinterpret_cast<const char*>(kTestNonGoogleLogId1),
                  sizeof(kTestNonGoogleLogId1)),
      net::ct::SCT_STATUS_OK, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions3",
      "signature3", base::Time::Now(),
      std::string(reinterpret_cast<const char*>(kTestNonGoogleLogId2),
                  sizeof(kTestNonGoogleLogId2)),
      net::ct::SCT_STATUS_OK, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions4",
      "signature4", base::Time::Now(),
      std::string(reinterpret_cast<const char*>(kTestNonGoogleLogId2),
                  sizeof(kTestNonGoogleLogId2)),
      net::ct::SCT_STATUS_INVALID_SIGNATURE, &verify_result.scts);

  mock_cert_verifier()->AddResultForCertAndHost(
      https_server()->GetCertificate().get(), "mixed-scts.test", verify_result,
      net::OK);

  SetExtendedReportingOrEnhancecProtectionEnabled(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("mixed-scts.test", "/")));
  WaitForRequests(1);
  EXPECT_EQ(1u, requests_seen());

  auto report = GetLastSeenReport();
  EXPECT_EQ(3, report.certificate_report(0).included_sct_size());
}

// Tests that invalid SCTs don't get included when the overall result is
// non-compliant with CT policy. Valid SCTs should still be reported.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       CTNonCompliantInvalidSCTsNotReported) {
  // Set up a mocked CertVerifyResult that includes both valid and invalid SCTs.
  net::CertVerifyResult verify_result;
  verify_result.verified_cert = https_server()->GetCertificate().get();
  verify_result.is_issued_by_known_root = true;
  // Add one valid SCT and two invalid SCTs. These SCTs will not meet the Chrome
  // CT policy requirements.
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions1",
      "signature1", base::Time::Now(),
      std::string(reinterpret_cast<const char*>(kTestNonGoogleLogId1),
                  sizeof(kTestNonGoogleLogId1)),
      net::ct::SCT_STATUS_OK, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions2",
      "signature2", base::Time::Now(),
      std::string(reinterpret_cast<const char*>(kTestNonGoogleLogId1),
                  sizeof(kTestNonGoogleLogId1)),
      net::ct::SCT_STATUS_INVALID_SIGNATURE, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions3",
      "signature3", base::Time::Now(),
      std::string(reinterpret_cast<const char*>(kTestNonGoogleLogId2),
                  sizeof(kTestNonGoogleLogId2)),
      net::ct::SCT_STATUS_INVALID_SIGNATURE, &verify_result.scts);

  mock_cert_verifier()->AddResultForCertAndHost(
      https_server()->GetCertificate().get(), "mixed-scts.test", verify_result,
      net::OK);

  SetExtendedReportingOrEnhancecProtectionEnabled(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("mixed-scts.test", "/")));
  WaitForRequests(1);
  EXPECT_EQ(1u, requests_seen());

  auto report = GetLastSeenReport();
  EXPECT_EQ(1, report.certificate_report(0).included_sct_size());
}

IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest, NoValidSCTsNoReport) {
  // Set up a mocked CertVerifyResult with only invalid SCTs.
  net::CertVerifyResult verify_result;
  verify_result.verified_cert = https_server()->GetCertificate().get();
  verify_result.is_issued_by_known_root = true;
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions1",
      "signature1", base::Time::Now(),
      std::string(reinterpret_cast<const char*>(kTestNonGoogleLogId1),
                  sizeof(kTestNonGoogleLogId1)),
      net::ct::SCT_STATUS_INVALID_TIMESTAMP, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions2",
      "signature2", base::Time::Now(),
      std::string(reinterpret_cast<const char*>(kTestNonGoogleLogId1),
                  sizeof(kTestNonGoogleLogId1)),
      net::ct::SCT_STATUS_INVALID_SIGNATURE, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions3",
      "signature3", base::Time::Now(),
      std::string(reinterpret_cast<const char*>(kTestNonGoogleLogId1),
                  sizeof(kTestNonGoogleLogId1)),
      net::ct::SCT_STATUS_INVALID_SIGNATURE, &verify_result.scts);

  mock_cert_verifier()->AddResultForCertAndHost(
      https_server()->GetCertificate().get(), "invalid-scts.test",
      verify_result, net::OK);

  SetExtendedReportingOrEnhancecProtectionEnabled(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("invalid-scts.test", "/")));
  EXPECT_EQ(0u, requests_seen());
  EXPECT_TRUE(FlushAndCheckZeroReports());
}

class SCTReportingServiceZeroSamplingRateBrowserTest
    : public SCTReportingServiceBrowserTest {
 public:
  SCTReportingServiceZeroSamplingRateBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kSCTAuditing,
          {{features::kSCTAuditingSamplingRate.name, "0.0"}}}},
        {});
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
  SetExtendedReportingOrEnhancecProtectionEnabled(true);

  // Visit an HTTPS page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/")));

  // Check that no reports are observed.
  EXPECT_EQ(0u, requests_seen());
}

// Tests the simple case where a report succeeds on the first try.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest, SucceedOnFirstTry) {
  // Succeed on the first try.
  set_error_count(0);

  SetExtendedReportingOrEnhancecProtectionEnabled(true);

  // Visit an HTTPS page and wait for the report to be sent.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("a.test", "/")));
  WaitForRequests(1);

  // Check that one report was sent and contains the expected details.
  EXPECT_EQ(1u, requests_seen());
  EXPECT_EQ(
      "a.test",
      GetLastSeenReport().certificate_report(0).context().origin().hostname());
}

IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest, RetryOnceAndSucceed) {
  // Succeed on the second try.
  set_error_count(1);

  SetExtendedReportingOrEnhancecProtectionEnabled(true);

  // Visit an HTTPS page and wait for the report to be sent twice.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("a.test", "/")));
  WaitForRequests(2);

  // Check that the report was sent twice and contains the expected details.
  EXPECT_EQ(2u, requests_seen());
  EXPECT_EQ(
      "a.test",
      GetLastSeenReport().certificate_report(0).context().origin().hostname());
}

IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest, FailAfterMaxRetries) {
  // Don't succeed for max_retries+1.
  set_error_count(16);

  SetExtendedReportingOrEnhancecProtectionEnabled(true);

  // Visit an HTTPS page and wait for the report to be sent.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("a.test", "/")));

  // Wait until the reporter completes 16 requests.
  WaitForRequests(16);

  // Check that the report was sent 16x and contains the expected details.
  EXPECT_EQ(16u, requests_seen());
  EXPECT_EQ(
      "a.test",
      GetLastSeenReport().certificate_report(0).context().origin().hostname());
}

// Test that a cert error on the first attempt to send a report will trigger
// retries that succeed if the server starts using a good cert.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       CertificateErrorTriggersRetry) {
  {
    // Override the retry delay to 1s so that the retries don't all happen
    // immediately and the test can reset the default verifier result in
    // between retry attempts.
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    network_service_test()->SetSCTAuditingRetryDelay(base::Seconds(1));

    // Default test fixture teardown will reset the delay back to the default.
  }

  // The first request to the report server will trigger a certificate error via
  // the mock cert verifier.
  mock_cert_verifier()->set_default_result(net::ERR_CERT_COMMON_NAME_INVALID);

  SetExtendedReportingOrEnhancecProtectionEnabled(true);

  // Visit an HTTPS page, which will trigger a report being sent to the report
  // server but that report request will result in a cert error.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("a.test", "/")));

  report_connection_listener()->WaitForConnections();

  // After seeing one connection, replace the mock cert verifier result with a
  // successful result.
  mock_cert_verifier()->set_default_result(net::OK);

  WaitForRequests(1);

  // The second try should have resulted in the first successful report being
  // seen by the HandleRequest() handler.
  EXPECT_EQ(1u, requests_seen());
  EXPECT_EQ(
      "a.test",
      GetLastSeenReport().certificate_report(0).context().origin().hostname());
}

class SCTHashdanceBrowserTest : public SCTReportingServiceBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    SCTReportingServiceBrowserTest::SetUpOnMainThread();
    SetExtendedReportingEnabled(false);

    // Add a valid SCT that was issued at the beginning of time (and should
    // therefore be assumed to have been ingested by the server already). We
    // need to use a stable timestamp to get the same leaf hash in every run.
    // This SCT results in the leaf hash
    // 157F5BD43E660E1A87C45797CE524B4171A231CC10FE912A51A14ABA17EAB6B2.
    net::CertVerifyResult verify_result;
    verify_result.verified_cert = certificate();
    verify_result.is_issued_by_known_root = true;
    MakeTestSCTAndStatus(
        net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions1",
        "signature1", base::Time::UnixEpoch(),
        std::string(reinterpret_cast<const char*>(kTestGoogleLogId),
                    std::size(kTestGoogleLogId)),
        net::ct::SCT_STATUS_OK, &verify_result.scts);
    mock_cert_verifier()->AddResultForCertAndHost(
        https_server()->GetCertificate().get(), "hashdance.test", verify_result,
        net::OK);
    network::mojom::CTLogInfoPtr log(std::in_place);
    std::string googleLogIdAsString(
        reinterpret_cast<const char*>(kTestGoogleLogId),
        sizeof(kTestGoogleLogId));
    log->id = googleLogIdAsString;
    log->mmd = base::Seconds(86400);
    std::vector<network::mojom::CTLogInfoPtr> log_list;
    log_list.emplace_back(std::move(log));
    base::RunLoop run_loop;
    content::GetNetworkService()->UpdateCtLogList(std::move(log_list),
                                                  run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kSCTAuditingHashdance};
};

IN_PROC_BROWSER_TEST_F(SCTHashdanceBrowserTest, ReportSCTNotFound) {
  SetHashdanceSuffixList(
      {base::HexEncode(base::as_bytes(base::make_span(
           "000000000000000000000000000000000000000000000000000000000000"))),
       base::HexEncode(base::as_bytes(base::make_span(
           "0FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF")))});

  // Visit an HTTPS page and wait for the lookup query to be sent.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("hashdance.test", "/")));
  WaitForRequests(2);

  // Check that the lookup query was sent with the expected details.
  EXPECT_EQ(requests_seen(), 2u);
  EXPECT_EQ(last_seen_length(), "20");
  EXPECT_EQ(last_seen_prefix(), "157F50");
  EXPECT_EQ(
      "hashdance.test",
      GetLastSeenReport().certificate_report(0).context().origin().hostname());
}

IN_PROC_BROWSER_TEST_F(SCTHashdanceBrowserTest, DoNotReportSCTFound) {
  SetHashdanceSuffixList(
      {HexToString(
           "000000000000000000000000000000000000000000000000000000000000"),
       HexToString(
           "5BD43E660E1A87C45797CE524B4171A231CC10FE912A51A14ABA17EAB6B2"),
       HexToString(
           "0FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF")});

  // Visit an HTTPS page and wait for the lookup query to be sent.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("hashdance.test", "/")));
  WaitForRequests(1);

  // Check that the lookup query was sent with the expected details.
  EXPECT_EQ(requests_seen(), 1u);
  EXPECT_EQ(last_seen_length(), "20");
  EXPECT_EQ(last_seen_prefix(), "157F50");

  // No requests should have been sent.
  EXPECT_TRUE(FlushAndCheckZeroReports(/*requests_so_far=*/1));
}

IN_PROC_BROWSER_TEST_F(SCTHashdanceBrowserTest,
                       HashdanceReportCountIncremented) {
  base::HistogramTester histograms;

  // Visit an HTTPS page and wait for the full report to be sent.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("hashdance.test", "/")));
  WaitForRequests(2);

  // Check that two requests (lookup and full report) were sent and the report
  // contains the expected details.
  EXPECT_EQ(2u, requests_seen());
  EXPECT_EQ(
      "hashdance.test",
      GetLastSeenReport().certificate_report(0).context().origin().hostname());

  // Check that the report count got incremented.
  int report_count = g_browser_process->local_state()->GetInteger(
      prefs::kSCTAuditingHashdanceReportCount);
  EXPECT_EQ(report_count, 1);

  // The histogram is logged *before* the report count is incremented, so the
  // histogram will only log a report count of zero, once.
  histograms.ExpectUniqueSample("Security.SCTAuditing.OptOut.ReportCount", 0,
                                1);
}

// Test that report count isn't incremented when retrying a single audit report.
// Regression test for crbug.com/1348313.
IN_PROC_BROWSER_TEST_F(SCTHashdanceBrowserTest,
                       HashdanceReportCountNotIncrementedOnRetry) {
  base::HistogramTester histograms;

  // Don't succeed for max_retries+1, for the *full report sending*, but the
  // hashdance lookup query will always succeed.
  set_error_count(16);

  // Visit an HTTPS page and wait for the report to be sent.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("hashdance.test", "/")));

  // Wait until the reporter completes 32 requests (16 lookup queries which
  // succeed, and 16 full report requests which fail).
  WaitForRequests(32);

  // Check that 32 requests were seen and contains the expected details.
  EXPECT_EQ(32u, requests_seen());
  EXPECT_EQ(
      "hashdance.test",
      GetLastSeenReport().certificate_report(0).context().origin().hostname());

  // Check that the report was only counted once towards the max-reports limit.
  int report_count = g_browser_process->local_state()->GetInteger(
      prefs::kSCTAuditingHashdanceReportCount);
  EXPECT_EQ(report_count, 1);

  // Retrying sending the same report will only check the report count once the
  // first time, so the histogram will only log a report count of zero, once.
  histograms.ExpectUniqueSample("Security.SCTAuditing.OptOut.ReportCount", 0,
                                1);
}

IN_PROC_BROWSER_TEST_F(SCTHashdanceBrowserTest, HashdanceReportLimitReached) {
  base::HistogramTester histograms;

  // Override the report count to be the maximum.
  g_browser_process->local_state()->SetInteger(
      prefs::kSCTAuditingHashdanceReportCount, 3);

  // Visit an HTTPS page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("hashdance.test", "/")));

  // Check that no reports are sent.
  EXPECT_EQ(0u, requests_seen());
  SetSafeBrowsingEnabled(false);  // Clears the deduplication cache.
  EXPECT_TRUE(FlushAndCheckZeroReports());

  histograms.ExpectUniqueSample("Security.SCTAuditing.OptOut.ReportCount", 3,
                                1);
}

// Wrapper around FilePathWatcher to help tests wait for an auditing report to
// be persisted to disk. This is also robust to the persistence file being
// written to before the test initiates the wait, helping avoid race conditions
// that can cause hard-to-debug flakes.
//
// This currently monitors *two* file paths, because depending on the platform
// (and the state of the network service sandbox rollout) the persisted data
// file path may have an extra "Network/" subdirectory component, but it is
// difficult to determine this from test data. WaitUntilPersisted() will wait
// until *either* of the two paths have been written to.
//
// ReportPersistenceWaiter also takes a `filesize_threshold` as the "empty"
// persistence file still has some structure/data in it. For the current
// persistence format (list of JSON dicts), the "empty" persistence file is 2
// bytes (the empty list `[]`).
class ReportPersistenceWaiter {
 public:
  ReportPersistenceWaiter(const base::FilePath& watched_file_path,
                          const base::FilePath& alternative_file_path,
                          int64_t filesize_threshold)
      : watched_file_path1_(watched_file_path),
        watched_file_path2_(alternative_file_path),
        filesize_threshold_(filesize_threshold) {}
  ReportPersistenceWaiter(const ReportPersistenceWaiter&) = delete;
  ReportPersistenceWaiter& operator=(const ReportPersistenceWaiter&) = delete;

  void WaitUntilPersisted() {
    DCHECK(!watcher1_);
    DCHECK(!watcher2_);
    {
      // Check if either file was already written and if so return early.
      base::ScopedAllowBlockingForTesting allow_blocking;
      int64_t file_size;
      // GetFileSize() will return `false` if the file does not yet exist.
      if (base::GetFileSize(watched_file_path1_, &file_size) &&
          file_size > filesize_threshold_) {
        return;
      }
      if (base::GetFileSize(watched_file_path2_, &file_size) &&
          file_size > filesize_threshold_) {
        return;
      }
    }
    watcher1_ = std::make_unique<base::FilePathWatcher>();
    watcher2_ = std::make_unique<base::FilePathWatcher>();
    EXPECT_TRUE(watcher1_->Watch(
        watched_file_path1_, base::FilePathWatcher::Type::kNonRecursive,
        base::BindRepeating(&ReportPersistenceWaiter::OnPathChanged,
                            base::Unretained(this))));
    EXPECT_TRUE(watcher2_->Watch(
        watched_file_path2_, base::FilePathWatcher::Type::kNonRecursive,
        base::BindRepeating(&ReportPersistenceWaiter::OnPathChanged,
                            base::Unretained(this))));
    run_loop_.Run();
    // The watchers should be destroyed before quitting the run loop.
    DCHECK(!watcher1_);
    DCHECK(!watcher2_);
  }

 private:
  void OnPathChanged(const base::FilePath& path, bool error) {
    EXPECT_TRUE(path == watched_file_path1_ || path == watched_file_path2_);
    EXPECT_FALSE(error);
    watcher1_.reset();
    watcher2_.reset();
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  const base::FilePath watched_file_path1_;
  const base::FilePath watched_file_path2_;
  const int64_t filesize_threshold_;
  std::unique_ptr<base::FilePathWatcher> watcher1_;
  std::unique_ptr<base::FilePathWatcher> watcher2_;
};

IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       PersistedReportClearedOnClearBrowsingHistory) {
  // Set a long retry delay so that retries don't occur immediately.
  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    network_service_test()->SetSCTAuditingRetryDelay(base::Minutes(1));
  }
  // Don't immediately succeed, so report stays persisted to disk.
  set_error_count(10);

  SetExtendedReportingOrEnhancecProtectionEnabled(true);

  // The empty/cleared persistence file will be 2 bytes (the empty JSON list).
  constexpr int64_t kEmptyPersistenceFileSize = 2;

  base::FilePath persistence_path1 = browser()->profile()->GetPath();
  // If the network service sandbox is enabled, then the network service data
  // dir path has an additional "Network" subdirectory in it. This means that
  // different platforms will have different persistence paths depending on the
  // current state of the network service sandbox rollout.
  // TODO(crbug.com/41315406): Simplify this once the paths are consistent
  // (i.e., after the network service sandbox is fully rolled out.)
  base::FilePath persistence_path2 =
      persistence_path1.Append(chrome::kNetworkDataDirname);
  persistence_path1 =
      persistence_path1.Append(chrome::kSCTAuditingPendingReportsFileName);
  persistence_path2 =
      persistence_path2.Append(chrome::kSCTAuditingPendingReportsFileName);

  // Visit an HTTPS page to generate an SCT auditing report. Sending the report
  // will result in an error, so the pending report will remain.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("a.test", "/")));

  // Check that the report got persisted to disk.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ReportPersistenceWaiter waiter(persistence_path1, persistence_path2,
                                   kEmptyPersistenceFileSize);
    waiter.WaitUntilPersisted();
    int64_t file_size1;
    int64_t file_size2;
    bool one_file_is_written =
        base::GetFileSize(persistence_path1, &file_size1) ||
        base::GetFileSize(persistence_path2, &file_size2);
    EXPECT_TRUE(one_file_is_written);
    EXPECT_TRUE(file_size1 > kEmptyPersistenceFileSize ||
                file_size2 > kEmptyPersistenceFileSize);
  }

  // Trigger removal and wait for completion.
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::BrowsingDataRemover* remover =
      contents->GetBrowserContext()->GetBrowsingDataRemover();
  content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_CACHE,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      &completion_observer);
  completion_observer.BlockUntilCompletion();

  // Check that the persistence file is cleared.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    int64_t file_size;
    if (base::GetFileSize(persistence_path1, &file_size)) {
      EXPECT_EQ(file_size, kEmptyPersistenceFileSize);
    } else if (base::GetFileSize(persistence_path2, &file_size)) {
      EXPECT_EQ(file_size, kEmptyPersistenceFileSize);
    } else {
      FAIL() << "Neither persistence file was ever written";
    }
  }
}

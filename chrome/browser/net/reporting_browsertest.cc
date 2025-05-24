// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/base_switches.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/tpcd/enterprise_reporting/enterprise_reporting_tab_helper.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/reporting/reporting_policy.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {

const char kReportingHost[] = "example.test";
const char kCrossOriginHost[] = "cross-origin.test";

class BaseReportingBrowserTest : public CertVerifierBrowserTest,
                                 public ::testing::WithParamInterface<bool> {
 public:
  BaseReportingBrowserTest()
      : https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {
    std::vector<base::test::FeatureRef> required_features = {
        network::features::kReporting, network::features::kNetworkErrorLogging};
    scoped_feature_list_.InitWithFeatures(
        // enabled_features
        required_features,
        // disabled_features
        {});
  }

  BaseReportingBrowserTest(const BaseReportingBrowserTest&) = delete;
  BaseReportingBrowserTest& operator=(const BaseReportingBrowserTest&) = delete;

  ~BaseReportingBrowserTest() override = default;

  void SetUp() override;
  void SetUpOnMainThread() override;

  net::EmbeddedTestServer* server() { return &https_server_; }

  net::test_server::ControllableHttpResponse* upload_response() {
    return upload_response_.get();
  }

  GURL GetReportingEnabledURL() const {
    return https_server_.GetURL(kReportingHost, "/original");
  }

  GURL GetCollectorURL() const {
    return https_server_.GetURL(kReportingHost, "/upload");
  }

  std::string GetAppropriateReportingHeader() const {
    return base::EscapeQueryParamValue(UseDocumentReporting()
                                           ? GetReportingEndpointsHeader()
                                           : GetReportToHeader(),
                                       /*use_plus=*/false);
  }

  virtual std::string GetReportingEndpointsHeader() const {
    return "Reporting-Endpoints: default=\"" + GetCollectorURL().spec() + "\"";
  }

  std::string GetReportToHeader() const {
    return "Report-To: {\"endpoints\":[{\"url\":\"" + GetCollectorURL().spec() +
           "\"}],\"max_age\":86400}";
  }

  std::string GetNELHeader() const {
    return base::EscapeQueryParamValue(
        "NEL: "
        "{\"report_to\":\"default\",\"max_age\":86400,\"success_fraction\":1."
        "0}",
        /*use_plus=*/false);
  }

  std::string GetCSPHeader() const {
    return base::EscapeQueryParamValue(
        "Content-Security-Policy: script-src 'none'; report-to default",
        /*use_plus=*/false);
  }

 protected:
  bool UseDocumentReporting() const {
#if BUILDFLAG(ENABLE_REPORTING)
    return GetParam();
#else
    return false;
#endif
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> upload_response_;
};

void BaseReportingBrowserTest::SetUp() {
  CertVerifierBrowserTest::SetUp();

  // Make report delivery happen instantly.
  net::ReportingPolicy policy;
  policy.delivery_interval = base::Seconds(0);
  net::ReportingPolicy::UsePolicyForTesting(policy);
}

void BaseReportingBrowserTest::SetUpOnMainThread() {
  CertVerifierBrowserTest::SetUpOnMainThread();

  host_resolver()->AddRule("*", "127.0.0.1");

  upload_response_ =
      std::make_unique<net::test_server::ControllableHttpResponse>(server(),
                                                                   "/upload");

  // Reporting and NEL will ignore configurations headers if the response
  // doesn't come from an HTTPS origin, or if the origin's certificate is
  // invalid.  Our test certs are valid, so we need a mock certificate verifier
  // to trick the Reporting stack into paying attention to our test headers.
  mock_cert_verifier()->set_default_result(net::OK);
  server()->AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(server()->Start());
}

class ReportingBrowserTest : public BaseReportingBrowserTest {
 public:
  ReportingBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kPartitionConnectionsByNetworkIsolationKey);
  }

  ReportingBrowserTest(const ReportingBrowserTest&) = delete;
  ReportingBrowserTest& operator=(const ReportingBrowserTest&) = delete;

  ~ReportingBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class NonIsolatedReportingBrowserTest : public BaseReportingBrowserTest {
 public:
  NonIsolatedReportingBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        net::features::kPartitionConnectionsByNetworkIsolationKey);
  }

  NonIsolatedReportingBrowserTest(const NonIsolatedReportingBrowserTest&) =
      delete;
  NonIsolatedReportingBrowserTest& operator=(
      const NonIsolatedReportingBrowserTest&) = delete;

  ~NonIsolatedReportingBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This is a subclass of `BaseReportingBrowserTest` that specifically tests the
// `kCrashReportingAPIMoreContextData` feature, which adds more context data to
// each `CrashReportBody`. See https://crbug.com/400432195.
class ReportingBrowserTestMoreContextData : public BaseReportingBrowserTest {
 public:
  ReportingBrowserTestMoreContextData() = default;

  ReportingBrowserTestMoreContextData(
      const ReportingBrowserTestMoreContextData&) = delete;
  ReportingBrowserTestMoreContextData& operator=(
      const ReportingBrowserTestMoreContextData&) = delete;

  ~ReportingBrowserTestMoreContextData() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        blink::features::kCrashReportingAPIMoreContextData,
        /*enabled=*/GetParam());
    BaseReportingBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    BaseReportingBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ReportingBrowserTestSpecifyCrashEndpoint
    : public BaseReportingBrowserTest {
 public:
  ReportingBrowserTestSpecifyCrashEndpoint() {
    scoped_feature_list_.InitWithFeatureState(
        blink::features::kOverrideCrashReportingEndpoint,
        /*enabled=*/GetParam());
  }

  ReportingBrowserTestSpecifyCrashEndpoint(
      const ReportingBrowserTestSpecifyCrashEndpoint&) = delete;
  ReportingBrowserTestSpecifyCrashEndpoint& operator=(
      const ReportingBrowserTestSpecifyCrashEndpoint&) = delete;

  ~ReportingBrowserTestSpecifyCrashEndpoint() override = default;

  std::string GetReportingEndpointsHeader() const override {
    // Override the endpoint name of crash reporting.
    return "Reporting-Endpoints: crash-reporting=\"" +
           GetCollectorURL().spec() + "\"";
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class JSCallStackReportingBrowserTest : public BaseReportingBrowserTest {
 public:
  JSCallStackReportingBrowserTest() = default;

  JSCallStackReportingBrowserTest(const JSCallStackReportingBrowserTest&) =
      delete;
  JSCallStackReportingBrowserTest& operator=(
      const JSCallStackReportingBrowserTest&) = delete;

  ~JSCallStackReportingBrowserTest() override = default;

  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          blink::features::kDocumentPolicyIncludeJSCallStacksInCrashReports);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          blink::features::kDocumentPolicyIncludeJSCallStacksInCrashReports);
    }
    BaseReportingBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    BaseReportingBrowserTest::SetUpOnMainThread();
  }

  std::string GetDocumentPolicyHeader() const {
    return "Document-Policy: include-js-call-stacks-in-crash-reports";
  }

  void ExecuteInfiniteLoopScriptAsync(content::RenderFrameHost* frame) {
    content::ExecuteScriptAsync(frame, R"(
    function infiniteLoop() {
      let cnt = 0;
      while (true) {
        if (cnt++ == 0) {
          console.log('infiniteLoop');
        }
      }
    }
    infiniteLoop();
  )");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class EnterpriseReportingBrowserTest : public policy::PolicyTest {
 public:
  EnterpriseReportingBrowserTest()
      : https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitWithFeatures(
        // enabled_features
        {net::features::kForceThirdPartyCookieBlocking,
         net::features::kReportingApiEnableEnterpriseCookieIssues,
         network::features::kReporting},
        // disabled_features
        {});
  }

  ~EnterpriseReportingBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    PolicyTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void SetUp() override {
    PolicyTest::SetUp();

    // Making the report delivery happen instantly for testing.
    net::ReportingPolicy policy;
    policy.delivery_interval = base::Seconds(0);
    net::ReportingPolicy::UsePolicyForTesting(policy);
  }

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    preflight_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(server(),
                                                                     "/upload");
    payload_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(server(),
                                                                     "/upload");

    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    server()->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(server()->Start());
  }

  void UpdateReportingEndpointsPolicy(base::Value::Dict dict) {
    SetPolicy(&policies_, policy::key::kReportingEndpoints,
              base::Value(std::move(dict)));
    UpdateProviderPolicy(policies_);
  }

  net::EmbeddedTestServer* server() { return &https_server_; }

  net::test_server::ControllableHttpResponse* preflight_response() {
    return preflight_response_.get();
  }

  net::test_server::ControllableHttpResponse* payload_response() {
    return payload_response_.get();
  }

  GURL GetCollectorURL() const {
    return https_server_.GetURL(kReportingHost, "/upload");
  }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
  base::test::ScopedFeatureList scoped_feature_list_;
  policy::PolicyMap policies_;
  net::test_server::EmbeddedTestServer https_server_;
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      preflight_response_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> payload_response_;
};

class HistogramReportingBrowserTest : public BaseReportingBrowserTest {
 public:
  HistogramReportingBrowserTest() = default;

  HistogramReportingBrowserTest(const HistogramReportingBrowserTest&) = delete;
  HistogramReportingBrowserTest& operator=(
      const HistogramReportingBrowserTest&) = delete;

  ~HistogramReportingBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam()) {
      command_line->AppendSwitch(switches::kNoErrorDialogs);
    }
    BaseReportingBrowserTest::SetUpCommandLine(command_line);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

base::Value::List ParseReportUpload(const std::string& payload) {
  base::Value::List parsed_payload = base::test::ParseJsonList(payload);

  // Clear out any non-reproducible fields.
  for (auto& report_value : parsed_payload) {
    base::Value::Dict& report = report_value.GetDict();
    report.Remove("age");
    report.RemoveByDottedPath("body.elapsed_time");
    std::string* user_agent = report.FindString("user_agent");
    if (user_agent) {
      *user_agent = "Mozilla/1.0";
    }
  }
  return parsed_payload;
}

}  // namespace

// Tests that NEL reports are delivered correctly, whether or not reporting
// isolation is enabled. NEL reports can only be configured with the Report-To
// header, but this header should continue to function until support is
// completely removed.
IN_PROC_BROWSER_TEST_P(ReportingBrowserTest, TestNELHeadersProcessed) {
  GURL main_url = server()->GetURL(
      kReportingHost, base::StringPrintf("/set-header?%s&%s",
                                         GetReportToHeader(), GetNELHeader()));
  EXPECT_TRUE(NavigateToURL(
      browser()->tab_strip_model()->GetActiveWebContents(), main_url));

  upload_response()->WaitForRequest();
  base::Value::List actual =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 204 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  base::Value::List expected = base::test::ParseJsonList(base::StringPrintf(
      R"json(
        [
          {
            "body": {
              "protocol": "http/1.1",
              "referrer": "",
              "sampling_fraction": 1.0,
              "server_ip": "127.0.0.1",
              "method": "GET",
              "status_code": 200,
              "phase": "application",
              "type": "ok",
            },
            "type": "network-error",
            "url": "%s",
            "user_agent": "Mozilla/1.0",
          },
        ]
      )json",
      main_url.spec().c_str()));
  EXPECT_EQ(expected, actual);
}

// Tests that CSP reports are delivered properly whether configured with the
// v0 Report-To header or the v1 Reporting-Endpoints header.
IN_PROC_BROWSER_TEST_P(ReportingBrowserTest, TestReportingHeadersProcessed) {
  // Navigate to reporting-enabled page.
  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StringPrintf(
          "/set-header-with-file/chrome/test/data/simple_alert.html?%s&%s",
          GetAppropriateReportingHeader(), GetCSPHeader()));
  EXPECT_TRUE(NavigateToURL(
      browser()->tab_strip_model()->GetActiveWebContents(), main_url));

  upload_response()->WaitForRequest();
  base::Value::List actual =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 204 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  base::Value::List expected = base::test::ParseJsonList(base::StringPrintf(
      R"json(
        [ {
           "body": {
              "blockedURL": "inline",
              "disposition": "enforce",
              "documentURL": "%s",
              "effectiveDirective": "script-src-elem",
              "lineNumber": 2,
              "originalPolicy": "script-src 'none'; report-to default",
              "referrer": "",
              "sample": "",
              "sourceFile": "%s",
              "statusCode": 200
           },
           "type": "csp-violation",
           "url": "%s",
           "user_agent": "Mozilla/1.0"
        } ]
      )json",
      main_url.spec().c_str(),
      // Full document URL without the query parameters.
      main_url.spec().substr(0, main_url.spec().find('?')),
      main_url.spec().c_str()));
  EXPECT_EQ(expected, actual);
}

// Tests that CSP reports are delivered properly whether configured with the
// v0 Report-To header or the v1 Reporting-Endpoints header. This is a Non-
// isolated test, so will run with NIK-based report isolation disabled. This is
// a regression test for https://crbug.com/1258112.
IN_PROC_BROWSER_TEST_P(NonIsolatedReportingBrowserTest,
                       TestReportingHeadersProcessed) {
  // Navigate to reporting-enabled page.
  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StringPrintf(
          "/set-header-with-file/chrome/test/data/simple_alert.html?%s&%s",
          GetAppropriateReportingHeader(), GetCSPHeader()));
  EXPECT_TRUE(NavigateToURL(
      browser()->tab_strip_model()->GetActiveWebContents(), main_url));

  // Ensure that the correct endpoint was found, and that a report was sent.
  // (If the endpoint cannot not be found, then a report will be sent at all.)
  upload_response()->WaitForRequest();
  base::Value::List actual =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 204 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  base::Value::List expected = base::test::ParseJsonList(base::StringPrintf(
      R"json(
        [ {
           "body": {
              "blockedURL": "inline",
              "disposition": "enforce",
              "documentURL": "%s",
              "effectiveDirective": "script-src-elem",
              "lineNumber": 2,
              "originalPolicy": "script-src 'none'; report-to default",
              "referrer": "",
              "sample": "",
              "sourceFile": "%s",
              "statusCode": 200
           },
           "type": "csp-violation",
           "url": "%s",
           "user_agent": "Mozilla/1.0"
        } ]
      )json",
      main_url.spec().c_str(),
      // Full document URL without the query parameters.
      main_url.spec().substr(0, main_url.spec().find('?')),
      main_url.spec().c_str()));
  EXPECT_EQ(expected, actual);
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTest,
                       ReportingRespectsNetworkIsolationKeys) {
  // Favicon page is necessary since they are not served by default (i.e.,
  // `title1.html`), and in that case the default request for a favicon will
  // trigger a NEL report for the wrong reason.
  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StringPrintf("/set-header-with-file/chrome/test/data/favicon/"
                         "page_with_favicon.html?%s&%s",
                         GetReportToHeader(),
                         "NEL: {\"report_to\":\"default\", \"max_age\":86400, "
                         "\"failure_fraction\":1.0}"));
  EXPECT_TRUE(NavigateToURL(
      browser()->tab_strip_model()->GetActiveWebContents(), main_url));

  // Open a cross-origin kReportingHost iframe that fails to load. No report
  // should be uploaded, since the NetworkAnonymizationKey does not match.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), server()->GetURL("/iframe_blank.html")));
  content::NavigateIframeToURL(
      browser()->tab_strip_model()->GetActiveWebContents(), "test",
      server()->GetURL(kReportingHost, "/close-socket?should-not-be-reported"));

  // Navigate the main frame to a kReportingHost URL that fails to load. A
  // report should be uploaded, since the NetworkAnonymizationKey matches that
  // of the original request where reporting information was learned.
  GURL expect_reported_url =
      server()->GetURL(kReportingHost, "/close-socket?should-be-reported");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), expect_reported_url));
  upload_response()->WaitForRequest();
  base::Value::List actual =
      ParseReportUpload(upload_response()->http_request()->content);

  // Verify the contents of the received report.
  base::Value::List expected = base::test::ParseJsonList(base::StringPrintf(
      R"json(
        [
          {
            "body": {
              "protocol": "http/1.1",
              "referrer": "",
              "sampling_fraction": 1.0,
              "server_ip": "127.0.0.1",
              "method": "GET",
              "status_code": 0,
              "phase": "application",
              "type": "http.response.invalid.empty",
            },
            "type": "network-error",
            "url": "%s",
            "user_agent": "Mozilla/1.0",
          },
        ]
      )json",
      expect_reported_url.spec().c_str()));
  EXPECT_EQ(expected, actual);
}

// These tests intentionally crash a render process, and so fail ASan tests.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_CrashReport DISABLED_CrashReport
#define MAYBE_CrashReportUnresponsive DISABLED_CrashReportUnresponsive
#define MAYBE_CrashReportUnresponsiveCrossOriginIframe \
  DISABLED_CrashReportUnresponsiveCrossOriginIframe
#define MAYBE_MainPageOptedIn DISABLED_MainPageOptedIn
#define MAYBE_MainPageNotOptedIn DISABLED_MainPageNotOptedIn
#define MAYBE_IframeUnresponsiveWithJSCallStackOptedIn \
  DISABLED_IframeUnresponsiveWithJSCallStackOptedIn
#define MAYBE_IframeUnresponsiveWithJSCallStackNotOptedIn \
  DISABLED_IframeUnresponsiveWithJSCallStackNotOptedIn
#define MAYBE_SpecifyCrashEndpoint DISABLED_SpecifyCrashEndpoint
#else
#define MAYBE_CrashReport CrashReport
#define MAYBE_CrashReportUnresponsive CrashReportUnresponsive
#define MAYBE_CrashReportUnresponsiveCrossOriginIframe \
  CrashReportUnresponsiveCrossOriginIframe
#define MAYBE_MainPageOptedIn MainPageOptedIn
#define MAYBE_MainPageNotOptedIn MainPageNotOptedIn
#define MAYBE_IframeUnresponsiveWithJSCallStackOptedIn \
  IframeUnresponsiveWithJSCallStackOptedIn
#define MAYBE_IframeUnresponsiveWithJSCallStackNotOptedIn \
  IframeUnresponsiveWithJSCallStackNotOptedIn
#define MAYBE_SpecifyCrashEndpoint SpecifyCrashEndpoint
#endif  // defined(ADDRESS_SANITIZER)

IN_PROC_BROWSER_TEST_P(ReportingBrowserTest, MAYBE_CrashReport) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost, "/set-header?" + GetAppropriateReportingHeader());
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  // Simulate a crash on the page.
  content::RenderProcessHostWatcher crash_observer(
      contents, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  contents->GetController().LoadURL(GURL(blink::kChromeUICrashURL),
                                    content::Referrer(),
                                    ui::PAGE_TRANSITION_TYPED, std::string());
  crash_observer.Wait();

  upload_response()->WaitForRequest();
  base::Value::List response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  const base::Value::Dict& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");

  EXPECT_EQ("crash", *type);
  EXPECT_EQ(*url, main_url.spec());
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTest, MAYBE_CrashReportUnresponsive) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost, "/set-header?" + GetAppropriateReportingHeader());
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);

  content::SimulateUnresponsivePrimaryMainFrameAndWaitForExit(contents);

  upload_response()->WaitForRequest();
  base::Value::List response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  const base::Value::Dict& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::Value::Dict* body = report.FindDict("body");
  const std::string* reason = body->FindString("reason");

  EXPECT_EQ("crash", *type);
  EXPECT_EQ(*url, main_url.spec());
  EXPECT_EQ("unresponsive", *reason);
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTestMoreContextData,
                       CrashReportUnresponsive) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to reporting-enabled page.
  GURL main_url = server()->GetURL(
      kReportingHost, "/set-header?" + GetAppropriateReportingHeader());
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  // Simulate the page being killed due to being unresponsive.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_HUNG);

  upload_response()->WaitForRequest();
  base::Value::List request =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  const base::Value::Dict& report = request.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::Value::Dict* body = report.FindDict("body");
  const std::string* reason = body->FindString("reason");
  const std::optional<bool> is_top_level = body->FindBool("is_top_level");
  const std::string* visibility_state = body->FindString("visibility_state");

  EXPECT_EQ("crash", *type);
  EXPECT_EQ(*url, main_url);
  EXPECT_EQ("unresponsive", *reason);
  // When the `kCrashReportingAPIMoreContextData` flag is enabled, expect the
  // extra CrashReportBody context bits to be present.
  if (GetParam()) {
    EXPECT_TRUE(*is_top_level);
    EXPECT_EQ("visible", *visibility_state);
  } else {
    EXPECT_EQ(std::nullopt, is_top_level);
    EXPECT_EQ(nullptr, visibility_state);
  }
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTestMoreContextData,
                       CrashReportHiddenPage) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to reporting-enabled page.
  GURL main_url = server()->GetURL(
      kReportingHost, "/set-header?" + GetAppropriateReportingHeader());
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  // Hide the page.
  contents->WasHidden();
  EXPECT_EQ(contents->GetPrimaryMainFrame()->GetVisibilityState(),
            content::PageVisibilityState::kHidden);

  // Simulate the page being killed due to being unresponsive.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_HUNG);

  upload_response()->WaitForRequest();
  base::Value::List request =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  const base::Value::Dict& report = request.begin()->GetDict();
  const base::Value::Dict* body = report.FindDict("body");
  const std::string* visibility_state = body->FindString("visibility_state");

  // When the `kCrashReportingAPIMoreContextData` flag is enabled, expect the
  // extra CrashReportBody context bits to be present.
  if (GetParam()) {
    EXPECT_EQ(*visibility_state, "hidden");
  } else {
    EXPECT_EQ(visibility_state, nullptr);
  }
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTestMoreContextData,
                       MAYBE_CrashReportUnresponsiveCrossOriginIframe) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(NavigateToURL(
      contents, server()->GetURL(kCrossOriginHost, "/iframe.html")));

  GURL iframe_url(server()->GetURL(
      kReportingHost, "/set-header?" + GetAppropriateReportingHeader()));
  ASSERT_TRUE(NavigateIframeToURL(contents, "test", iframe_url));

  content::RenderFrameHost* subframe = ChildFrameAt(contents, 0);
  ASSERT_TRUE(subframe);
  content::SimulateUnresponsiveRenderer(contents,
                                        subframe->GetRenderWidgetHost());

  // Simulate the page being killed due to being unresponsive.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(
      subframe->GetProcess());
  subframe->GetProcess()->Shutdown(content::RESULT_CODE_HUNG);

  upload_response()->WaitForRequest();
  base::Value::List response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  const base::Value::Dict& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::Value::Dict* body = report.FindDict("body");
  const std::string* reason = body->FindString("reason");
  const std::optional<bool> is_top_level = body->FindBool("is_top_level");

  EXPECT_EQ("crash", *type);
  EXPECT_EQ(*url, iframe_url);
  EXPECT_EQ("unresponsive", *reason);
  if (GetParam()) {
    EXPECT_FALSE(*is_top_level);
  } else {
    EXPECT_EQ(std::nullopt, is_top_level);
  }
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTestSpecifyCrashEndpoint,
                       MAYBE_SpecifyCrashEndpoint) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost, "/set-header?" + GetAppropriateReportingHeader());
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  // Simulate a crash on the page.
  content::RenderProcessHostWatcher crash_observer(
      contents, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  contents->GetController().LoadURL(GURL(blink::kChromeUICrashURL),
                                    content::Referrer(),
                                    ui::PAGE_TRANSITION_TYPED, std::string());
  crash_observer.Wait();

  upload_response()->WaitForRequest();
  base::Value::List response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  const base::Value::Dict& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");

  EXPECT_EQ("crash", *type);
  EXPECT_EQ(*url, main_url.spec());
}

IN_PROC_BROWSER_TEST_P(JSCallStackReportingBrowserTest, MAYBE_MainPageOptedIn) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StringPrintf("/set-header?%s&%s", GetAppropriateReportingHeader(),
                         GetDocumentPolicyHeader()));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);
  content::WebContentsConsoleObserver console_observer(contents);
  console_observer.SetPattern("infiniteLoop");

  ExecuteInfiniteLoopScriptAsync(frame);

  ASSERT_TRUE(console_observer.Wait());
  content::SimulateUnresponsivePrimaryMainFrameAndWaitForExit(contents);

  upload_response()->WaitForRequest();
  base::Value::List response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  const base::Value::Dict& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::Value::Dict* body = report.FindDict("body");
  const std::string* reason = body->FindString("reason");
  const std::string* call_stack = body->FindString("stack");

  EXPECT_EQ("crash", *type);
  EXPECT_EQ(*url, main_url.spec());
  EXPECT_EQ("unresponsive", *reason);
  // TODO(crbug.com/407473725): Improve JS call stack collection test coverage.
  if (GetParam() && call_stack) {
    EXPECT_TRUE(call_stack->find("infiniteLoop") != std::string::npos);
  } else {
    EXPECT_EQ(nullptr, call_stack);
  }
}

IN_PROC_BROWSER_TEST_P(JSCallStackReportingBrowserTest,
                       MAYBE_MainPageNotOptedIn) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost, "/set-header?" + GetAppropriateReportingHeader());
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);
  content::WebContentsConsoleObserver console_observer(contents);
  console_observer.SetPattern("infiniteLoop");

  ExecuteInfiniteLoopScriptAsync(frame);

  ASSERT_TRUE(console_observer.Wait());

  content::SimulateUnresponsivePrimaryMainFrameAndWaitForExit(contents);

  upload_response()->WaitForRequest();
  base::Value::List response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  const base::Value::Dict& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::Value::Dict* body = report.FindDict("body");
  const std::string* reason = body->FindString("reason");
  const std::string* call_stack = body->FindString("stack");

  EXPECT_EQ("crash", *type);
  EXPECT_EQ(*url, main_url.spec());
  EXPECT_EQ("unresponsive", *reason);
  // TODO(crbug.com/407473725): Improve JS call stack collection test coverage.
  if (GetParam() && call_stack) {
    EXPECT_EQ(
        "Website owner has not opted in for JS call stacks in crash reports.",
        *call_stack);
  } else {
    EXPECT_EQ(nullptr, call_stack);
  }
}

IN_PROC_BROWSER_TEST_P(JSCallStackReportingBrowserTest,
                       MAYBE_IframeUnresponsiveWithJSCallStackOptedIn) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StringPrintf(
          "/set-header-with-file/chrome/test/data/iframe.html?%s&%s",
          GetAppropriateReportingHeader(), GetDocumentPolicyHeader()));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  GURL iframe_url(server()->GetURL(
      kReportingHost,
      base::StringPrintf("/set-header?%s&%s", GetAppropriateReportingHeader(),
                         GetDocumentPolicyHeader())));
  ASSERT_TRUE(NavigateIframeToURL(contents, "test", iframe_url));
  content::RenderFrameHost* subframe = ChildFrameAt(contents, 0);
  ASSERT_TRUE(subframe);

  content::WebContentsConsoleObserver console_observer(contents);
  console_observer.SetPattern("infiniteLoop");
  ExecuteInfiniteLoopScriptAsync(subframe);
  ASSERT_TRUE(console_observer.Wait());

  content::SimulateUnresponsivePrimaryMainFrameAndWaitForExit(contents);

  upload_response()->WaitForRequest();
  base::Value::List response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  const base::Value::Dict& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::Value::Dict* body = report.FindDict("body");
  const std::string* reason = body->FindString("reason");
  const std::string* call_stack = body->FindString("stack");

  EXPECT_EQ("crash", *type);
  EXPECT_EQ(*url, main_url.spec());
  EXPECT_EQ("unresponsive", *reason);
  // TODO(crbug.com/407473725): Improve JS call stack collection test coverage.
  if (GetParam() && call_stack) {
    EXPECT_EQ("Unable to collect JS call stack.", *call_stack);
  } else {
    EXPECT_EQ(nullptr, call_stack);
  }
}

IN_PROC_BROWSER_TEST_P(JSCallStackReportingBrowserTest,
                       MAYBE_IframeUnresponsiveWithJSCallStackNotOptedIn) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StringPrintf(
          "/set-header-with-file/chrome/test/data/iframe.html?%s&%s",
          GetAppropriateReportingHeader(), GetDocumentPolicyHeader()));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  GURL iframe_url(server()->GetURL(
      kReportingHost, "/set-header?" + GetAppropriateReportingHeader()));
  ASSERT_TRUE(NavigateIframeToURL(contents, "test", iframe_url));
  content::RenderFrameHost* subframe = ChildFrameAt(contents, 0);
  ASSERT_TRUE(subframe);

  content::WebContentsConsoleObserver console_observer(contents);
  console_observer.SetPattern("infiniteLoop");
  ExecuteInfiniteLoopScriptAsync(subframe);
  ASSERT_TRUE(console_observer.Wait());

  content::SimulateUnresponsivePrimaryMainFrameAndWaitForExit(contents);

  upload_response()->WaitForRequest();
  base::Value::List response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  const base::Value::Dict& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::Value::Dict* body = report.FindDict("body");
  const std::string* reason = body->FindString("reason");
  const std::string* call_stack = body->FindString("stack");

  EXPECT_EQ("crash", *type);
  EXPECT_EQ(*url, main_url.spec());
  EXPECT_EQ("unresponsive", *reason);
  // TODO(crbug.com/407473725): Improve JS call stack collection test coverage.
  if (GetParam() && call_stack) {
    EXPECT_EQ("Unable to collect JS call stack.", *call_stack);
  } else {
    EXPECT_EQ(nullptr, call_stack);
  }
}

// Tests that enterprise reports generated by a RenderFrameHost cookie error are
// properly delivered to an endpoint configured by the enterprise policy.
IN_PROC_BROWSER_TEST_F(EnterpriseReportingBrowserTest,
                       RenderFrameHostCookieError) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      net::features::kForceThirdPartyCookieBlocking));
  ASSERT_TRUE(base::FeatureList::IsEnabled(network::features::kReporting));
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      net::features::kReportingApiEnableEnterpriseCookieIssues));

  // Configure an enterprise policy endpoint for report delivery.
  UpdateReportingEndpointsPolicy(base::Value::Dict().Set(
      "enterprise-third-party-cookie-access-error", GetCollectorURL().spec()));

  // Generate and queue a report for delivery from a RenderFrameHost cookie
  // error
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url_a = server()->GetURL("a.test", "/iframe_blank.html");
  GURL url_b = server()->GetURL("b.test", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, url_a));
  ASSERT_TRUE(content::NavigateIframeToURL(web_contents, "test", url_b));
  ASSERT_TRUE(
      content::ExecJs(content::ChildFrameAt(web_contents, 0),
                      "document.cookie = 'foo=bar;SameSite=None;Secure'"));

  preflight_response()->WaitForRequest();
  preflight_response()->Send("HTTP/1.1 204 OK\r\n");
  preflight_response()->Send("Access-Control-Allow-Origin: *\r\n");
  preflight_response()->Send("Access-Control-Allow-Headers: *\r\n");
  preflight_response()->Send("\r\n");
  preflight_response()->Done();

  payload_response()->WaitForRequest();
  base::Value::List actualReport =
      ParseReportUpload(payload_response()->http_request()->content);
  payload_response()->Send("HTTP/1.1 204 OK\r\n");
  payload_response()->Send("\r\n");
  payload_response()->Done();

  base::Value::List expectedReport =
      base::test::ParseJsonList(base::StringPrintf(
          R"json(
          [
            {
              "body": {
                "frameUrl": "%s",
                "accessUrl": "%s",
                "name": "foo",
                "domain": "b.test",
                "path": "/",
                "accessOperation": "write"
              },
              "type": "enterprise-third-party-cookie-access-error",
              "url": "%s",
              "user_agent": "Mozilla/1.0"
            },
          ]
        )json",
          url_b.spec().c_str(), url_b.spec().c_str(), url_a.spec().c_str()));
  EXPECT_EQ(expectedReport, actualReport);
}

// Tests that enterprise reports generated by a NavigationHandle cookie error
// are properly delivered to an endpoint configured by the enterprise policy.
IN_PROC_BROWSER_TEST_F(EnterpriseReportingBrowserTest,
                       NavigationHandleCookieError) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      net::features::kForceThirdPartyCookieBlocking));
  ASSERT_TRUE(base::FeatureList::IsEnabled(network::features::kReporting));
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      net::features::kReportingApiEnableEnterpriseCookieIssues));

  // Configure an enterprise policy endpoint for report delivery.
  UpdateReportingEndpointsPolicy(base::Value::Dict().Set(
      "enterprise-third-party-cookie-access-error", GetCollectorURL().spec()));

  // Generate and queue a report for delivery from a NavigationHandle cookie
  // error
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url_a = server()->GetURL("a.test", "/iframe_blank.html");
  GURL url_b = server()->GetURL("b.test", "/title1.html");
  ASSERT_TRUE(content::SetCookie(web_contents->GetBrowserContext(), url_b,
                                 "foo=bar;SameSite=None;Secure"));
  ASSERT_TRUE(content::NavigateToURL(web_contents, url_a));
  ASSERT_TRUE(content::NavigateIframeToURL(web_contents, "test", url_b));

  preflight_response()->WaitForRequest();
  preflight_response()->Send("HTTP/1.1 204 OK\r\n");
  preflight_response()->Send("Access-Control-Allow-Origin: *\r\n");
  preflight_response()->Send("Access-Control-Allow-Headers: *\r\n");
  preflight_response()->Send("\r\n");
  preflight_response()->Done();

  payload_response()->WaitForRequest();
  base::Value::List actualReport =
      ParseReportUpload(payload_response()->http_request()->content);
  payload_response()->Send("HTTP/1.1 204 OK\r\n");
  payload_response()->Send("\r\n");
  payload_response()->Done();

  base::Value::List expectedReport =
      base::test::ParseJsonList(base::StringPrintf(
          R"json(
          [
            {
              "body": {
                "frameUrl": "%s",
                "accessUrl": "%s",
                "name": "foo",
                "domain": "b.test",
                "path": "/",
                "accessOperation": "read"
              },
              "type": "enterprise-third-party-cookie-access-error",
              "url": "%s",
              "user_agent": "Mozilla/1.0"
            },
          ]
        )json",
          url_b.spec().c_str(), url_b.spec().c_str(), url_a.spec().c_str()));
  EXPECT_EQ(expectedReport, actualReport);
}

IN_PROC_BROWSER_TEST_P(HistogramReportingBrowserTest,
                       CrashReportUnresponsiveHistogram) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::HistogramTester histogram_tester;

  GURL main_url = server()->GetURL(
      kReportingHost, "/set-header?" + GetAppropriateReportingHeader());
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);
  content::SimulateUnresponsiveRenderer(contents, frame->GetRenderWidgetHost());
  std::string_view histogram_name =
      "ReportingAndNEL.UnresponsiveRenderer.CrashReportOutcome";

  if (GetParam()) {
    histogram_tester.ExpectBucketCount(histogram_name, /*kDropped*/ 1,
                                       /*expected_count*/ 1);
  } else {
    histogram_tester.ExpectBucketCount(histogram_name,
                                       /*kPotentiallyQueued */ 0,
                                       /*expected_count*/ 1);
  }
}

INSTANTIATE_TEST_SUITE_P(All, ReportingBrowserTest, ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         NonIsolatedReportingBrowserTest,
                         ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         ReportingBrowserTestMoreContextData,
                         ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         ReportingBrowserTestSpecifyCrashEndpoint,
                         ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         JSCallStackReportingBrowserTest,
                         ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All, HistogramReportingBrowserTest, ::testing::Bool());

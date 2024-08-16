// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/base_switches.h"
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

const char kReportingHost[] = "example.com";

class BaseReportingBrowserTest : public CertVerifierBrowserTest,
                                 public ::testing::WithParamInterface<bool> {
 public:
  BaseReportingBrowserTest()
      : https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {
    std::vector<base::test::FeatureRef> required_features = {
        network::features::kReporting, network::features::kNetworkErrorLogging,
        features::kCrashReporting};
    if (UseDocumentReporting()) {
      required_features.push_back(net::features::kDocumentReporting);
    }
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

  net::test_server::ControllableHttpResponse* original_response() {
    return original_response_.get();
  }

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
    return UseDocumentReporting() ? GetReportingEndpointsHeader()
                                  : GetReportToHeader();
  }

  std::string GetReportingEndpointsHeader() const {
    return "Reporting-Endpoints: default=\"" + GetCollectorURL().spec() +
           "\"\r\n";
  }

  std::string GetReportToHeader() const {
    return "Report-To: {\"endpoints\":[{\"url\":\"" + GetCollectorURL().spec() +
           "\"}],\"max_age\":86400}\r\n";
  }

  std::string GetNELHeader() const {
    return "NEL: "
           "{\"report_to\":\"default\",\"max_age\":86400,\"success_fraction\":"
           "1.0}\r\n";
  }

  std::string GetCSPHeader() const {
    return "Content-Security-Policy: script-src 'none'; report-to default\r\n";
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
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      original_response_;
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

  original_response_ =
      std::make_unique<net::test_server::ControllableHttpResponse>(server(),
                                                                   "/original");
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
    iframe_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(server(),
                                                                     "/iframe");
    BaseReportingBrowserTest::SetUpOnMainThread();
  }

  net::test_server::ControllableHttpResponse* iframe_response() {
    return iframe_response_.get();
  }

  GURL GetIframeURL() { return server()->GetURL(kReportingHost, "/iframe"); }

  std::string GetDocumentPolicyHeader() const {
    return "Document-Policy: include-js-call-stacks-in-crash-reports\r\n";
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
  std::unique_ptr<net::test_server::ControllableHttpResponse> iframe_response_;
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
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 204 OK\r\n");
  original_response()->Send(GetReportToHeader());
  original_response()->Send(GetNELHeader());
  original_response()->Send("\r\n");
  original_response()->Done();

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
              "status_code": 204,
              "phase": "application",
              "type": "ok",
            },
            "type": "network-error",
            "url": "%s",
            "user_agent": "Mozilla/1.0",
          },
        ]
      )json",
      GetReportingEnabledURL().spec().c_str()));
  EXPECT_EQ(expected, actual);
}

// Tests that CSP reports are delivered properly whether configured with the
// v0 Report-To header or the v1 Reporting-Endpoints header.
IN_PROC_BROWSER_TEST_P(ReportingBrowserTest, TestReportingHeadersProcessed) {
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 200 OK\r\n");
  original_response()->Send("Content-Type: text/html\r\n");
  original_response()->Send(GetAppropriateReportingHeader());
  original_response()->Send(GetCSPHeader());
  original_response()->Send("\r\n");
  original_response()->Send("<script>alert(1);</script>\r\n");
  original_response()->Done();

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
              "lineNumber": 1,
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
      GetReportingEnabledURL().spec().c_str(),
      GetReportingEnabledURL().spec().c_str(),
      GetReportingEnabledURL().spec().c_str()));
  EXPECT_EQ(expected, actual);
}

// Tests that CSP reports are delivered properly whether configured with the
// v0 Report-To header or the v1 Reporting-Endpoints header. This is a Non-
// isolated test, so will run with NIK-based report isolation disabled. This is
// a regression test for https://crbug.com/1258112.
IN_PROC_BROWSER_TEST_P(NonIsolatedReportingBrowserTest,
                       TestReportingHeadersProcessed) {
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 200 OK\r\n");
  original_response()->Send("Content-Type: text/html\r\n");
  original_response()->Send(GetAppropriateReportingHeader());
  original_response()->Send(GetCSPHeader());
  original_response()->Send("\r\n");
  original_response()->Send("<script>alert(1);</script>\r\n");
  original_response()->Done();

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
              "lineNumber": 1,
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
      GetReportingEnabledURL().spec().c_str(),
      GetReportingEnabledURL().spec().c_str(),
      GetReportingEnabledURL().spec().c_str()));
  EXPECT_EQ(expected, actual);
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTest,
                       ReportingRespectsNetworkIsolationKeys) {
  // Navigate main frame to a kReportingHost URL and learn NEL and Reporting
  // information for that host.
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);
  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 204 OK\r\n");
  original_response()->Send(GetReportToHeader());
  original_response()->Send(
      "NEL: {"
      "  \"report_to\":\"default\","
      "  \"max_age\":86400,"
      "  \"failure_fraction\":1.0"
      "}\r\n\r\n");
  original_response()->Done();

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
#define MAYBE_MainPageOptedIn DISABLED_MainPageOptedIn
#define MAYBE_MainPageNotOptedIn DISABLED_MainPageNotOptedIn
#define MAYBE_IframeUnresponsiveWithJSCallStackOptedIn \
  DISABLED_IframeUnresponsiveWithJSCallStackOptedIn
#define MAYBE_IframeUnresponsiveWithJSCallStackNotOptedIn \
  DISABLED_IframeUnresponsiveWithJSCallStackNotOptedIn
#else
#define MAYBE_CrashReport CrashReport

// Flaky on Mac (multiple versions), see https://crbug.com/1261749
// Flaky on other platforms as well, see https://crbug.com/1377031
#define MAYBE_CrashReportUnresponsive DISABLED_CrashReportUnresponsive
#define MAYBE_MainPageOptedIn DISABLED_MainPageOptedIn
#define MAYBE_MainPageNotOptedIn DISABLED_MainPageNotOptedIn
#define MAYBE_IframeUnresponsiveWithJSCallStackOptedIn \
  DISABLED_IframeUnresponsiveWithJSCallStackOptedIn
#define MAYBE_IframeUnresponsiveWithJSCallStackNotOptedIn \
  DISABLED_IframeUnresponsiveWithJSCallStackNotOptedIn
#endif  // defined(ADDRESS_SANITIZER)

IN_PROC_BROWSER_TEST_P(ReportingBrowserTest, MAYBE_CrashReport) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver navigation_observer(contents);

  // Navigate to reporting-enabled page.
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 200 OK\r\n");
  original_response()->Send(GetAppropriateReportingHeader());
  original_response()->Send("\r\n");
  original_response()->Done();
  navigation_observer.Wait();

  // Simulate a crash on the page.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  contents->GetController().LoadURL(GURL(blink::kChromeUICrashURL),
                                    content::Referrer(),
                                    ui::PAGE_TRANSITION_TYPED, std::string());

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
  EXPECT_EQ(GetReportingEnabledURL().spec(), *url);
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTest, MAYBE_CrashReportUnresponsive) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver navigation_observer(contents);

  // Navigate to reporting-enabled page.
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 200 OK\r\n");
  original_response()->Send(GetAppropriateReportingHeader());
  original_response()->Send("\r\n");
  original_response()->Done();
  navigation_observer.Wait();

  // Simulate the page being killed due to being unresponsive.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_HUNG);

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
  EXPECT_EQ(GetReportingEnabledURL().spec(), *url);
  EXPECT_EQ("unresponsive", *reason);
}

IN_PROC_BROWSER_TEST_P(JSCallStackReportingBrowserTest, MAYBE_MainPageOptedIn) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver navigation_observer(contents);

  // Navigate to reporting-enabled page.
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 200 OK\r\n");
  original_response()->Send(GetAppropriateReportingHeader());
  original_response()->Send(GetDocumentPolicyHeader());
  original_response()->Send("\r\n");
  original_response()->Done();
  navigation_observer.Wait();

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);
  content::WebContentsConsoleObserver console_observer(contents);
  console_observer.SetPattern("infiniteLoop");

  ExecuteInfiniteLoopScriptAsync(frame);

  ASSERT_TRUE(console_observer.Wait());

  content::SimulateUnresponsiveRenderer(contents, frame->GetRenderWidgetHost());

  // Simulate the page being killed due to being unresponsive.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_HUNG);

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
  EXPECT_EQ(GetReportingEnabledURL().spec(), *url);
  EXPECT_EQ("unresponsive", *reason);
  if (GetParam()) {
    EXPECT_TRUE(call_stack->find("infiniteLoop") != std::string::npos);
  } else {
    EXPECT_EQ(nullptr, call_stack);
  }
}

IN_PROC_BROWSER_TEST_P(JSCallStackReportingBrowserTest,
                       MAYBE_MainPageNotOptedIn) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver navigation_observer(contents);

  // Navigate to reporting-enabled page.
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 200 OK\r\n");
  original_response()->Send(GetAppropriateReportingHeader());
  original_response()->Send("\r\n");
  original_response()->Done();
  navigation_observer.Wait();

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);
  content::WebContentsConsoleObserver console_observer(contents);
  console_observer.SetPattern("infiniteLoop");

  ExecuteInfiniteLoopScriptAsync(frame);

  ASSERT_TRUE(console_observer.Wait());

  content::SimulateUnresponsiveRenderer(contents, frame->GetRenderWidgetHost());

  // Simulate the page being killed due to being unresponsive.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_HUNG);

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
  EXPECT_EQ(GetReportingEnabledURL().spec(), *url);
  EXPECT_EQ("unresponsive", *reason);
  if (GetParam()) {
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
  content::TestNavigationObserver navigation_observer(contents);

  // Navigate to reporting-enabled page.
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 200 OK\r\n");
  original_response()->Send(GetAppropriateReportingHeader());
  original_response()->Send(GetDocumentPolicyHeader());
  original_response()->Send("\r\n");
  original_response()->Done();
  navigation_observer.Wait();

  std::string script =
      "let iframe = document.createElement('iframe');"
      "iframe.src = '" +
      GetIframeURL().spec() + "'; document.body.appendChild(iframe);";
  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);
  ExecuteScriptAsync(frame, script);

  iframe_response()->WaitForRequest();
  iframe_response()->Send("HTTP/1.1 200 OK\r\n");
  iframe_response()->Send(GetAppropriateReportingHeader());
  iframe_response()->Send(GetDocumentPolicyHeader());
  iframe_response()->Send("\r\n");
  iframe_response()->Done();
  content::WaitForLoadStop(contents);

  content::RenderFrameHost* subframe = ChildFrameAt(contents, 0);
  ASSERT_TRUE(subframe);
  content::WebContentsConsoleObserver console_observer(contents);
  console_observer.SetPattern("infiniteLoop");

  ExecuteInfiniteLoopScriptAsync(subframe);

  ASSERT_TRUE(console_observer.Wait());

  content::SimulateUnresponsiveRenderer(contents,
                                        subframe->GetRenderWidgetHost());

  // Simulate the page being killed due to being unresponsive.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_HUNG);

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
  EXPECT_EQ(GetReportingEnabledURL().spec(), *url);
  EXPECT_EQ("unresponsive", *reason);
  if (GetParam()) {
    EXPECT_EQ("Unable to collect JS call stack.", *call_stack);
  } else {
    EXPECT_EQ(nullptr, call_stack);
  }
}

IN_PROC_BROWSER_TEST_P(JSCallStackReportingBrowserTest,
                       MAYBE_IframeUnresponsiveWithJSCallStackNotOptedIn) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver navigation_observer(contents);

  // Navigate to reporting-enabled page.
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 200 OK\r\n");
  original_response()->Send(GetAppropriateReportingHeader());
  original_response()->Send(GetDocumentPolicyHeader());
  original_response()->Send("\r\n");
  original_response()->Done();
  navigation_observer.Wait();

  std::string script =
      "let iframe = document.createElement('iframe');"
      "iframe.src = '" +
      GetIframeURL().spec() + "'; document.body.appendChild(iframe);";
  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);
  content::ExecuteScriptAsync(frame, script);

  iframe_response()->WaitForRequest();
  iframe_response()->Send("HTTP/1.1 200 OK\r\n");
  iframe_response()->Send(GetAppropriateReportingHeader());
  iframe_response()->Send("\r\n");
  iframe_response()->Done();
  content::WaitForLoadStop(contents);

  content::RenderFrameHost* subframe = ChildFrameAt(contents, 0);
  ASSERT_TRUE(subframe);
  content::WebContentsConsoleObserver console_observer(contents);
  console_observer.SetPattern("infiniteLoop");

  ExecuteInfiniteLoopScriptAsync(subframe);

  ASSERT_TRUE(console_observer.Wait());

  content::SimulateUnresponsiveRenderer(contents,
                                        subframe->GetRenderWidgetHost());

  // Simulate the page being killed due to being unresponsive.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_HUNG);

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
  EXPECT_EQ(GetReportingEnabledURL().spec(), *url);
  EXPECT_EQ("unresponsive", *reason);
  if (GetParam()) {
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

  // Navigate to reporting-enabled page.
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 200 OK\r\n");
  original_response()->Send(GetAppropriateReportingHeader());
  original_response()->Send("\r\n");
  original_response()->Done();

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
                         JSCallStackReportingBrowserTest,
                         ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All, HistogramReportingBrowserTest, ::testing::Bool());

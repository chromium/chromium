// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/base_switches.h"
#include "base/check_op.h"
#include "base/process/kill.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/task_manager/task_manager_tester.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/reporting/reporting_policy.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "sandbox/policy/switches.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

const char kReportingHost[] = "example.test";
const char kCrossOriginHost[] = "cross-origin.test";

base::ListValue ParseReportUpload(const std::string& payload) {
  base::ListValue parsed_payload = base::test::ParseJsonList(payload);

  // Clear out any non-reproducible fields.
  for (auto& report_value : parsed_payload) {
    base::DictValue& report = report_value.GetDict();
    report.Remove("age");
    report.RemoveByDottedPath("body.elapsed_time");
    if (report.FindString("user_agent")) {
      report.Set("user_agent", "Mozilla/1.0");
    }
  }
  return parsed_payload;
}

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
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  net::EmbeddedTestServer* server() { return &https_server_; }

  net::test_server::ControllableHttpResponse* upload_response() {
    return upload_response_.get();
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
    return base::StrCat(
        {"Reporting-Endpoints: default=\"", GetCollectorURL().spec(), "\""});
  }

  std::string GetReportToHeader() const {
    return base::StrCat({"Report-To: {\"endpoints\":[{\"url\":\"",
                         GetCollectorURL().spec(), "\"}],\"max_age\":86400}"});
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

  base::ListValue WaitForReportUploads(size_t expected_report_count) {
    base::ListValue all_reports;
    net::test_server::ControllableHttpResponse* responses[] = {
        upload_response_.get(), upload_response2_.get()};
    CHECK_LE(expected_report_count, std::size(responses));
    for (auto* response : responses) {
      if (all_reports.size() >= expected_report_count) {
        break;
      }
      response->WaitForRequest();
      base::ListValue batch =
          ParseReportUpload(response->http_request()->content);
      response->Send("HTTP/1.1 200 OK\r\n");
      response->Send("\r\n");
      response->Done();
      for (auto& item : batch) {
        all_reports.Append(std::move(item));
      }
    }
    return all_reports;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> upload_response_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> upload_response2_;
};

void BaseReportingBrowserTest::SetUp() {
  CertVerifierBrowserTest::SetUp();

  // Make report delivery happen instantly.
  net::ReportingPolicy policy;
  policy.delivery_interval = base::Seconds(0);
  net::ReportingPolicy::UsePolicyForTesting(policy);
}

void BaseReportingBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  CertVerifierBrowserTest::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_LINUX)
  // Disable the sandbox so that renderers do not run as PID 1 inside a Linux
  // PID namespace (where the kernel marks the process with SIGNAL_UNKILLABLE)
  // and without seccomp-bpf restrictions during crash handling.
  //
  // When simulating an in-renderer crash (e.g. via OOM_CRASH(0) /
  // PA_IMMEDIATE_CRASH(), which executes `int3` followed by `ud2`), Crashpad
  // catches `SIGTRAP` (`int3`), records the dump, restores `SIG_DFL`, and
  // re-raises `SIGTRAP` via `raise(SIGTRAP)` in
  // `Signals::RestoreHandlerAndReraiseSignalOnReturn()`. When the renderer is
  // PID 1 of its PID namespace, `SIGNAL_UNKILLABLE` causes the kernel to
  // ignore `raise(SIGTRAP)` with `SIG_DFL`, allowing execution to continue to
  // `ud2` (`SIGILL`). `SIGILL` then enters `StackDumpSignalHandler`, which
  // calls `alarm()` (triggering a seccomp-bpf `SIGSYS` violation) and fails
  // to terminate the `SIGNAL_UNKILLABLE` process after restoring `SIG_DFL`,
  // causing `RenderProcessHostWatcher::Wait()` to time out.
  command_line->AppendSwitch(sandbox::policy::switches::kNoSandbox);
#elif BUILDFLAG(IS_WIN)
  // Disable in-process stack traces in child processes on Windows. In
  // non-official test builds, `ContentMainRunnerImpl::Initialize()` calls
  // `base::debug::EnableInProcessStackDumping()` in child processes unless
  // `kDisableInProcessStackTraces` is set, installing
  // `StackDumpExceptionFilter` with `SYMOPT_DEFERRED_LOADS`. When simulating
  // an in-renderer crash (e.g. via `chrome://crash` or `OOM_CRASH(0)`),
  // `StackDumpExceptionFilter` invokes DbgHelp / DIA (`dbghelp.dll` /
  // `msdia140.dll`) inside the sandboxed renderer process to lazily load and
  // parse `browser_tests.exe.pdb` and symbolize the stack trace before the
  // process can terminate, which can cause `RenderProcessHostWatcher::Wait()`
  // to time out under load.
  command_line->AppendSwitch(switches::kDisableInProcessStackTraces);
#endif
}

void BaseReportingBrowserTest::SetUpOnMainThread() {
  CertVerifierBrowserTest::SetUpOnMainThread();

  host_resolver()->AddRule("*", "127.0.0.1");

  upload_response_ =
      std::make_unique<net::test_server::ControllableHttpResponse>(server(),
                                                                   "/upload");
  upload_response2_ =
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

class CrashReportingBrowserTest : public ReportingBrowserTest {
 public:
  CrashReportingBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&CrashReportingBrowserTest::web_contents,
                                base::Unretained(this))) {}

  CrashReportingBrowserTest(const CrashReportingBrowserTest&) = delete;
  CrashReportingBrowserTest& operator=(const CrashReportingBrowserTest&) =
      delete;

  ~CrashReportingBrowserTest() override = default;

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
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
  ReportingBrowserTestMoreContextData() {
    scoped_feature_list_.InitWithFeatureState(
        blink::features::kCrashReportingAPIMoreContextData,
        /*enabled=*/GetParam());
  }

  ReportingBrowserTestMoreContextData(
      const ReportingBrowserTestMoreContextData&) = delete;
  ReportingBrowserTestMoreContextData& operator=(
      const ReportingBrowserTestMoreContextData&) = delete;

  ~ReportingBrowserTestMoreContextData() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This is a subclass of `BaseReportingBrowserTest` that specifically tests the
// `kCrashReportingStorageAPI` feature, which adds a new API to attach string
// data to crash reports.
class ReportingBrowserTestCrashReportingStorage
    : public BaseReportingBrowserTest {
 public:
  ReportingBrowserTestCrashReportingStorage() = default;

  ReportingBrowserTestCrashReportingStorage(
      const ReportingBrowserTestCrashReportingStorage&) = delete;
  ReportingBrowserTestCrashReportingStorage& operator=(
      const ReportingBrowserTestCrashReportingStorage&) = delete;

  ~ReportingBrowserTestCrashReportingStorage() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kCrashReportingStorageAPI};
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
    return base::StrCat({"Reporting-Endpoints: crash-reporting=\"",
                         GetCollectorURL().spec(), "\""});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class JSCallStackReportingBrowserTest : public BaseReportingBrowserTest {
 public:
  JSCallStackReportingBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        blink::features::kDocumentPolicyIncludeJSCallStacksInCrashReports,
        /*enabled=*/GetParam());
  }

  JSCallStackReportingBrowserTest(const JSCallStackReportingBrowserTest&) =
      delete;
  JSCallStackReportingBrowserTest& operator=(
      const JSCallStackReportingBrowserTest&) = delete;

  ~JSCallStackReportingBrowserTest() override = default;

  std::string GetDocumentPolicyHeader() const {
    return "Document-Policy: include-js-call-stacks-in-crash-reports";
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
};

}  // namespace

// Tests that NEL reports are delivered correctly, whether or not reporting
// isolation is enabled. NEL reports can only be configured with the Report-To
// header, but this header should continue to function until support is
// completely removed.
IN_PROC_BROWSER_TEST_P(ReportingBrowserTest, TestNELHeadersProcessed) {
  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetReportToHeader(), "&", GetNELHeader()}));
  EXPECT_TRUE(NavigateToURL(
      browser()->tab_strip_model()->GetActiveWebContents(), main_url));

  upload_response()->WaitForRequest();
  base::ListValue actual =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 204 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  base::ListValue expected = base::test::ParseJsonList(base::StringPrintf(
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
  EXPECT_EQ(actual, expected);
}

// Tests that CSP reports are delivered properly whether configured with the
// v0 Report-To header or the v1 Reporting-Endpoints header.
IN_PROC_BROWSER_TEST_P(ReportingBrowserTest, TestReportingHeadersProcessed) {
  // Navigate to reporting-enabled page.
  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header-with-file/chrome/test/data/simple_alert.html?",
                    GetAppropriateReportingHeader(), "&", GetCSPHeader()}));
  EXPECT_TRUE(NavigateToURL(
      browser()->tab_strip_model()->GetActiveWebContents(), main_url));

  upload_response()->WaitForRequest();
  base::ListValue actual =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 204 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  base::ListValue expected = base::test::ParseJsonList(base::StringPrintf(
      R"json(
        [ {
           "body": {
              "blockedURL": "inline",
              "columnNumber": 11,
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
  EXPECT_EQ(actual, expected);
}

// Tests that CSP reports are delivered properly whether configured with the
// v0 Report-To header or the v1 Reporting-Endpoints header. This is a Non-
// isolated test, so will run with NIK-based report isolation disabled. This is
// a regression test for https://crbug.com/40200931.
IN_PROC_BROWSER_TEST_P(NonIsolatedReportingBrowserTest,
                       TestReportingHeadersProcessed) {
  // Navigate to reporting-enabled page.
  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header-with-file/chrome/test/data/simple_alert.html?",
                    GetAppropriateReportingHeader(), "&", GetCSPHeader()}));
  EXPECT_TRUE(NavigateToURL(
      browser()->tab_strip_model()->GetActiveWebContents(), main_url));

  // Ensure that the correct endpoint was found, and that a report was sent.
  // (If the endpoint cannot not be found, then a report will be sent at all.)
  upload_response()->WaitForRequest();
  base::ListValue actual =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 204 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  base::ListValue expected = base::test::ParseJsonList(base::StringPrintf(
      R"json(
        [ {
           "body": {
              "blockedURL": "inline",
              "columnNumber": 11,
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
  EXPECT_EQ(actual, expected);
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTest,
                       ReportingRespectsNetworkIsolationKeys) {
  // Favicon page is necessary since they are not served by default (i.e.,
  // `title1.html`), and in that case the default request for a favicon will
  // trigger a NEL report for the wrong reason.
  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header-with-file/chrome/test/data/favicon/"
                    "page_with_favicon.html?",
                    GetReportToHeader(),
                    "&NEL: {\"report_to\":\"default\", \"max_age\":86400, "
                    "\"failure_fraction\":1.0}"}));
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
  base::ListValue actual =
      ParseReportUpload(upload_response()->http_request()->content);

  // Verify the contents of the received report.
  base::ListValue expected = base::test::ParseJsonList(base::StringPrintf(
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
  EXPECT_EQ(actual, expected);
}

// These tests intentionally crash a render process, and so fail ASan tests.
#if defined(ADDRESS_SANITIZER)
#define DISABLED_ON_ASAN(x) DISABLED_##x
#else
#define DISABLED_ON_ASAN(x) x
#endif  // defined(ADDRESS_SANITIZER)

IN_PROC_BROWSER_TEST_P(ReportingBrowserTest, DISABLED_ON_ASAN(CrashReport)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  // Simulate a crash on the page.
  content::RenderProcessHostWatcher crash_observer(
      contents, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  contents->GetController().LoadURL(GURL(blink::kChromeUICrashURL),
                                    content::Referrer(),
                                    ui::PAGE_TRANSITION_TYPED, std::string());
  crash_observer.Wait();

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");

  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTest,
                       DISABLED_ON_ASAN(CrashReportUnresponsive)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);

  content::SimulateUnresponsivePrimaryMainFrameAndWaitForExit(contents);

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");

  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());

  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "unresponsive");
}

IN_PROC_BROWSER_TEST_P(
    CrashReportingBrowserTest,
    DISABLED_ON_ASAN(CrashReportUnresponsiveWithoutHungExitCode)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);

  content::WebContentsConsoleObserver console_observer(contents);
  console_observer.SetPattern("infiniteLoop");
  ExecuteInfiniteLoopScriptAsync(frame);
  ASSERT_TRUE(console_observer.Wait());

  content::RenderProcessHost* rph = frame->GetProcess();
  content::RenderProcessHostWatcher watcher(
      rph, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  // Mark the widget as unresponsive.
  content::SimulateUnresponsiveRenderer(contents, frame->GetRenderWidgetHost());

  // Simulate process kill with a generic exit code (e.g. RESULT_CODE_KILLED),
  // not RESULT_CODE_HUNG.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  rph->Shutdown(content::RESULT_CODE_KILLED);
  watcher.Wait();

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify that the crash report was still generated with reason:
  // "unresponsive" because the browser detected that the renderer was
  // unresponsive.
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");

  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());

  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "unresponsive");
}

IN_PROC_BROWSER_TEST_P(CrashReportingBrowserTest,
                       DISABLED_ON_ASAN(CrashReportOOM)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);

  content::SimulateOOMPrimaryMainFrameAndWaitForExit(contents);

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify that the crash report was generated with reason: "oom".
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");

  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());

  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "oom");
}

// This test is Windows-only because Windows is the only platform that has the
// relevant exit code (`base::win::kStatusInvalidImageHashExitCode`).
#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_P(CrashReportingBrowserTest,
                       DISABLED_ON_ASAN(CrashReportIntegrityFailure)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);

  content::RenderProcessHost* rph = frame->GetProcess();
  content::RenderProcessHostWatcher watcher(
      rph, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  EXPECT_TRUE(rph->Shutdown(base::win::kStatusInvalidImageHashExitCode));
  watcher.Wait();
  EXPECT_FALSE(watcher.did_exit_normally());
  EXPECT_TRUE(contents->IsCrashed());

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify that the crash report was generated with no reason.
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");

  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());

  EXPECT_EQ(reason, nullptr);
}
#endif  // BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_P(CrashReportingBrowserTest,
                       DISABLED_ON_ASAN(CrashReportKilledBadMessage)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);

  content::RenderProcessHost* rph = frame->GetProcess();
  content::RenderProcessHostWatcher watcher(
      rph, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  EXPECT_TRUE(rph->Shutdown(content::RESULT_CODE_KILLED_BAD_MESSAGE));
  watcher.Wait();
  EXPECT_FALSE(watcher.did_exit_normally());
  EXPECT_TRUE(contents->IsCrashed());

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify that the crash report was generated with no reason.
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");

  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());

  EXPECT_EQ(reason, nullptr);
}

#if BUILDFLAG(IS_POSIX)
IN_PROC_BROWSER_TEST_P(CrashReportingBrowserTest,
                       DISABLED_ON_ASAN(CrashReportProcessWasKilledBySigkill)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);

  content::RenderProcessHost* rph = frame->GetProcess();
  content::RenderProcessHostWatcher watcher(
      rph, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  ASSERT_EQ(kill(rph->GetProcess().Pid(), SIGKILL), 0);
  watcher.Wait();
  EXPECT_FALSE(watcher.did_exit_normally());
  EXPECT_TRUE(contents->IsCrashed());

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");

  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());

#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, an external SIGKILL on a responsive renderer is mapped to
  // TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM and reported as "oom".
  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "oom");
#else
  EXPECT_EQ(reason, nullptr);
#endif
}

IN_PROC_BROWSER_TEST_P(
    CrashReportingBrowserTest,
    DISABLED_ON_ASAN(CrashReportUnresponsiveProcessWasKilledBySigkill)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);

  content::WebContentsConsoleObserver console_observer(contents);
  console_observer.SetPattern("infiniteLoop");
  ExecuteInfiniteLoopScriptAsync(frame);
  ASSERT_TRUE(console_observer.Wait());

  content::RenderProcessHost* rph = frame->GetProcess();
  content::RenderProcessHostWatcher watcher(
      rph, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  // Mark the widget as unresponsive before killing the process with SIGKILL.
  content::SimulateUnresponsiveRenderer(contents, frame->GetRenderWidgetHost());

  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  ASSERT_EQ(kill(rph->GetProcess().Pid(), SIGKILL), 0);
  watcher.Wait();
  EXPECT_FALSE(watcher.did_exit_normally());
  EXPECT_TRUE(contents->IsCrashed());

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify that even when terminated with SIGKILL (which maps to
  // TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM on ChromeOS), an unresponsive
  // renderer is reported with reason: "unresponsive" rather than "oom".
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");

  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());

  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "unresponsive");
}
#endif  // BUILDFLAG(IS_POSIX)

IN_PROC_BROWSER_TEST_P(CrashReportingBrowserTest,
                       DISABLED_ON_ASAN(NoCrashReportForTaskManagerKill)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL killed_url = server()->GetURL(
      kReportingHost,
      base::StrCat(
          {"/set-header?", GetAppropriateReportingHeader(), "&killed"}));
  EXPECT_TRUE(NavigateToURL(contents, killed_url));

  auto tester =
      task_manager::TaskManagerTester::Create(base::RepeatingClosure());
  ASSERT_NO_FATAL_FAILURE(
      task_manager::browsertest_util::WaitForTaskManagerRows(
          1, task_manager::browsertest_util::MatchAnyTab()));

  SessionID tab_id = sessions::SessionTabHelper::IdForTab(contents);
  std::optional<size_t> tab_row;
  for (size_t i = 0; i < tester->GetRowCount(); ++i) {
    if (tester->GetTabId(i) == tab_id) {
      tab_row = i;
      break;
    }
  }
  ASSERT_TRUE(tab_row.has_value());

  content::RenderProcessHost* rph =
      contents->GetPrimaryMainFrame()->GetProcess();
  content::RenderProcessHostWatcher watcher(
      rph, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  tester->Kill(*tab_row);
  watcher.Wait();
  EXPECT_FALSE(watcher.did_exit_normally());
  EXPECT_TRUE(contents->IsCrashed());

  // Crash report delivery is asynchronous across the NetworkContext Mojo pipe
  // and the subsequent HTTP POST to `upload_response()`, so asserting
  // immediately after `watcher.Wait()` could pass before a queued report
  // arrives, while calling `upload_response()->WaitForRequest()` directly would
  // hang when no report is sent.
  //
  // To deterministically flush the reporting pipeline without arbitrary sleeps,
  // navigate to a second reporting-enabled page on the same endpoint and
  // trigger a real crash. Any report queued for `killed_url` would either
  // arrive at `upload_response()` first or be batched into the same upload
  // payload.
  GURL crashed_url = server()->GetURL(
      kReportingHost,
      base::StrCat(
          {"/set-header?", GetAppropriateReportingHeader(), "&crashed"}));
  EXPECT_TRUE(NavigateToURL(contents, crashed_url));

  content::SimulateOOMPrimaryMainFrameAndWaitForExit(contents);

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  ASSERT_EQ(response.size(), 1u);
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");

  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, crashed_url.spec());
}

IN_PROC_BROWSER_TEST_P(
    CrashReportingBrowserTest,
    DISABLED_ON_ASAN(CrashReportOOMTakesPriorityOverUnresponsive)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);

  // Mark the renderer as unresponsive.
  content::SimulateUnresponsiveRenderer(contents, frame->GetRenderWidgetHost());

  // Simulate OOM shutdown while unresponsive.
  content::SimulateOOMPrimaryMainFrameAndWaitForExit(contents);

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify that the crash report was generated with reason: "oom" rather than
  // "unresponsive".
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");

  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());

  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "oom");
}

IN_PROC_BROWSER_TEST_P(CrashReportingBrowserTest,
                       DISABLED_ON_ASAN(CrashReportBlinkOOM)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);

  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  content::RenderProcessHostWatcher crash_observer(
      contents, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  contents->GetController().LoadURL(GURL(blink::kChromeUIBlinkOOMURL),
                                    content::Referrer(),
                                    ui::PAGE_TRANSITION_TYPED, std::string());
  crash_observer.Wait();

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify that the crash report was generated with reason: "oom" from Blink's
  // OOM callback.
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");

  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());

  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "oom");
}

IN_PROC_BROWSER_TEST_P(CrashReportingBrowserTest,
                       DISABLED_ON_ASAN(CrashReportBlinkWorkerOOM)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);

  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  content::RenderProcessHostWatcher crash_observer(
      contents, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  contents->GetController().LoadURL(GURL(blink::kChromeUIBlinkWorkerOOMURL),
                                    content::Referrer(),
                                    ui::PAGE_TRANSITION_TYPED, std::string());
  crash_observer.Wait();

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify that the crash report was generated with reason: "oom" from Blink's
  // OOM callback on the worker thread.
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");

  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());

  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "oom");
}

IN_PROC_BROWSER_TEST_P(CrashReportingBrowserTest,
                       DISABLED_ON_ASAN(CrashReportV8OOM)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);

  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  content::RenderProcessHostWatcher crash_observer(
      contents, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  contents->GetController().LoadURL(GURL(blink::kChromeUIV8OOMURL),
                                    content::Referrer(),
                                    ui::PAGE_TRANSITION_TYPED, std::string());
  crash_observer.Wait();

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify that the crash report was generated with reason: "oom" from Blink's
  // OOM callback when V8 triggers FatalProcessOutOfMemory.
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");

  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());

  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "oom");
}

IN_PROC_BROWSER_TEST_P(CrashReportingBrowserTest,
                       DISABLED_ON_ASAN(NoCrashReportForBFCachedPage)) {
  if (!content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    GTEST_SKIP();
  }

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL bfcached_url = server()->GetURL(
      kReportingHost,
      base::StrCat(
          {"/set-header?", GetAppropriateReportingHeader(), "&bfcached"}));
  EXPECT_TRUE(NavigateToURL(contents, bfcached_url));

  content::RenderFrameHostWrapper rfh_bfcached(contents->GetPrimaryMainFrame());
  ASSERT_TRUE(rfh_bfcached.get());

  // Navigate to a second reporting-enabled page so the first page enters
  // BackForwardCache.
  GURL active_url = server()->GetURL(
      kReportingHost,
      base::StrCat(
          {"/set-header?", GetAppropriateReportingHeader(), "&active"}));
  EXPECT_TRUE(NavigateToURL(contents, active_url));

  ASSERT_FALSE(rfh_bfcached.IsDestroyed());
  ASSERT_EQ(rfh_bfcached->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  content::RenderFrameHost* rfh_active = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh_active);

  content::ScopedAllowRendererCrashes allow_renderer_crashes;
  if (rfh_bfcached->GetProcess() != rfh_active->GetProcess()) {
    content::RenderProcessHost* rph_bfcached = rfh_bfcached->GetProcess();
    content::RenderProcessHostWatcher bfcached_watcher(
        rph_bfcached,
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    rph_bfcached->Shutdown(content::RESULT_CODE_HUNG);
    bfcached_watcher.Wait();
  }

  // Crash the active page's renderer process.
  content::SimulateOOMPrimaryMainFrameAndWaitForExit(contents);

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify that only the active page generated a crash report, and not the
  // BFCached page.
  ASSERT_EQ(response.size(), 1u);
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");

  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, active_url.spec());
}

IN_PROC_BROWSER_TEST_P(CrashReportingBrowserTest,
                       DISABLED_ON_ASAN(NoCrashReportForPrerenderedPage)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader(), "&main"}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  GURL prerender_url = server()->GetURL(
      kReportingHost,
      base::StrCat(
          {"/set-header?", GetAppropriateReportingHeader(), "&prerender"}));
  content::PrerenderHostId host_id =
      prerender_helper().AddPrerender(prerender_url);
  content::RenderFrameHost* rfh_prerender =
      prerender_helper().GetPrerenderedMainFrameHost(host_id);
  ASSERT_TRUE(rfh_prerender);
  ASSERT_EQ(rfh_prerender->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kPrerendering);

  content::RenderFrameHost* rfh_main = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh_main);

  content::ScopedAllowRendererCrashes allow_renderer_crashes;
  if (rfh_prerender->GetProcess() != rfh_main->GetProcess()) {
    content::RenderProcessHost* rph_prerender = rfh_prerender->GetProcess();
    content::RenderProcessHostWatcher prerender_watcher(
        rph_prerender,
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    rph_prerender->Shutdown(content::RESULT_CODE_HUNG);
    prerender_watcher.Wait();
  }

  // Crash the active page's renderer process.
  content::SimulateOOMPrimaryMainFrameAndWaitForExit(contents);

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify that only the active page generated a crash report, and not the
  // prerendered page.
  ASSERT_EQ(response.size(), 1u);
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");

  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTestCrashReportingStorage,
                       CrashStorageAPI) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to reporting-enabled page.
  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  // Use the crash reporting storage API, to collect some data about the current
  // page. In this case, just the current origin and a custom key, to verify
  // they come out on the other end of the crash report.
  EXPECT_TRUE(content::EvalJs(contents->GetPrimaryMainFrame(), R"(
    (async () => {
      await crashReport.initialize(1024);
      crashReport.set('self.origin', self.origin + '/');
      crashReport.set('outer_height', window.outerHeight);
      crashReport.set('custom_key', 'custom_value');
      crashReport.delete('custom_key');
    })();
  )")
                  .is_ok());

  // Simulate the page being killed due to being unresponsive.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_HUNG);

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");
  const base::DictValue* crash_report_api_body =
      body->FindDict("crash_report_api");
  ASSERT_NE(crash_report_api_body, nullptr);
  const std::string* self_origin =
      crash_report_api_body->FindString("self.origin");
  const std::string* custom_key =
      crash_report_api_body->FindString("custom_key");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");
  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());
  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "unresponsive");
  ASSERT_NE(self_origin, nullptr);
  EXPECT_EQ(*self_origin, contents->GetPrimaryMainFrame()
                              ->GetLastCommittedOrigin()
                              .GetURL()
                              .spec());
  // Because `crashReport.delete('custom_key')` was called before the process
  // crashed, this value is not present in the report body.
  EXPECT_EQ(custom_key, nullptr);
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTestCrashReportingStorage,
                       CrashStorageAPIDuplicateValue) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to reporting-enabled page.
  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  // Use the crash reporting storage API, to collect some data about the current
  // page. In this case, a custom key gets written to twice, to verify that the
  // most recent write gets recorded and comes out on the other end of the crash
  // report.
  EXPECT_TRUE(content::EvalJs(contents->GetPrimaryMainFrame(), R"(
    (async () => {
      await crashReport.initialize(100);
      crashReport.set('custom_key', 'value_1');
      crashReport.set('custom_key', 'value_2');
    })();
  )")
                  .is_ok());

  // Simulate the page being killed due to being unresponsive.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_HUNG);

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");
  const base::DictValue* crash_report_api_body =
      body->FindDict("crash_report_api");
  ASSERT_NE(crash_report_api_body, nullptr);
  const std::string* custom_key =
      crash_report_api_body->FindString("custom_key");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");
  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());
  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "unresponsive");
  ASSERT_NE(custom_key, nullptr);
  EXPECT_EQ(*custom_key, "value_2");
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTestCrashReportingStorage,
                       CrashStorageAPIFullBuffer) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to reporting-enabled page.
  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  // It takes exactly 29 bytes to write the JSON stringified data containing
  // this key/value pair to shared memory, because of the extra JSON characters.
  // Request exactly 29 bytes, and verify that the key/value are preserved.
  // Because the browser needs to store a leading integer before the
  // developer-supplied bytes to track how many bytes of shared memory are
  // actually used, this means that the browser must internally allocate
  // 29 + `sizeof(uint32_t)` bytes, the first `sizeof(uint32_t)` of which are
  // reserved for the leading integer. This test ensures that the browser
  // allocates the right number of bytes, instead of using some of the
  // developer-requested bytes to store the leading integer; if it did that, the
  // key/value data that requires all 29 bytes will be truncated.
  EXPECT_TRUE(content::EvalJs(contents->GetPrimaryMainFrame(), R"(
    (async () => {
      await crashReport.initialize(29);
      crashReport.set('custom_key', 'custom_value');
    })();
  )")
                  .is_ok());

  // Simulate the page being killed due to being unresponsive.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_HUNG);

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");
  const base::DictValue* crash_report_api_body =
      body->FindDict("crash_report_api");
  ASSERT_NE(crash_report_api_body, nullptr);
  const std::string* custom_key =
      crash_report_api_body->FindString("custom_key");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");
  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());
  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "unresponsive");
  ASSERT_NE(custom_key, nullptr);
  EXPECT_EQ(*custom_key, "custom_value");
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTestMoreContextData,
                       CrashReportUnresponsive) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to reporting-enabled page.
  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  // Simulate the page being killed due to being unresponsive.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_HUNG);

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");
  const std::optional<bool> is_top_level = body->FindBool("is_top_level");
  const std::string* visibility_state = body->FindString("visibility_state");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");
  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());
  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "unresponsive");
  // When the `kCrashReportingAPIMoreContextData` flag is enabled, expect the
  // extra CrashReportBody context bits to be present.
  if (GetParam()) {
    ASSERT_TRUE(is_top_level.has_value());
    EXPECT_TRUE(*is_top_level);
    ASSERT_NE(visibility_state, nullptr);
    EXPECT_EQ(*visibility_state, "visible");
  } else {
    EXPECT_EQ(is_top_level, std::nullopt);
    EXPECT_EQ(visibility_state, nullptr);
  }
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTestMoreContextData,
                       CrashReportHiddenPage) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to reporting-enabled page.
  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
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
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* visibility_state = body->FindString("visibility_state");

  // When the `kCrashReportingAPIMoreContextData` flag is enabled, expect the
  // extra CrashReportBody context bits to be present.
  if (GetParam()) {
    ASSERT_NE(visibility_state, nullptr);
    EXPECT_EQ(*visibility_state, "hidden");
  } else {
    EXPECT_EQ(visibility_state, nullptr);
  }
}

IN_PROC_BROWSER_TEST_P(
    ReportingBrowserTestMoreContextData,
    DISABLED_ON_ASAN(CrashReportUnresponsiveCrossOriginIframe)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(NavigateToURL(
      contents, server()->GetURL(kCrossOriginHost, "/iframe.html")));

  GURL iframe_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
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
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");
  const std::optional<bool> is_top_level = body->FindBool("is_top_level");
  const std::string* visibility_state = body->FindString("visibility_state");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");
  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, iframe_url.spec());
  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "unresponsive");
  if (GetParam()) {
    ASSERT_TRUE(is_top_level.has_value());
    EXPECT_FALSE(*is_top_level);
    ASSERT_NE(visibility_state, nullptr);
    EXPECT_EQ(*visibility_state, "visible");
  } else {
    EXPECT_EQ(is_top_level, std::nullopt);
    EXPECT_EQ(visibility_state, nullptr);
  }
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTestMoreContextData,
                       DISABLED_ON_ASAN(CrashReportHiddenCrossOriginIframe)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(NavigateToURL(
      contents, server()->GetURL(kCrossOriginHost, "/iframe.html")));

  GURL iframe_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  ASSERT_TRUE(NavigateIframeToURL(contents, "test", iframe_url));

  content::RenderFrameHost* subframe = ChildFrameAt(contents, 0);
  ASSERT_TRUE(subframe);

  // Hide the page before the cross-origin iframe crashes.
  contents->WasHidden();
  EXPECT_EQ(contents->GetPrimaryMainFrame()->GetVisibilityState(),
            content::PageVisibilityState::kHidden);

  content::ScopedAllowRendererCrashes allow_renderer_crashes(
      subframe->GetProcess());
  subframe->GetProcess()->Shutdown(content::RESULT_CODE_HUNG);

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::optional<bool> is_top_level = body->FindBool("is_top_level");
  const std::string* visibility_state = body->FindString("visibility_state");

  if (GetParam()) {
    ASSERT_TRUE(is_top_level.has_value());
    EXPECT_FALSE(*is_top_level);
    ASSERT_NE(visibility_state, nullptr);
    EXPECT_EQ(*visibility_state, "hidden");
  } else {
    EXPECT_EQ(is_top_level, std::nullopt);
    EXPECT_EQ(visibility_state, nullptr);
  }
}

IN_PROC_BROWSER_TEST_P(
    ReportingBrowserTestMoreContextData,
    DISABLED_ON_ASAN(CrashReportUnresponsiveSameOriginIframe)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header-with-file/chrome/test/data/iframe.html?",
                    GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  GURL iframe_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  ASSERT_TRUE(NavigateIframeToURL(contents, "test", iframe_url));

  content::RenderFrameHost* main_rfh = contents->GetPrimaryMainFrame();
  content::RenderFrameHost* subframe = ChildFrameAt(contents, 0);
  ASSERT_TRUE(subframe);
  ASSERT_EQ(subframe->GetProcess(), main_rfh->GetProcess());

  constexpr char kScript[] = R"(
    (async () => {
      await crashReport.initialize(100);
      crashReport.set('subframe_key', 'subframe_value');
    })();
  )";
  EXPECT_TRUE(content::EvalJs(subframe, kScript).is_ok());

  // Simulate the shared process being killed due to being unresponsive.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  main_rfh->GetProcess()->Shutdown(content::RESULT_CODE_HUNG);

  base::ListValue reports = WaitForReportUploads(/*expected_report_count=*/2);
  ASSERT_EQ(reports.size(), 2u);

  // Verify the main frame report.
  const base::DictValue& main_report = reports[0].GetDict();
  const std::string* main_type = main_report.FindString("type");
  ASSERT_NE(main_type, nullptr);
  EXPECT_EQ(*main_type, "crash");

  const std::string* main_report_url = main_report.FindString("url");
  ASSERT_NE(main_report_url, nullptr);
  EXPECT_EQ(*main_report_url, main_url.spec());

  const base::DictValue* main_body = main_report.FindDict("body");
  ASSERT_NE(main_body, nullptr);

  const std::string* main_reason = main_body->FindString("reason");
  ASSERT_NE(main_reason, nullptr);
  EXPECT_EQ(*main_reason, "unresponsive");

  const base::DictValue* main_crash_report_api_body =
      main_body->FindDict("crash_report_api");
  ASSERT_NE(main_crash_report_api_body, nullptr);
  EXPECT_TRUE(main_crash_report_api_body->empty());

  if (GetParam()) {
    const std::optional<bool> main_is_top_level =
        main_body->FindBool("is_top_level");
    ASSERT_TRUE(main_is_top_level.has_value());
    EXPECT_TRUE(*main_is_top_level);

    const std::string* main_visibility_state =
        main_body->FindString("visibility_state");
    ASSERT_NE(main_visibility_state, nullptr);
    EXPECT_EQ(*main_visibility_state, "visible");
  } else {
    EXPECT_EQ(main_body->FindBool("is_top_level"), std::nullopt);
    EXPECT_EQ(main_body->FindString("visibility_state"), nullptr);
  }

  // Verify the report generated by the same-process iframe.
  const base::DictValue& iframe_report = reports[1].GetDict();

  const std::string* iframe_type = iframe_report.FindString("type");
  ASSERT_NE(iframe_type, nullptr);
  EXPECT_EQ(*iframe_type, "crash");

  const std::string* iframe_report_url = iframe_report.FindString("url");
  ASSERT_NE(iframe_report_url, nullptr);
  EXPECT_EQ(*iframe_report_url, iframe_url.spec());

  const base::DictValue* iframe_body = iframe_report.FindDict("body");
  ASSERT_NE(iframe_body, nullptr);

  const std::string* iframe_reason = iframe_body->FindString("reason");
  ASSERT_NE(iframe_reason, nullptr);
  EXPECT_EQ(*iframe_reason, "unresponsive");

  const base::DictValue* crash_report_api_body =
      iframe_body->FindDict("crash_report_api");
  ASSERT_NE(crash_report_api_body, nullptr);

  const std::string* subframe_key =
      crash_report_api_body->FindString("subframe_key");
  ASSERT_NE(subframe_key, nullptr);
  EXPECT_EQ(*subframe_key, "subframe_value");

  if (GetParam()) {
    const std::optional<bool> iframe_is_top_level =
        iframe_body->FindBool("is_top_level");
    ASSERT_TRUE(iframe_is_top_level.has_value());
    EXPECT_FALSE(*iframe_is_top_level);

    const std::string* iframe_visibility_state =
        iframe_body->FindString("visibility_state");
    ASSERT_NE(iframe_visibility_state, nullptr);
    EXPECT_EQ(*iframe_visibility_state, "visible");
  } else {
    EXPECT_EQ(iframe_body->FindBool("is_top_level"), std::nullopt);
    EXPECT_EQ(iframe_body->FindString("visibility_state"), nullptr);
  }
}

IN_PROC_BROWSER_TEST_P(
    CrashReportingBrowserTest,
    DISABLED_ON_ASAN(CrashReportMainFrameAndSameOriginIframe)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header-with-file/chrome/test/data/iframe.html?",
                    GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  GURL iframe_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  ASSERT_TRUE(NavigateIframeToURL(contents, "test", iframe_url));

  content::RenderFrameHost* main_rfh = contents->GetPrimaryMainFrame();
  content::RenderFrameHost* subframe = ChildFrameAt(contents, 0);
  ASSERT_TRUE(subframe);
  ASSERT_EQ(subframe->GetProcess(), main_rfh->GetProcess());

  constexpr char kMainScript[] = R"(
    (async () => {
      await crashReport.initialize(100);
      crashReport.set('frame', 'main');
    })();
  )";
  EXPECT_TRUE(content::EvalJs(main_rfh, kMainScript).is_ok());

  constexpr char kSubframeScript[] = R"(
    (async () => {
      await crashReport.initialize(100);
      crashReport.set('frame', 'subframe');
    })();
  )";
  EXPECT_TRUE(content::EvalJs(subframe, kSubframeScript).is_ok());

  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  content::SimulateOOMPrimaryMainFrameAndWaitForExit(contents);

  base::ListValue reports = WaitForReportUploads(/*expected_report_count=*/2);
  ASSERT_EQ(reports.size(), 2u);

  const base::DictValue& main_report = reports[0].GetDict();

  const std::string* main_type = main_report.FindString("type");
  ASSERT_NE(main_type, nullptr);
  EXPECT_EQ(*main_type, "crash");

  const std::string* main_report_url = main_report.FindString("url");
  ASSERT_NE(main_report_url, nullptr);
  EXPECT_EQ(*main_report_url, main_url.spec());

  const base::DictValue* main_body = main_report.FindDict("body");
  ASSERT_NE(main_body, nullptr);

  const std::string* main_reason = main_body->FindString("reason");
  ASSERT_NE(main_reason, nullptr);
  EXPECT_EQ(*main_reason, "oom");

  const base::DictValue* main_crash_report_api =
      main_body->FindDict("crash_report_api");
  ASSERT_NE(main_crash_report_api, nullptr);

  const std::string* main_frame_value =
      main_crash_report_api->FindString("frame");
  ASSERT_NE(main_frame_value, nullptr);
  EXPECT_EQ(*main_frame_value, "main");

  const base::DictValue& iframe_report = reports[1].GetDict();

  const std::string* iframe_type = iframe_report.FindString("type");
  ASSERT_NE(iframe_type, nullptr);
  EXPECT_EQ(*iframe_type, "crash");

  const std::string* iframe_report_url = iframe_report.FindString("url");
  ASSERT_NE(iframe_report_url, nullptr);
  EXPECT_EQ(*iframe_report_url, iframe_url.spec());

  const base::DictValue* iframe_body = iframe_report.FindDict("body");
  ASSERT_NE(iframe_body, nullptr);

  const std::string* iframe_reason = iframe_body->FindString("reason");
  ASSERT_NE(iframe_reason, nullptr);
  EXPECT_EQ(*iframe_reason, "oom");

  const base::DictValue* iframe_crash_report_api =
      iframe_body->FindDict("crash_report_api");
  ASSERT_NE(iframe_crash_report_api, nullptr);

  const std::string* iframe_frame_value =
      iframe_crash_report_api->FindString("frame");
  ASSERT_NE(iframe_frame_value, nullptr);
  EXPECT_EQ(*iframe_frame_value, "subframe");
}

IN_PROC_BROWSER_TEST_P(
    CrashReportingBrowserTest,
    DISABLED_ON_ASAN(
        CrashReportInnerWebContentsSharingProcessUnderProcessLimit)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header-with-file/chrome/test/data/iframe.html?",
                    GetAppropriateReportingHeader(), "&outer"}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  content::RenderFrameHost* main_rfh = contents->GetPrimaryMainFrame();
  content::RenderFrameHost* subframe = ChildFrameAt(contents, 0);
  ASSERT_TRUE(subframe);

  GURL inner_url = server()->GetURL(
      kReportingHost,
      base::StrCat(
          {"/set-header?", GetAppropriateReportingHeader(), "&inner"}));

  content::WebContents::CreateParams create_params(
      contents->GetBrowserContext(),
      content::SiteInstance::CreateForURL(contents->GetBrowserContext(),
                                          inner_url));
  content::WebContents* inner_contents =
      content::CreateAndAttachInnerContents(subframe, std::move(create_params));
  ASSERT_TRUE(inner_contents);

  EXPECT_TRUE(content::NavigateToURLFromRenderer(inner_contents, inner_url));

  content::RenderFrameHost* inner_rfh = inner_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(inner_rfh);
  ASSERT_EQ(inner_rfh->GetProcess(), main_rfh->GetProcess());
  ASSERT_NE(inner_rfh->GetSiteInstance(), main_rfh->GetSiteInstance());
  ASSERT_FALSE(inner_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      main_rfh->GetSiteInstance()));

  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  content::SimulateOOMPrimaryMainFrameAndWaitForExit(contents);

  base::ListValue reports = WaitForReportUploads(/*expected_report_count=*/2);
  ASSERT_EQ(reports.size(), 2u);

  const base::DictValue& main_report = reports[0].GetDict();
  const std::string* main_type = main_report.FindString("type");
  ASSERT_NE(main_type, nullptr);
  EXPECT_EQ(*main_type, "crash");
  const std::string* main_report_url = main_report.FindString("url");
  ASSERT_NE(main_report_url, nullptr);
  EXPECT_EQ(*main_report_url, main_url.spec());
  const base::DictValue* main_body = main_report.FindDict("body");
  ASSERT_NE(main_body, nullptr);
  const std::string* main_reason = main_body->FindString("reason");
  ASSERT_NE(main_reason, nullptr);
  EXPECT_EQ(*main_reason, "oom");

  const base::DictValue& inner_report = reports[1].GetDict();
  const std::string* inner_type = inner_report.FindString("type");
  ASSERT_NE(inner_type, nullptr);
  EXPECT_EQ(*inner_type, "crash");
  const std::string* inner_report_url = inner_report.FindString("url");
  ASSERT_NE(inner_report_url, nullptr);
  EXPECT_EQ(*inner_report_url, inner_url.spec());
  const base::DictValue* inner_body = inner_report.FindDict("body");
  ASSERT_NE(inner_body, nullptr);
  const std::string* inner_reason = inner_body->FindString("reason");
  ASSERT_NE(inner_reason, nullptr);
  EXPECT_EQ(*inner_reason, "oom");
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTestSpecifyCrashEndpoint,
                       DISABLED_ON_ASAN(SpecifyCrashEndpoint)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  // Simulate a crash on the page.
  content::RenderProcessHostWatcher crash_observer(
      contents, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  contents->GetController().LoadURL(GURL(blink::kChromeUICrashURL),
                                    content::Referrer(),
                                    ui::PAGE_TRANSITION_TYPED, std::string());
  crash_observer.Wait();

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");
  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());
}

IN_PROC_BROWSER_TEST_P(JSCallStackReportingBrowserTest,
                       DISABLED_ON_ASAN(MainPageOptedIn)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader(), "&",
                    GetDocumentPolicyHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);
  content::WebContentsConsoleObserver console_observer(contents);
  console_observer.SetPattern("infiniteLoop");

  ExecuteInfiniteLoopScriptAsync(frame);

  ASSERT_TRUE(console_observer.Wait());
  content::SimulateUnresponsivePrimaryMainFrameAndWaitForExit(contents);

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");
  const std::string* call_stack = body->FindString("stack");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");
  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());
  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "unresponsive");
  // TODO(crbug.com/407473725): Improve JS call stack collection test coverage.
  if (GetParam() && call_stack) {
    EXPECT_TRUE(call_stack->find("infiniteLoop") != std::string::npos);
  } else {
    EXPECT_EQ(call_stack, nullptr);
  }
}

IN_PROC_BROWSER_TEST_P(JSCallStackReportingBrowserTest,
                       DISABLED_ON_ASAN(MainPageNotOptedIn)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame);
  content::WebContentsConsoleObserver console_observer(contents);
  console_observer.SetPattern("infiniteLoop");

  ExecuteInfiniteLoopScriptAsync(frame);

  ASSERT_TRUE(console_observer.Wait());

  content::SimulateUnresponsivePrimaryMainFrameAndWaitForExit(contents);

  upload_response()->WaitForRequest();
  base::ListValue response =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");
  ASSERT_NE(body, nullptr);
  const std::string* reason = body->FindString("reason");
  const std::string* call_stack = body->FindString("stack");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");
  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());
  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "unresponsive");
  // TODO(crbug.com/407473725): Improve JS call stack collection test coverage.
  if (GetParam() && call_stack) {
    EXPECT_EQ(
        *call_stack,
        "Website owner has not opted in for JS call stacks in crash reports.");
  } else {
    EXPECT_EQ(call_stack, nullptr);
  }
}

IN_PROC_BROWSER_TEST_P(
    JSCallStackReportingBrowserTest,
    DISABLED_ON_ASAN(IframeUnresponsiveWithJSCallStackOptedIn)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header-with-file/chrome/test/data/iframe.html?",
                    GetAppropriateReportingHeader(), "&",
                    GetDocumentPolicyHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  GURL iframe_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader(), "&",
                    GetDocumentPolicyHeader()}));
  ASSERT_TRUE(NavigateIframeToURL(contents, "test", iframe_url));
  content::RenderFrameHost* subframe = ChildFrameAt(contents, 0);
  ASSERT_TRUE(subframe);

  content::WebContentsConsoleObserver console_observer(contents);
  console_observer.SetPattern("infiniteLoop");
  ExecuteInfiniteLoopScriptAsync(subframe);
  ASSERT_TRUE(console_observer.Wait());

  content::SimulateUnresponsivePrimaryMainFrameAndWaitForExit(contents);

  base::ListValue reports = WaitForReportUploads(/*expected_report_count=*/2);
  ASSERT_EQ(reports.size(), 2u);

  // Verify the main frame report.
  const base::DictValue& main_report = reports[0].GetDict();

  const std::string* main_type = main_report.FindString("type");
  ASSERT_NE(main_type, nullptr);
  EXPECT_EQ(*main_type, "crash");

  const std::string* main_report_url = main_report.FindString("url");
  ASSERT_NE(main_report_url, nullptr);
  EXPECT_EQ(*main_report_url, main_url.spec());

  const base::DictValue* main_body = main_report.FindDict("body");
  ASSERT_NE(main_body, nullptr);

  const std::string* main_reason = main_body->FindString("reason");
  ASSERT_NE(main_reason, nullptr);
  EXPECT_EQ(*main_reason, "unresponsive");

  const std::string* main_call_stack = main_body->FindString("stack");
  if (GetParam() && main_call_stack) {
    EXPECT_EQ(*main_call_stack, "Unable to collect JS call stack.");
  } else {
    EXPECT_EQ(main_call_stack, nullptr);
  }

  // Verify the same-process iframe report includes the JS call stack.
  const base::DictValue& iframe_report = reports[1].GetDict();

  const std::string* iframe_type = iframe_report.FindString("type");
  ASSERT_NE(iframe_type, nullptr);
  EXPECT_EQ(*iframe_type, "crash");

  const std::string* iframe_report_url = iframe_report.FindString("url");
  ASSERT_NE(iframe_report_url, nullptr);
  EXPECT_EQ(*iframe_report_url, iframe_url.spec());

  const base::DictValue* iframe_body = iframe_report.FindDict("body");
  ASSERT_NE(iframe_body, nullptr);

  const std::string* iframe_reason = iframe_body->FindString("reason");
  ASSERT_NE(iframe_reason, nullptr);
  EXPECT_EQ(*iframe_reason, "unresponsive");

  const std::string* iframe_call_stack = iframe_body->FindString("stack");
  if (GetParam() && iframe_call_stack) {
    EXPECT_TRUE(iframe_call_stack->find("infiniteLoop") != std::string::npos);
  } else {
    EXPECT_EQ(iframe_call_stack, nullptr);
  }
}

IN_PROC_BROWSER_TEST_P(
    JSCallStackReportingBrowserTest,
    DISABLED_ON_ASAN(IframeUnresponsiveWithJSCallStackNotOptedIn)) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header-with-file/chrome/test/data/iframe.html?",
                    GetAppropriateReportingHeader(), "&",
                    GetDocumentPolicyHeader()}));
  EXPECT_TRUE(NavigateToURL(contents, main_url));

  GURL iframe_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  ASSERT_TRUE(NavigateIframeToURL(contents, "test", iframe_url));
  content::RenderFrameHost* subframe = ChildFrameAt(contents, 0);
  ASSERT_TRUE(subframe);

  content::WebContentsConsoleObserver console_observer(contents);
  console_observer.SetPattern("infiniteLoop");
  ExecuteInfiniteLoopScriptAsync(subframe);
  ASSERT_TRUE(console_observer.Wait());

  content::SimulateUnresponsivePrimaryMainFrameAndWaitForExit(contents);

  base::ListValue reports = WaitForReportUploads(/*expected_report_count=*/2);
  ASSERT_EQ(reports.size(), 2u);

  // Verify the main frame report.
  const base::DictValue& main_report = reports[0].GetDict();

  const std::string* main_type = main_report.FindString("type");
  ASSERT_NE(main_type, nullptr);
  EXPECT_EQ(*main_type, "crash");

  const std::string* main_report_url = main_report.FindString("url");
  ASSERT_NE(main_report_url, nullptr);
  EXPECT_EQ(*main_report_url, main_url.spec());

  const base::DictValue* main_body = main_report.FindDict("body");
  ASSERT_NE(main_body, nullptr);

  const std::string* main_reason = main_body->FindString("reason");
  ASSERT_NE(main_reason, nullptr);
  EXPECT_EQ(*main_reason, "unresponsive");

  const std::string* main_call_stack = main_body->FindString("stack");
  if (GetParam() && main_call_stack) {
    EXPECT_EQ(*main_call_stack, "Unable to collect JS call stack.");
  } else {
    EXPECT_EQ(main_call_stack, nullptr);
  }

  // Verify the same-process iframe report omits the JS call stack since the
  // iframe did not opt in.
  const base::DictValue& iframe_report = reports[1].GetDict();

  const std::string* iframe_type = iframe_report.FindString("type");
  ASSERT_NE(iframe_type, nullptr);
  EXPECT_EQ(*iframe_type, "crash");

  const std::string* iframe_report_url = iframe_report.FindString("url");
  ASSERT_NE(iframe_report_url, nullptr);
  EXPECT_EQ(*iframe_report_url, iframe_url.spec());

  const base::DictValue* iframe_body = iframe_report.FindDict("body");
  ASSERT_NE(iframe_body, nullptr);

  const std::string* iframe_reason = iframe_body->FindString("reason");
  ASSERT_NE(iframe_reason, nullptr);
  EXPECT_EQ(*iframe_reason, "unresponsive");

  const std::string* iframe_call_stack = iframe_body->FindString("stack");
  if (GetParam() && iframe_call_stack) {
    EXPECT_EQ(
        *iframe_call_stack,
        "Website owner has not opted in for JS call stacks in crash reports.");
  } else {
    EXPECT_EQ(iframe_call_stack, nullptr);
  }
}

IN_PROC_BROWSER_TEST_P(HistogramReportingBrowserTest,
                       CrashReportUnresponsiveHistogram) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::HistogramTester histogram_tester;

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
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
INSTANTIATE_TEST_SUITE_P(All, CrashReportingBrowserTest, ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         NonIsolatedReportingBrowserTest,
                         ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         ReportingBrowserTestMoreContextData,
                         ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         ReportingBrowserTestCrashReportingStorage,
                         ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         ReportingBrowserTestSpecifyCrashEndpoint,
                         ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         JSCallStackReportingBrowserTest,
                         ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All, HistogramReportingBrowserTest, ::testing::Bool());

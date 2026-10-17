// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/android/child_process_binding_types.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ssl/cert_verifier_platform_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/android/child_process_importance.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
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
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

const char kReportingHost[] = "example.test";

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

// On Android (`android_browsertests`), browser tests inherit from
// `AndroidBrowserTest` rather than desktop-only `InProcessBrowserTest` /
// `CertVerifierBrowserTest`. `CertVerifierPlatformBrowserTest` derives from
// `PlatformBrowserTest` (which aliases to `AndroidBrowserTest` on Android) and
// sets up `ContentMockCertVerifier` so Reporting API HTTPS origins trust the
// embedded test server certificate.
class AndroidCrashReportingBrowserTest
    : public CertVerifierPlatformBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  AndroidCrashReportingBrowserTest()
      : https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {network::features::kReporting, network::features::kNetworkErrorLogging,
         net::features::kPartitionConnectionsByNetworkIsolationKey,
         blink::features::kDocumentPolicyIncludeJSCallStacksInCrashReports},
        /*disabled_features=*/{});
  }

  AndroidCrashReportingBrowserTest(const AndroidCrashReportingBrowserTest&) =
      delete;
  AndroidCrashReportingBrowserTest& operator=(
      const AndroidCrashReportingBrowserTest&) = delete;

  ~AndroidCrashReportingBrowserTest() override = default;

  void SetUp() override {
    CertVerifierPlatformBrowserTest::SetUp();

    // Make report delivery happen instantly.
    net::ReportingPolicy policy;
    policy.delivery_interval = base::Seconds(0);
    net::ReportingPolicy::UsePolicyForTesting(policy);
  }

  void SetUpOnMainThread() override {
    CertVerifierPlatformBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    upload_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(server(),
                                                                     "/upload");

    mock_cert_verifier()->set_default_result(net::OK);
    server()->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(server()->Start());
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  net::EmbeddedTestServer* server() { return &https_server_; }

  net::test_server::ControllableHttpResponse* upload_response() {
    return upload_response_.get();
  }

  GURL GetCollectorURL() const {
    return https_server_.GetURL(kReportingHost, "/upload");
  }

  std::string GetAppropriateReportingHeader() const {
    return base::EscapeQueryParamValue(
        GetParam() ? GetReportingEndpointsHeader() : GetReportToHeader(),
        /*use_plus=*/false);
  }

  std::string GetReportingEndpointsHeader() const {
    return base::StrCat(
        {"Reporting-Endpoints: default=\"", GetCollectorURL().spec(), "\""});
  }

  std::string GetReportToHeader() const {
    return base::StrCat({"Report-To: {\"endpoints\":[{\"url\":\"",
                         GetCollectorURL().spec(), "\"}],\"max_age\":86400}"});
  }

  std::string GetDocumentPolicyHeader() const {
    return "Document-Policy: include-js-call-stacks-in-crash-reports";
  }

 protected:
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
  net::EmbeddedTestServer https_server_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> upload_response_;
};

// On Android, foreground renderer terminations report
// `base::TERMINATION_STATUS_OOM_PROTECTED` because `waitpid()` cannot be used
// on Zygote children. Verify that a normal foreground renderer crash omits
// `reason` instead of misreporting `reason: "oom"`.
IN_PROC_BROWSER_TEST_P(AndroidCrashReportingBrowserTest,
                       ForegroundCrashReportOmitsReason) {
  content::WebContents* contents = GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(content::NavigateToURL(contents, main_url));

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

  ASSERT_FALSE(response.empty());
  const base::DictValue& report = response.begin()->GetDict();
  const std::string* type = report.FindString("type");
  const std::string* url = report.FindString("url");
  const base::DictValue* body = report.FindDict("body");

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");

  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());

  ASSERT_NE(body, nullptr);
  EXPECT_EQ(body->FindString("reason"), nullptr);
}

// Verify that when a foreground Android renderer is killed due to being
// unresponsive (`base::TERMINATION_STATUS_OOM_PROTECTED`), the report specifies
// `reason: "unresponsive"` rather than `"oom"`.
IN_PROC_BROWSER_TEST_P(AndroidCrashReportingBrowserTest,
                       ForegroundUnresponsiveCrashReportHasUnresponsiveReason) {
  content::WebContents* contents = GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(content::NavigateToURL(contents, main_url));

  content::SimulateUnresponsivePrimaryMainFrameAndWaitForExit(contents);

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

  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "unresponsive");
}

// Verify that when a foreground Android renderer with
// `Document-Policy: include-js-call-stacks-in-crash-reports` becomes
// unresponsive and is terminated (`base::TERMINATION_STATUS_OOM_PROTECTED`),
// `reason` is `"unresponsive"` so the collected JS call stack is included.
IN_PROC_BROWSER_TEST_P(
    AndroidCrashReportingBrowserTest,
    ForegroundUnresponsiveWithDocumentPolicyIncludesJSCallStack) {
  content::WebContents* contents = GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader(), "&",
                    GetDocumentPolicyHeader()}));
  EXPECT_TRUE(content::NavigateToURL(contents, main_url));

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

  // SimulateUnresponsivePrimaryMainFrameAndWaitForExit() immediately calls
  // Shutdown(RESULT_CODE_HUNG) after requesting the JS call stack via an
  // async Mojo IPC InterruptJavaScriptIsolateAndCollectCallStack(), so
  // call_stack is only populated if the IPC reply arrives before the process
  // exits, causing a race. See crbug.com/407473725.
  if (call_stack) {
    EXPECT_TRUE(call_stack->find("infiniteLoop") != std::string::npos);
  }
}

// Verify that an out-of-memory kill on a foreground Android page sets
// `reason: "oom"`.
IN_PROC_BROWSER_TEST_P(AndroidCrashReportingBrowserTest,
                       ForegroundOOMCrashReportHasOOMReason) {
  content::WebContents* contents = GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(content::NavigateToURL(contents, main_url));

  content::SimulateOOMPrimaryMainFrameAndWaitForExit(contents);

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

  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(*reason, "oom");
}

// Verify that a renderer killed due to a bad Mojo message on Android still
// generates a crash report with omitted `reason`.
IN_PROC_BROWSER_TEST_P(AndroidCrashReportingBrowserTest,
                       ForegroundKilledBadMessageCrashReportOmitsReason) {
  content::WebContents* contents = GetActiveWebContents();

  GURL main_url = server()->GetURL(
      kReportingHost,
      base::StrCat({"/set-header?", GetAppropriateReportingHeader()}));
  EXPECT_TRUE(content::NavigateToURL(contents, main_url));

  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  content::RenderProcessHostWatcher watcher(
      contents, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_KILLED_BAD_MESSAGE);
  watcher.Wait();

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

  ASSERT_NE(type, nullptr);
  EXPECT_EQ(*type, "crash");

  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, main_url.spec());

  ASSERT_NE(body, nullptr);
  EXPECT_EQ(body->FindString("reason"), nullptr);
}

// On Android, hiding a page downgrades its child process bindings so
// `ChildProcessLauncherHelper::GetTerminationInfo` returns
// `base::TERMINATION_STATUS_NORMAL_TERMINATION` when the renderer is evicted
// by the OS under memory pressure. Verify that terminating a hidden renderer
// does not generate a spurious crash report.
IN_PROC_BROWSER_TEST_P(AndroidCrashReportingBrowserTest,
                       NoCrashReportForHiddenPageEviction) {
  content::WebContents* contents = GetActiveWebContents();

  GURL evicted_url = server()->GetURL(
      kReportingHost,
      base::StrCat(
          {"/set-header?", GetAppropriateReportingHeader(), "&evicted"}));
  EXPECT_TRUE(content::NavigateToURL(contents, evicted_url));

  // Downgrade both page importance (set to IMPORTANT for active tabs on
  // Android) and visibility, then wait for the process launcher thread to drop
  // the child connection's STRONG/VISIBLE bindings.
  contents->SetPrimaryPageImportance(content::ChildProcessImportance::NORMAL,
                                     content::ChildProcessImportance::NORMAL);
  contents->WasHidden();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return contents->GetPrimaryMainFrame()
               ->GetProcess()
               ->GetEffectiveChildBindingState() <
           base::android::ChildBindingState::VISIBLE;
  }));

  content::RenderProcessHostWatcher watcher(
      contents, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_KILLED_BAD_MESSAGE);
  watcher.Wait();
  EXPECT_TRUE(watcher.did_exit_normally());

  // Bring the tab back to the foreground, navigate to a second
  // reporting-enabled page on the same endpoint, and trigger a foreground OOM
  // crash to flush the reporting pipeline without sleeping.
  contents->SetPrimaryPageImportance(content::ChildProcessImportance::IMPORTANT,
                                     content::ChildProcessImportance::NORMAL);
  contents->WasShown();
  GURL crashed_url = server()->GetURL(
      kReportingHost,
      base::StrCat(
          {"/set-header?", GetAppropriateReportingHeader(), "&crashed"}));
  EXPECT_TRUE(content::NavigateToURL(contents, crashed_url));

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

// Verify that an intentional browser-initiated shutdown with a normal exit code
// on Android does not generate a crash report even when the termination status
// is `base::TERMINATION_STATUS_OOM_PROTECTED`.
IN_PROC_BROWSER_TEST_P(AndroidCrashReportingBrowserTest,
                       NoCrashReportForBrowserInitiatedNormalShutdown) {
  content::WebContents* contents = GetActiveWebContents();

  GURL shutdown_url = server()->GetURL(
      kReportingHost,
      base::StrCat(
          {"/set-header?", GetAppropriateReportingHeader(), "&shutdown"}));
  EXPECT_TRUE(content::NavigateToURL(contents, shutdown_url));

  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  content::RenderProcessHostWatcher watcher(
      contents, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_NORMAL_EXIT);
  watcher.Wait();

  // Navigate to a second reporting-enabled page on the same endpoint and
  // trigger a real OOM crash to deterministically flush the reporting pipeline
  // without sleeping. Any report queued for `shutdown_url` would arrive before
  // or alongside `crashed_url`.
  GURL crashed_url = server()->GetURL(
      kReportingHost,
      base::StrCat(
          {"/set-header?", GetAppropriateReportingHeader(), "&crashed"}));
  EXPECT_TRUE(content::NavigateToURL(contents, crashed_url));

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

INSTANTIATE_TEST_SUITE_P(All,
                         AndroidCrashReportingBrowserTest,
                         ::testing::Bool());

}  // namespace

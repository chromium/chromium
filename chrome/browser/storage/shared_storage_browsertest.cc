// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/shared_storage_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_select_url_fenced_frame_config_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace storage {

namespace {

constexpr char kSimpleTestHost[] = "a.test";
constexpr char kSimplePagePath[] = "/simple.html";
constexpr char kCrossOriginHost[] = "b.test";
constexpr char kThirdOriginHost[] = "c.test";
constexpr char kFourthOriginHost[] = "d.test";
constexpr char kRemainingBudgetPrefix[] = "remaining budget: ";
constexpr char kErrorTypeHistogram[] =
    "Storage.SharedStorage.Worklet.Error.Type";
constexpr char kEntriesQueuedCountHistogram[] =
    "Storage.SharedStorage.AsyncIterator.EntriesQueuedCount";
constexpr char kReceivedEntriesBenchmarksHistogram[] =
    "Storage.SharedStorage.AsyncIterator.ReceivedEntriesBenchmarks";
constexpr char kIteratedEntriesBenchmarksHistogram[] =
    "Storage.SharedStorage.AsyncIterator.IteratedEntriesBenchmarks";
constexpr char kTimingDocumentAddModuleHistogram[] =
    "Storage.SharedStorage.Document.Timing.AddModule";
constexpr char kTimingDocumentRunHistogram[] =
    "Storage.SharedStorage.Document.Timing.Run";
constexpr char kTimingDocumentSelectUrlHistogram[] =
    "Storage.SharedStorage.Document.Timing.SelectURL";
constexpr char kTimingDocumentAppendHistogram[] =
    "Storage.SharedStorage.Document.Timing.Append";
constexpr char kTimingDocumentSetHistogram[] =
    "Storage.SharedStorage.Document.Timing.Set";
constexpr char kTimingDocumentDeleteHistogram[] =
    "Storage.SharedStorage.Document.Timing.Delete";
constexpr char kTimingDocumentClearHistogram[] =
    "Storage.SharedStorage.Document.Timing.Clear";
constexpr char kTimingWorkletAppendHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.Append";
constexpr char kTimingWorkletSetHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.Set";
constexpr char kTimingWorkletGetHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.Get";
constexpr char kTimingWorkletLengthHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.Length";
constexpr char kTimingWorkletDeleteHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.Delete";
constexpr char kTimingWorkletClearHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.Clear";
constexpr char kTimingWorkletKeysHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.Keys.Next";
constexpr char kTimingWorkletEntriesHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.Entries.Next";
constexpr char kWorkletNumPerPageHistogram[] =
    "Storage.SharedStorage.Worklet.NumPerPage";
constexpr char kTimingRemainingBudgetHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.RemainingBudget";

const double kBudgetAllowed = 5.0;

#if BUILDFLAG(IS_ANDROID)
base::FilePath GetChromeTestDataDir() {
  return base::FilePath(FILE_PATH_LITERAL("chrome/test/data"));
}
#endif

// With `WebContentsConsoleObserver`, we can only wait for the last message in a
// group.
base::RepeatingCallback<
    bool(const content::WebContentsConsoleObserver::Message& message)>
MakeFilter(std::vector<std::string> possible_last_messages) {
  return base::BindRepeating(
      [](std::vector<std::string> possible_last_messages,
         const content::WebContentsConsoleObserver::Message& message) {
        return base::Contains(possible_last_messages,
                              base::UTF16ToUTF8(message.message));
      },
      std::move(possible_last_messages));
}

std::string GetSharedStorageDisabledErrorMessage() {
  return base::StrCat({"a JavaScript error: \"Error: ",
                       content::GetSharedStorageDisabledMessage(), "\"\n"});
}

std::string GetSharedStorageSelectURLDisabledErrorMessage() {
  return base::StrCat({"a JavaScript error: \"Error: ",
                       content::GetSharedStorageSelectURLDisabledMessage(),
                       "\"\n"});
}

std::string GetSharedStorageAddModuleDisabledErrorMessage() {
  return base::StrCat({"a JavaScript error: \"Error: ",
                       content::GetSharedStorageAddModuleDisabledMessage(),
                       "\"\n"});
}

void DelayBy(base::TimeDelta delta) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), delta);
  run_loop.Run();
}

// TODO(cammie): Find a way to ensure that histograms are available at the
// necessary time without having to resort to sleeping/polling.
void WaitForHistograms(std::vector<std::string> histogram_names) {
  while (true) {
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    std::vector<std::string> still_waiting;
    for (const auto& name : histogram_names) {
      if (!base::StatisticsRecorder::FindHistogram(name))
        still_waiting.push_back(name);
    }

    histogram_names = std::move(still_waiting);

    if (histogram_names.empty())
      break;

    DelayBy(base::Seconds(1));
  }
}

// Return the active RenderFrameHost loaded in the last iframe in `parent_rfh`.
content::RenderFrameHost* LastChild(content::RenderFrameHost* parent_rfh) {
  int child_end = 0;
  while (ChildFrameAt(parent_rfh, child_end))
    child_end++;
  if (child_end == 0)
    return nullptr;
  return ChildFrameAt(parent_rfh, child_end - 1);
}

// Create an <iframe> inside `parent_rfh`, and navigate it toward `url`.
// This returns the new RenderFrameHost associated with new document created in
// the iframe.
content::RenderFrameHost* CreateIframe(content::RenderFrameHost* parent_rfh,
                                       const GURL& url) {
  EXPECT_EQ("iframe loaded",
            content::EvalJs(parent_rfh, content::JsReplace(R"(
    new Promise((resolve) => {
      const iframe = document.createElement("iframe");
      iframe.src = $1;
      iframe.onload = _ => { resolve("iframe loaded"); };
      document.body.appendChild(iframe);
    }))",
                                                           url)));
  return LastChild(parent_rfh);
}

}  // namespace

class SharedStorageChromeBrowserTestBase : public PlatformBrowserTest {
 public:
  SharedStorageChromeBrowserTestBase() {
    base::test::TaskEnvironment task_environment;

    // TODO(crbug.com/1378703): Update the tests to support Privacy Sandbox 4.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kSharedStorageAPI,
                              privacy_sandbox::kPrivacySandboxSettings3,
                              features::kPrivacySandboxAdsAPIsOverride},
        /*disabled_features=*/{privacy_sandbox::kPrivacySandboxSettings4});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    content::SetupCrossSiteRedirector(https_server());
    CHECK(https_server()->Start());

    InitPrefs();
  }

  ~SharedStorageChromeBrowserTestBase() override = default;

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  void SetPrefs(bool enable_privacy_sandbox, bool allow_third_party_cookies) {
    GetProfile()->GetPrefs()->SetBoolean(prefs::kPrivacySandboxApisEnabledV2,
                                         enable_privacy_sandbox);
    GetProfile()->GetPrefs()->SetBoolean(
        prefs::kPrivacySandboxManuallyControlledV2, enable_privacy_sandbox);

    GetProfile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            allow_third_party_cookies
                ? content_settings::CookieControlsMode::kOff
                : content_settings::CookieControlsMode::kBlockThirdParty));
  }

  // Virtual so derived classes can initialize differently. For the base class,
  // enables Privacy Sandbox and allows 3P cookies.
  virtual void InitPrefs() {
    SetPrefs(/*enable_privacy_sandbox=*/true,
             /*allow_third_party_cookies*/ true);
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  Profile* GetProfile() {
#if BUILDFLAG(IS_ANDROID)
    return TabModelList::models()[0]->GetProfile();
#else
    return browser()->profile();
#endif
  }

  void AddSimpleModule(const content::ToRenderFrameHost& execution_target) {
    content::WebContentsConsoleObserver add_module_console_observer(
        GetActiveWebContents());
    add_module_console_observer.SetFilter(
        MakeFilter({"Finish executing simple_module.js"}));

    std::string host =
        execution_target.render_frame_host()->GetLastCommittedOrigin().host();
    GURL module_script_url =
        https_server()->GetURL(host, "/shared_storage/simple_module.js");

    EXPECT_TRUE(content::ExecJs(
        execution_target,
        content::JsReplace("sharedStorage.worklet.addModule($1)",
                           module_script_url)));

    ASSERT_TRUE(add_module_console_observer.Wait());

    EXPECT_LE(1u,
              content::GetAttachedSharedStorageWorkletHostsCount(
                  execution_target.render_frame_host()->GetStoragePartition()));
    EXPECT_EQ(0u,
              content::GetKeepAliveSharedStorageWorkletHostsCount(
                  execution_target.render_frame_host()->GetStoragePartition()));
    EXPECT_EQ(1u, add_module_console_observer.messages().size());
    EXPECT_EQ(
        "Finish executing simple_module.js",
        base::UTF16ToUTF8(add_module_console_observer.messages()[0].message));
  }

  bool ExecuteScriptInWorklet(
      const content::ToRenderFrameHost& execution_target,
      const std::string& script,
      const std::string& last_script_message) {
    content::WebContentsConsoleObserver add_module_console_observer(
        GetActiveWebContents());
    add_module_console_observer.SetFilter(
        MakeFilter({"Finish executing customizable_module.js"}));

    base::StringPairs run_function_body_replacement;
    run_function_body_replacement.emplace_back("{{RUN_FUNCTION_BODY}}", script);

    std::string host =
        execution_target.render_frame_host()->GetLastCommittedOrigin().host();

    GURL module_script_url = https_server()->GetURL(
        host, net::test_server::GetFilePathWithReplacements(
                  "/shared_storage/customizable_module.js",
                  run_function_body_replacement));

    EXPECT_TRUE(content::ExecJs(
        execution_target,
        content::JsReplace("sharedStorage.worklet.addModule($1)",
                           module_script_url)));

    EXPECT_TRUE(add_module_console_observer.Wait());

    EXPECT_LE(1u,
              content::GetAttachedSharedStorageWorkletHostsCount(
                  execution_target.render_frame_host()->GetStoragePartition()));
    EXPECT_EQ(0u,
              content::GetKeepAliveSharedStorageWorkletHostsCount(
                  execution_target.render_frame_host()->GetStoragePartition()));
    EXPECT_EQ(1u, add_module_console_observer.messages().size());
    EXPECT_EQ(
        "Finish executing customizable_module.js",
        base::UTF16ToUTF8(add_module_console_observer.messages()[0].message));

    content::WebContentsConsoleObserver script_console_observer(
        GetActiveWebContents());
    script_console_observer.SetFilter(MakeFilter(
        {last_script_message, ExpectedSharedStorageDisabledMessage()}));

    content::EvalJsResult result = content::EvalJs(execution_target, R"(
        sharedStorage.run('test-operation');
      )");

    EXPECT_TRUE(script_console_observer.Wait());
    EXPECT_EQ(1u, script_console_observer.messages().size());

    EXPECT_EQ(last_script_message,
              base::UTF16ToUTF8(script_console_observer.messages()[0].message));

    return result.error.empty();
  }

  double RemainingBudget(const content::ToRenderFrameHost& execution_target,
                         bool should_add_module = false,
                         bool keep_alive_after_operation = true) {
    if (should_add_module)
      AddSimpleModule(execution_target);

    content::WebContentsConsoleObserver budget_console_observer(
        GetActiveWebContents());
    const std::string kRemainingBudgetPrefixStr(kRemainingBudgetPrefix);
    budget_console_observer.SetPattern(
        base::StrCat({kRemainingBudgetPrefixStr, "*"}));

    EXPECT_TRUE(ExecJs(execution_target,
                       content::JsReplace("window.keepWorklet = $1;",
                                          keep_alive_after_operation)));

    EXPECT_TRUE(ExecJs(execution_target, R"(
      sharedStorage.run('remaining-budget-operation', {keepAlive: keepWorklet});
    )"));

    bool observed = budget_console_observer.Wait();
    EXPECT_TRUE(observed);
    if (!observed) {
      return nan("");
    }

    EXPECT_EQ(1u, budget_console_observer.messages().size());
    std::string console_message =
        base::UTF16ToUTF8(budget_console_observer.messages()[0].message);
    EXPECT_TRUE(base::StartsWith(console_message, kRemainingBudgetPrefixStr));

    std::string result_string = console_message.substr(
        kRemainingBudgetPrefixStr.size(),
        console_message.size() - kRemainingBudgetPrefixStr.size());

    double result = 0.0;
    EXPECT_TRUE(base::StringToDouble(result_string, &result));

    return result;
  }

  virtual bool ResolveSelectURLToConfig() const { return false; }

  std::string ExpectedSharedStorageDisabledMessage() {
    return "Error: " + content::GetSharedStorageDisabledMessage();
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

class SharedStorageChromeBrowserTest
    : public SharedStorageChromeBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  SharedStorageChromeBrowserTest() {
    fenced_frame_api_change_feature_.InitWithFeatureState(
        blink::features::kFencedFramesAPIChanges, ResolveSelectURLToConfig());

    fenced_frame_feature_.InitAndEnableFeature(blink::features::kFencedFrames);
  }
  ~SharedStorageChromeBrowserTest() override = default;

  bool ResolveSelectURLToConfig() const override { return GetParam(); }

 private:
  base::test::ScopedFeatureList fenced_frame_api_change_feature_;
  base::test::ScopedFeatureList fenced_frame_feature_;
};

using SharedStorageChromeBrowserParams =
    std::tuple</*resolve_to_config=*/bool,
               /*enable_privacy_sandbox=*/bool,
               /*allow_third_party_cookies=*/bool>;

class SharedStoragePrefBrowserTest
    : public SharedStorageChromeBrowserTestBase,
      public testing::WithParamInterface<SharedStorageChromeBrowserParams> {
 public:
  SharedStoragePrefBrowserTest() {
    fenced_frame_api_change_feature_.InitWithFeatureState(
        blink::features::kFencedFramesAPIChanges, ResolveSelectURLToConfig());
    fenced_frame_feature_.InitAndEnableFeature(blink::features::kFencedFrames);
  }

  bool ResolveSelectURLToConfig() const override {
    return std::get<0>(GetParam());
  }
  bool EnablePrivacySandbox() const { return std::get<1>(GetParam()); }
  bool AllowThirdPartyCookies() const { return std::get<2>(GetParam()); }

  bool SuccessExpected() {
    return EnablePrivacySandbox() && AllowThirdPartyCookies();
  }

  // Sets prefs as parametrized.
  //
  // TODO(crbug.com/1396748): We may need to update how preferences are set once
  // the Privacy Sandbox settings release 4 is launched (crbug.com/1378703).
  void InitPrefs() override {
    SetPrefs(EnablePrivacySandbox(), AllowThirdPartyCookies());
  }

  void AddSimpleModuleWithPermissionBypassed(
      const content::ToRenderFrameHost& execution_target) {
    content::WebContentsConsoleObserver add_module_console_observer(
        GetActiveWebContents());
    add_module_console_observer.SetFilter(
        MakeFilter({"Finish executing simple_module.js"}));

    // We allow Shared Storage for `addModule()`.
    content::SetBypassIsSharedStorageAllowed(/*allow=*/true);

    EXPECT_TRUE(content::ExecJs(execution_target, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

    EXPECT_TRUE(add_module_console_observer.Wait());

    // Shared Storage is enabled in order to `addModule()`.
    EXPECT_EQ(1u, add_module_console_observer.messages().size());
    EXPECT_EQ(
        "Finish executing simple_module.js",
        base::UTF16ToUTF8(add_module_console_observer.messages()[0].message));

    content::SetBypassIsSharedStorageAllowed(/*allow=*/false);
  }

  bool ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      const content::ToRenderFrameHost& execution_target,
      const std::string& script,
      const std::string& last_script_message) {
    content::WebContentsConsoleObserver add_module_console_observer(
        GetActiveWebContents());
    add_module_console_observer.SetFilter(
        MakeFilter({"Finish executing customizable_module.js"}));

    base::StringPairs run_function_body_replacement;
    run_function_body_replacement.emplace_back("{{RUN_FUNCTION_BODY}}", script);

    std::string host =
        execution_target.render_frame_host()->GetLastCommittedOrigin().host();

    GURL module_script_url = https_server()->GetURL(
        host, net::test_server::GetFilePathWithReplacements(
                  "/shared_storage/customizable_module.js",
                  run_function_body_replacement));

    // We allow Shared Storage for `addModule()` and `run()`, but any operations
    // nested within the script run by `run()` will have preferences applied
    // according to test parameters. When the latter disallow Shared Storage, it
    // siumlates the situation where preferences are updated to block Shared
    // Storage during the course of a previously allowed `run()` call.
    content::SetBypassIsSharedStorageAllowed(/*allow=*/true);

    EXPECT_TRUE(content::ExecJs(
        execution_target,
        content::JsReplace("sharedStorage.worklet.addModule($1)",
                           module_script_url)));

    EXPECT_TRUE(add_module_console_observer.Wait());

    EXPECT_EQ(1u,
              content::GetAttachedSharedStorageWorkletHostsCount(
                  execution_target.render_frame_host()->GetStoragePartition()));
    EXPECT_EQ(0u,
              content::GetKeepAliveSharedStorageWorkletHostsCount(
                  execution_target.render_frame_host()->GetStoragePartition()));
    EXPECT_EQ(1u, add_module_console_observer.messages().size());
    EXPECT_EQ(
        "Finish executing customizable_module.js",
        base::UTF16ToUTF8(add_module_console_observer.messages()[0].message));

    WaitForHistograms({kTimingDocumentAddModuleHistogram});
    histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);

    content::WebContentsConsoleObserver script_console_observer(
        GetActiveWebContents());
    script_console_observer.SetFilter(MakeFilter(
        {last_script_message, ExpectedSharedStorageDisabledMessage()}));

    content::EvalJsResult result = content::EvalJs(execution_target, R"(
        sharedStorage.run('test-operation');
      )");

    EXPECT_TRUE(script_console_observer.Wait());
    EXPECT_EQ(1u, script_console_observer.messages().size());

    if (SuccessExpected()) {
      EXPECT_EQ(
          last_script_message,
          base::UTF16ToUTF8(script_console_observer.messages()[0].message));
    } else {
      EXPECT_EQ(
          ExpectedSharedStorageDisabledMessage(),
          base::UTF16ToUTF8(script_console_observer.messages()[0].message));
    }

    WaitForHistograms({kTimingDocumentRunHistogram});
    histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 1);

    content::SetBypassIsSharedStorageAllowed(/*allow=*/false);

    return result.error.empty();
  }

 private:
  base::test::ScopedFeatureList fenced_frame_api_change_feature_;
  base::test::ScopedFeatureList fenced_frame_feature_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SharedStoragePrefBrowserTest,
    testing::Combine(testing::Bool(), testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<SharedStoragePrefBrowserTest::ParamType>&
           info) {
      return base::StrCat(
          {"ResolveSelectURLTo", std::get<0>(info.param) ? "Config" : "URN",
           "_PrivacySandbox", std::get<1>(info.param) ? "Enabled" : "Disabled",
           "_3PCookies", std::get<2>(info.param) ? "Allowed" : "Blocked"});
    });

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, AddModule) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  content::WebContentsConsoleObserver console_observer(GetActiveWebContents());
  console_observer.SetFilter(MakeFilter({"Finish executing simple_module.js"}));

  content::EvalJsResult result = content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )");

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageAddModuleDisabledErrorMessage(), result.error);
    EXPECT_EQ(0u, console_observer.messages().size());

    WaitForHistograms({kErrorTypeHistogram});
    histogram_tester_.ExpectUniqueSample(
        kErrorTypeHistogram,
        blink::SharedStorageWorkletErrorType::kAddModuleWebVisible, 1);
    return;
  }

  ASSERT_TRUE(console_observer.Wait());

  // Privacy Sandbox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(result.error.empty());
  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kTimingDocumentAddModuleHistogram, kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, RunOperation) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  AddSimpleModuleWithPermissionBypassed(GetActiveWebContents());
  content::WebContentsConsoleObserver run_op_console_observer(
      GetActiveWebContents());
  run_op_console_observer.SetFilter(
      MakeFilter({"Finish executing \'test-operation\'"}));

  content::EvalJsResult run_op_result =
      content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )");

  WaitForHistograms({kTimingDocumentAddModuleHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageDisabledErrorMessage(), run_op_result.error);

    // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
    EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                       GURL(url::kAboutBlankURL)));
    WaitForHistograms({kErrorTypeHistogram, kWorkletNumPerPageHistogram});
    histogram_tester_.ExpectUniqueSample(
        kErrorTypeHistogram,
        blink::SharedStorageWorkletErrorType::kRunWebVisible, 1);
    histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
    return;
  }

  ASSERT_TRUE(run_op_console_observer.Wait());

  // Privacy Sandbox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(run_op_result.error.empty());
  EXPECT_EQ(1u, run_op_console_observer.messages().size());
  EXPECT_EQ("Finish executing \'test-operation\'",
            base::UTF16ToUTF8(run_op_console_observer.messages()[0].message));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram,
                     kTimingDocumentRunHistogram, kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, RunURLSelectionOperation) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  AddSimpleModuleWithPermissionBypassed(GetActiveWebContents());
  content::WebContentsConsoleObserver run_url_op_console_observer(
      GetActiveWebContents());
  run_url_op_console_observer.SetFilter(
      MakeFilter({"Finish executing \'test-url-selection-operation\'"}));

  EXPECT_TRUE(ExecJs(GetActiveWebContents(),
                     content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  // Construct and add the `TestSelectURLFencedFrameConfigObserver` to shared
  // storage worklet host manager.
  content::StoragePartition* storage_partition =
      content::ToRenderFrameHost(GetActiveWebContents())
          .render_frame_host()
          ->GetStoragePartition();
  content::TestSelectURLFencedFrameConfigObserver config_observer(
      storage_partition);
  content::EvalJsResult run_url_op_result = EvalJs(GetActiveWebContents(), R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation',
            [
              {
                url: "fenced_frames/title0.html"
              },
              {
                url: "fenced_frames/title1.html",
                reportingMetadata: {
                  "click": "fenced_frames/report1.html"
                }
              },
              {
                url: "fenced_frames/title2.html"
              }
            ],
            {
              data: {'mockResult': 1},
              resolveToConfig: resolveSelectURLToConfig
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )");

  WaitForHistograms({kTimingDocumentAddModuleHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageSelectURLDisabledErrorMessage(),
              run_url_op_result.error);

    // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
    EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                       GURL(url::kAboutBlankURL)));
    WaitForHistograms({kErrorTypeHistogram, kWorkletNumPerPageHistogram});
    histogram_tester_.ExpectUniqueSample(
        kErrorTypeHistogram,
        blink::SharedStorageWorkletErrorType::kSelectURLWebVisible, 1);
    histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
    return;
  }

  ASSERT_TRUE(run_url_op_console_observer.Wait());

  // Privacy Sandbox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(run_url_op_result.error.empty());
  absl::optional<GURL> observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));
  GURL urn_uuid = observed_urn_uuid.value();

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(run_url_op_result.ExtractString(), observed_urn_uuid->spec());
  }

  EXPECT_EQ(1u, run_url_op_console_observer.messages().size());
  EXPECT_EQ(
      "Finish executing \'test-url-selection-operation\'",
      base::UTF16ToUTF8(run_url_op_console_observer.messages()[0].message));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram,
                     kTimingDocumentSelectUrlHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentSelectUrlHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, Set) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  content::EvalJsResult set_result = content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.set('customKey', 'customValue');
    )");

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageDisabledErrorMessage(), set_result.error);
    return;
  }

  // Privacy Sandbox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(set_result.error.empty());

  WaitForHistograms({kTimingDocumentSetHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentSetHistogram, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, Append) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  content::EvalJsResult append_result =
      content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.append('customKey', 'customValue');
    )");

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageDisabledErrorMessage(), append_result.error);
    return;
  }

  // Privacy Sandbox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(append_result.error.empty());

  WaitForHistograms({kTimingDocumentAppendHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAppendHistogram, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, Delete) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  content::EvalJsResult delete_result =
      content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.delete('customKey');
    )");

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageDisabledErrorMessage(), delete_result.error);
    return;
  }

  // Privacy Sandbox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(delete_result.error.empty());

  WaitForHistograms({kTimingDocumentDeleteHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentDeleteHistogram, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, Clear) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  content::EvalJsResult clear_result =
      content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.clear();
    )");

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageDisabledErrorMessage(), clear_result.error);
    return;
  }

  // Privacy Sandbox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(clear_result.error.empty());

  WaitForHistograms({kTimingDocumentClearHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentClearHistogram, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletSet) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  // If `set()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), R"(
      console.log(await sharedStorage.set('key0', 'value0'));
      console.log('Finished script');
    )",
      "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingWorkletSetHistogram});
    histogram_tester_.ExpectTotalCount(kTimingWorkletSetHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletAppend) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  // If `append()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), R"(
      console.log(await sharedStorage.append('key0', 'value0'));
      console.log('Finished script');
    )",
      "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingWorkletAppendHistogram});
    histogram_tester_.ExpectTotalCount(kTimingWorkletAppendHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletDelete) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  // If `delete()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), R"(
      console.log(await sharedStorage.delete('key0'));
      console.log('Finished script');
    )",
      "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingWorkletDeleteHistogram});
    histogram_tester_.ExpectTotalCount(kTimingWorkletDeleteHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletClear) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  // If `clear()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), R"(
      console.log(await sharedStorage.clear());
      console.log('Finished script');
    )",
      "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingWorkletClearHistogram});
    histogram_tester_.ExpectTotalCount(kTimingWorkletClearHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletGet) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  // To prevent failure in the case where Shared Storage is enabled, we set a
  // key before retrieving it; but in the case here we expect failure, we test
  // only `get()` to isolate the behavior and determine if the promise is
  // rejected solely from that call.
  std::string script = SuccessExpected() ? R"(
      console.log(await sharedStorage.set('key0', 'value0'));
      console.log(await sharedStorage.get('key0'));
      console.log('Finished script');
    )"
                                         : R"(
      console.log(await sharedStorage.get('key0'));
      console.log('Finished script');
    )";

  // If `get()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), script, "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingWorkletSetHistogram, kTimingWorkletGetHistogram});
    histogram_tester_.ExpectTotalCount(kTimingWorkletSetHistogram, 1);
    histogram_tester_.ExpectTotalCount(kTimingWorkletGetHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletKeys) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  // If `keys()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), R"(
      for await (const key of sharedStorage.keys()) {
        console.log(key);
      }
      console.log('Finished script');
    )",
      "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingWorkletKeysHistogram});
    histogram_tester_.ExpectTotalCount(kTimingWorkletKeysHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletEntries) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  // If `entries()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), R"(
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
      console.log('Finished script');
    )",
      "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingWorkletEntriesHistogram});
    histogram_tester_.ExpectTotalCount(kTimingWorkletEntriesHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletLength) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  // If `length()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), R"(
      console.log(await sharedStorage.length());
      console.log('Finished script');
    )",
      "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingWorkletLengthHistogram});
    histogram_tester_.ExpectTotalCount(kTimingWorkletLengthHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletRemainingBudget) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  // If `remainingBudget()` fails due to Shared Storage being disabled, there
  // will be a console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), R"(
      console.log(await sharedStorage.remainingBudget());
      console.log('Finished script');
    )",
      "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingRemainingBudgetHistogram});
    histogram_tester_.ExpectTotalCount(kTimingRemainingBudgetHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       WorkletKeysEntries_AllIterated) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      for (let i = 0; i < 150; ++i) {
        sharedStorage.set('key' + i.toString().padStart(3, '0'),
                          'value' + i.toString().padStart(3, '0'));
      }
      for await (const key of sharedStorage.keys()) {
        console.log(key);
      }
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kWorkletNumPerPageHistogram, kTimingDocumentAddModuleHistogram,
       kTimingDocumentRunHistogram, kTimingWorkletKeysHistogram,
       kTimingWorkletEntriesHistogram, kEntriesQueuedCountHistogram,
       kReceivedEntriesBenchmarksHistogram,
       kIteratedEntriesBenchmarksHistogram});

  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingWorkletKeysHistogram, 151);
  histogram_tester_.ExpectTotalCount(kTimingWorkletEntriesHistogram, 151);
  histogram_tester_.ExpectUniqueSample(kEntriesQueuedCountHistogram, 150, 2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 0,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 10,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 20,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 30,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 40,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 50,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 60,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 70,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 80,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 90,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 100,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 0,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 10,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 20,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 30,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 40,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 50,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 60,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 70,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 80,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 90,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 100,
                                      2);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       WorkletKeysEntries_PartiallyIterated) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      for (let i = 0; i < 300; ++i) {
        sharedStorage.set('key' + i.toString().padStart(3, '0'),
                          'value' + i.toString().padStart(3, '0'));
      }
      var keys = sharedStorage.keys();
      for (let i = 0; i < 150; ++i) {
        let key_dict = await keys.next();
        console.log(key_dict['value']);
      }
      var entries = sharedStorage.entries();
      for (let i = 0; i < 101; ++i) {
        let entry_dict = await entries.next();
        console.log(entry_dict['value']);
      }
      var keys2 = sharedStorage.keys();
      for (let i = 0; i < 243; ++i) {
        let key_dict = await keys2.next();
        console.log(key_dict['value']);
      }
      var entries = sharedStorage.entries();
      for (let i = 0; i < 299; ++i) {
        let entry_dict = await entries.next();
        console.log(entry_dict['value']);
      }
      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kWorkletNumPerPageHistogram, kTimingDocumentAddModuleHistogram,
       kTimingDocumentRunHistogram, kTimingWorkletKeysHistogram,
       kTimingWorkletEntriesHistogram, kEntriesQueuedCountHistogram,
       kReceivedEntriesBenchmarksHistogram,
       kIteratedEntriesBenchmarksHistogram});

  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingWorkletKeysHistogram, 150 + 243);
  histogram_tester_.ExpectTotalCount(kTimingWorkletEntriesHistogram, 101 + 299);
  histogram_tester_.ExpectUniqueSample(kEntriesQueuedCountHistogram, 300, 4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 0,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 10,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 20,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 30,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 40,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 50,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 60,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 70,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 80,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 90,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 100,
                                      4);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 0,
                                      4);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 10,
                                      4);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 20,
                                      4);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 30,
                                      4);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 40,
                                      3);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 50,
                                      3);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 60,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 70,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 80,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 90,
                                      1);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 100,
                                      0);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       WorkletKeysEntries_AllIteratedLessThanTenKeys) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      for (let i = 0; i < 5; ++i) {
        sharedStorage.set('key' + i.toString().padStart(3, '0'),
                          'value' + i.toString().padStart(3, '0'));
      }
      for await (const key of sharedStorage.keys()) {
        console.log(key);
      }
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kWorkletNumPerPageHistogram, kTimingDocumentAddModuleHistogram,
       kTimingDocumentRunHistogram, kTimingWorkletKeysHistogram,
       kTimingWorkletEntriesHistogram, kEntriesQueuedCountHistogram,
       kReceivedEntriesBenchmarksHistogram,
       kIteratedEntriesBenchmarksHistogram});

  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingWorkletKeysHistogram, 6);
  histogram_tester_.ExpectTotalCount(kTimingWorkletEntriesHistogram, 6);
  histogram_tester_.ExpectUniqueSample(kEntriesQueuedCountHistogram, 5, 2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 0,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 10,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 20,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 30,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 40,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 50,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 60,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 70,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 80,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 90,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 100,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 0,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 10,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 20,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 30,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 40,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 50,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 60,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 70,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 80,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 90,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 100,
                                      2);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       WorkletKeysEntries_PartiallyIteratedLessThanTenKeys) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      for (let i = 0; i < 5; ++i) {
        sharedStorage.set('key' + i.toString().padStart(3, '0'),
                          'value' + i.toString().padStart(3, '0'));
      }
      var keys = sharedStorage.keys();
      for (let i = 0; i < 4; ++i) {
        let key_dict = await keys.next();
        console.log(key_dict['value']);
      }
      var entries = sharedStorage.entries();
      for (let i = 0; i < 2; ++i) {
        let entry_dict = await entries.next();
        console.log(entry_dict['value']);
      }
      var keys2 = sharedStorage.keys();
      for (let i = 0; i < 3; ++i) {
        let key_dict = await keys2.next();
        console.log(key_dict['value']);
      }
      var entries = sharedStorage.entries();
      for (let i = 0; i < 1; ++i) {
        let entry_dict = await entries.next();
        console.log(entry_dict['value']);
      }
      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kWorkletNumPerPageHistogram, kTimingDocumentAddModuleHistogram,
       kTimingDocumentRunHistogram, kTimingWorkletKeysHistogram,
       kTimingWorkletEntriesHistogram, kEntriesQueuedCountHistogram,
       kReceivedEntriesBenchmarksHistogram,
       kIteratedEntriesBenchmarksHistogram});

  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingWorkletKeysHistogram, 4 + 3);
  histogram_tester_.ExpectTotalCount(kTimingWorkletEntriesHistogram, 2 + 1);
  histogram_tester_.ExpectUniqueSample(kEntriesQueuedCountHistogram, 5, 4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 0,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 10,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 20,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 30,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 40,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 50,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 60,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 70,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 80,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 90,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 100,
                                      4);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 0,
                                      4);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 10,
                                      4);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 20,
                                      4);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 30,
                                      3);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 40,
                                      3);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 50,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 60,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 70,
                                      1);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 80,
                                      1);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 90,
                                      0);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       WorkletKeysEntries_AllIteratedNoKeys) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      sharedStorage.set('key', 'value');
      sharedStorage.delete('key');
      for await (const key of sharedStorage.keys()) {
        console.log(key);
      }
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kWorkletNumPerPageHistogram, kTimingDocumentAddModuleHistogram,
       kTimingDocumentRunHistogram, kTimingWorkletKeysHistogram,
       kTimingWorkletEntriesHistogram, kEntriesQueuedCountHistogram,
       kReceivedEntriesBenchmarksHistogram,
       kIteratedEntriesBenchmarksHistogram});

  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingWorkletKeysHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingWorkletEntriesHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kEntriesQueuedCountHistogram, 0, 2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 0,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 10,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 20,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 30,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 40,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 50,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 60,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 70,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 80,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 90,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 100,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 0,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 10,
                                      0);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       AddModule_InvalidScriptUrlError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  std::string invalid_url = "http://#";
  content::EvalJsResult result = content::EvalJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", invalid_url));

  EXPECT_EQ(
      base::StrCat(
          {"a JavaScript error: \"Error: The module script url is invalid.\n",
           "    at __const_std::string&_script__:1:24):\n",
           "        {sharedStorage.worklet.addModule(\"", invalid_url, "\")\n",
           "                               ^^^^^\n"}),
      result.error);

  WaitForHistograms({kErrorTypeHistogram});
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kAddModuleWebVisible, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       AddModule_CrossOriginScriptError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  GURL script_url = https_server()->GetURL(kCrossOriginHost,
                                           "/shared_storage/simple_module.js");
  content::EvalJsResult result = content::EvalJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url));

  EXPECT_EQ(
      base::StrCat({"a JavaScript error: \"Error: Only same origin module ",
                    "script is allowed.",
                    "\n    at __const_std::string&_script__:1:24):\n        ",
                    "{sharedStorage.worklet.addModule(\"",
                    script_url.spec().substr(0, 38),
                    "\n                               ^^^^^\n"}),
      result.error);

  WaitForHistograms({kErrorTypeHistogram});
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kAddModuleWebVisible, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       AddModule_LoadFailureError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  GURL script_url = https_server()->GetURL(
      kSimpleTestHost, "/shared_storage/nonexistent_module.js");
  content::EvalJsResult result = content::EvalJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url));

  EXPECT_EQ(
      base::StrCat({"a JavaScript error: \"Error: Failed to load ",
                    script_url.spec(), " HTTP status = 404 Not Found.\"\n"}),
      result.error);

  WaitForHistograms({kErrorTypeHistogram});
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kAddModuleWebVisible, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       AddModule_UnexpectedRedirectError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  GURL script_url = https_server()->GetURL(
      kSimpleTestHost, "/server-redirect?shared_storage/simple_module.js");
  content::EvalJsResult result = content::EvalJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url));

  EXPECT_EQ(
      base::StrCat({"a JavaScript error: \"Error: Unexpected redirect on ",
                    script_url.spec(), ".\"\n"}),
      result.error);

  WaitForHistograms({kErrorTypeHistogram});
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kAddModuleWebVisible, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       AddModule_EmptyResultError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  GURL script_url = https_server()->GetURL(
      kSimpleTestHost, "/shared_storage/erroneous_module.js");
  content::EvalJsResult result = content::EvalJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url));

  EXPECT_THAT(
      result.error,
      testing::HasSubstr("ReferenceError: undefinedVariable is not defined"));

  WaitForHistograms({kErrorTypeHistogram});
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kAddModuleWebVisible, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       AddModule_MultipleAddModuleError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  GURL script_url = https_server()->GetURL(kSimpleTestHost,
                                           "/shared_storage/simple_module.js");

  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));
  content::EvalJsResult result = content::EvalJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url));

  EXPECT_EQ(base::StrCat({"a JavaScript error: \"Error: ",
                          "sharedStorage.worklet.addModule() can only ",
                          "be invoked once per browsing context.\"\n"}),
            result.error);

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kAddModuleWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, Run_NotLoadedError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(),
                              R"(
      sharedStorage.run(
          'test-operation', {data: {}});
    )"));

  WaitForHistograms({kErrorTypeHistogram});
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kRunNonWebVisible, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, Run_NotRegisteredError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  GURL script_url = https_server()->GetURL(kSimpleTestHost,
                                           "/shared_storage/simple_module.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(),
                              R"(
      sharedStorage.run(
          'test-operation-1', {data: {}});
    )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kRunNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, Run_FunctionError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  GURL script_url = https_server()->GetURL(
      kSimpleTestHost, "/shared_storage/erroneous_module2.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(),
                              R"(
      sharedStorage.run(
          'test-operation', {data: {}});
    )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kRunNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, Run_ScriptError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  GURL script_url = https_server()->GetURL(
      kSimpleTestHost, "/shared_storage/erroneous_module4.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(),
                              R"(
      sharedStorage.run(
          'test-operation', {data: {}});
    )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kRunNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       Run_UnexpectedCustomDataError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  GURL script_url = https_server()->GetURL(
      kSimpleTestHost, "/shared_storage/erroneous_module5.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(),
                              R"(
      sharedStorage.run(
          'test-operation', {data: {'customField': 'customValue123'}});
    )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kRunNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       SelectUrl_NotLoadedError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  EXPECT_TRUE(ExecJs(GetActiveWebContents(),
                     content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));
  content::EvalJsResult result = EvalJs(GetActiveWebContents(), R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation-1',
            [
              {
                url: "fenced_frames/title0.html"
              }
            ],
            {
              data: {},
              resolveToConfig: resolveSelectURLToConfig
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )");

  EXPECT_EQ(base::StrCat({"a JavaScript error: \"Error: ",
                          "sharedStorage.worklet.addModule() has to be ",
                          "called before sharedStorage.selectURL().\"\n"}),
            result.error);

  WaitForHistograms({kErrorTypeHistogram});

  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kSelectURLWebVisible, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       SelectUrl_NotRegisteredError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  GURL script_url = https_server()->GetURL(kSimpleTestHost,
                                           "/shared_storage/simple_module.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(ExecJs(GetActiveWebContents(),
                     content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation-1',
            [
              {
                url: "fenced_frames/title0.html"
              }
            ],
            {
              data: {},
              resolveToConfig: resolveSelectURLToConfig
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       SelectUrl_FunctionError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  GURL script_url = https_server()->GetURL(
      kSimpleTestHost, "/shared_storage/erroneous_module2.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(ExecJs(GetActiveWebContents(),
                     content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation',
            [
              {
                url: "fenced_frames/title0.html"
              }
            ],
            {
              data: {},
              resolveToConfig: resolveSelectURLToConfig
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, SelectUrl_ScriptError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  GURL script_url = https_server()->GetURL(
      kSimpleTestHost, "/shared_storage/erroneous_module4.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(ExecJs(GetActiveWebContents(),
                     content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation',
            [
              {
                url: "fenced_frames/title0.html"
              }
            ],
            {
              data: {},
              resolveToConfig: resolveSelectURLToConfig
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       SelectUrl_UnexpectedCustomDataError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  GURL script_url = https_server()->GetURL(
      kSimpleTestHost, "/shared_storage/erroneous_module5.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(ExecJs(GetActiveWebContents(),
                     content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation',
            [
              {
                url: "fenced_frames/title0.html"
              }
            ],
            {
              data: {'customField': 'customValue123'},
              resolveToConfig: resolveSelectURLToConfig
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       SelectUrl_OutOfRangeError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  GURL script_url = https_server()->GetURL(
      kSimpleTestHost, "/shared_storage/erroneous_module6.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(ExecJs(GetActiveWebContents(),
                     content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation-1',
            [
              {
                url: "fenced_frames/title0.html"
              }
            ],
            {
              data: {},
              resolveToConfig: resolveSelectURLToConfig
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       SelectUrl_ReturnValueToIntError) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  GURL script_url = https_server()->GetURL(
      kSimpleTestHost, "/shared_storage/erroneous_module6.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(ExecJs(GetActiveWebContents(),
                     content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation-2',
            [
              {
                url: "fenced_frames/title0.html"
              }
            ],
            {
              data: {},
              resolveToConfig: resolveSelectURLToConfig
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, DocumentTiming) {
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Seconds(60));

  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(),
                              R"(
      sharedStorage.set('key0', 'value0');

      sharedStorage.set('key1', 'value1');
      sharedStorage.set('key1', 'value111');

      sharedStorage.set('key2', 'value2');
      sharedStorage.set('key2', 'value222', {ignoreIfPresent: true});

      sharedStorage.set('key3', 'value3');
      sharedStorage.append('key3', 'value333');
      sharedStorage.append('key2', 'value22');
      sharedStorage.append('key4', 'value4');

      sharedStorage.delete('key0');
      sharedStorage.delete('key2');
      sharedStorage.clear();
    )"));

  WaitForHistograms(
      {kTimingDocumentSetHistogram, kTimingDocumentAppendHistogram,
       kTimingDocumentDeleteHistogram, kTimingDocumentClearHistogram});

  histogram_tester_.ExpectTotalCount(kTimingDocumentSetHistogram, 6);
  histogram_tester_.ExpectTotalCount(kTimingDocumentAppendHistogram, 3);
  histogram_tester_.ExpectTotalCount(kTimingDocumentDeleteHistogram, 2);
  histogram_tester_.ExpectTotalCount(kTimingDocumentClearHistogram, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, WorkletTiming) {
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Seconds(60));

  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(),
                                     R"(
      sharedStorage.set('key0', 'value0');

      sharedStorage.set('key1', 'value1');
      sharedStorage.set('key1', 'value111');

      sharedStorage.set('key2', 'value2');
      sharedStorage.set('key2', 'value222', {ignoreIfPresent: true});

      sharedStorage.set('key3', 'value3');
      sharedStorage.append('key3', 'value333');
      sharedStorage.append('key2', 'value22');
      sharedStorage.append('key4', 'value4');

      console.log(await sharedStorage.get('key0'));
      console.log(await sharedStorage.get('key1'));
      console.log(await sharedStorage.get('key2'));
      console.log(await sharedStorage.get('key3'));
      console.log(await sharedStorage.get('key4'));
      console.log(await sharedStorage.length());

      sharedStorage.delete('key0');
      sharedStorage.delete('key2');

      // It's necessary to `await` this finally promise, since we are not
      // using the option `keepAlive: true` in the `run()` call. The worklet
      // will be closed by the browser once the worklet signals to the browser
      // that the `run()` call has finished.
      await sharedStorage.clear();

      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kCrossOriginHost, kSimplePagePath)));
  WaitForHistograms(
      {kTimingDocumentAddModuleHistogram, kTimingDocumentRunHistogram,
       kTimingWorkletSetHistogram, kTimingWorkletAppendHistogram,
       kTimingWorkletGetHistogram, kTimingWorkletLengthHistogram,
       kTimingWorkletDeleteHistogram, kTimingWorkletClearHistogram,
       kWorkletNumPerPageHistogram});

  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingWorkletSetHistogram, 6);
  histogram_tester_.ExpectTotalCount(kTimingWorkletAppendHistogram, 3);
  histogram_tester_.ExpectTotalCount(kTimingWorkletGetHistogram, 5);
  histogram_tester_.ExpectTotalCount(kTimingWorkletLengthHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingWorkletDeleteHistogram, 2);
  histogram_tester_.ExpectTotalCount(kTimingWorkletClearHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

// Flaky: https://crbug.com/1406845
IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       DISABLED_WorkletNumPerPage_Two) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  content::RenderFrameHost* main_frame =
      GetActiveWebContents()->GetPrimaryMainFrame();

  EXPECT_TRUE(ExecuteScriptInWorklet(main_frame,
                                     R"(
      sharedStorage.set('key0', 'value0');
      console.log('Finished script');
    )",
                                     "Finished script"));

  content::RenderFrameHost* iframe = CreateIframe(
      main_frame, https_server()->GetURL(kCrossOriginHost, kSimplePagePath));

  EXPECT_TRUE(ExecuteScriptInWorklet(iframe,
                                     R"(
      sharedStorage.set('key0', 'value0');
      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram,
                     kTimingDocumentRunHistogram, kTimingWorkletSetHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 2);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 2);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 2, 1);
  EXPECT_LE(1u,
            histogram_tester_.GetAllSamples(kTimingWorkletSetHistogram).size());
}

// Flaky: https://crbug.com/1406845
IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       DISABLED_WorkletNumPerPage_Three) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kSimpleTestHost, kSimplePagePath)));

  content::RenderFrameHost* main_frame =
      GetActiveWebContents()->GetPrimaryMainFrame();

  EXPECT_TRUE(ExecuteScriptInWorklet(main_frame,
                                     R"(
      sharedStorage.set('key0', 'value0');
      console.log('Finished script');
    )",
                                     "Finished script"));

  content::RenderFrameHost* iframe = CreateIframe(
      main_frame, https_server()->GetURL(kCrossOriginHost, kSimplePagePath));

  EXPECT_TRUE(ExecuteScriptInWorklet(iframe,
                                     R"(
      sharedStorage.set('key0', 'value0');
      console.log('Finished script');
    )",
                                     "Finished script"));

  content::RenderFrameHost* nested_iframe = CreateIframe(
      iframe, https_server()->GetURL(kThirdOriginHost, kSimplePagePath));

  EXPECT_TRUE(ExecuteScriptInWorklet(nested_iframe,
                                     R"(
      sharedStorage.set('key0', 'value0');
      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram,
                     kTimingDocumentRunHistogram, kTimingWorkletSetHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 3);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 3);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 3, 1);
  EXPECT_LE(1u,
            histogram_tester_.GetAllSamples(kTimingWorkletSetHistogram).size());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageChromeBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<
                             SharedStorageChromeBrowserTest::ParamType>& info) {
                           return base::StrCat({"ResolveSelectURLTo",
                                                info.param ? "Config" : "URN"});
                         });

class SharedStorageFencedFrameChromeBrowserTest
    : public SharedStorageChromeBrowserTestBase {
 public:
  SharedStorageFencedFrameChromeBrowserTest() {
    base::test::TaskEnvironment task_environment;

    shared_storage_feature_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{blink::features::kSharedStorageAPI,
          {{"SharedStorageBitBudget", base::NumberToString(kBudgetAllowed)}}}},
        /*disabled_features=*/{});

    fenced_frame_api_change_feature_.InitAndEnableFeature(
        blink::features::kFencedFramesAPIChanges);

    fenced_frame_feature_.InitAndEnableFeature(blink::features::kFencedFrames);
  }

  ~SharedStorageFencedFrameChromeBrowserTest() override = default;

  bool ResolveSelectURLToConfig() const override { return true; }

  content::RenderFrameHost* SelectURLAndCreateFencedFrame(
      content::RenderFrameHost* render_frame_host,
      bool should_add_module = true,
      bool keep_alive_after_operation = true) {
    if (should_add_module)
      AddSimpleModule(render_frame_host);

    content::WebContentsConsoleObserver run_url_op_console_observer(
        GetActiveWebContents());
    run_url_op_console_observer.SetFilter(
        MakeFilter({"Finish executing \'test-url-selection-operation\'"}));

    EXPECT_TRUE(
        ExecJs(render_frame_host,
               content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                  ResolveSelectURLToConfig())));

    EXPECT_TRUE(ExecJs(render_frame_host,
                       content::JsReplace("window.keepWorklet = $1;",
                                          keep_alive_after_operation)));

    // Construct and add the `TestSelectURLFencedFrameConfigObserver` to shared
    // storage worklet host manager.
    content::StoragePartition* storage_partition =
        content::ToRenderFrameHost(GetActiveWebContents())
            .render_frame_host()
            ->GetStoragePartition();
    content::TestSelectURLFencedFrameConfigObserver config_observer(
        storage_partition);
    content::EvalJsResult run_url_op_result = EvalJs(render_frame_host, R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation',
            [
              {
                url: "fenced_frames/title0.html"
              },
              {
                url: "fenced_frames/title1.html",
                reportingMetadata:
                {
                  "click": "fenced_frames/report1.html"
                }
              },
              {
                url: "fenced_frames/title2.html"
              }
            ],
            {
              data: {'mockResult': 1},
              resolveToConfig: resolveSelectURLToConfig,
              keepAlive: keepWorklet
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )");

    EXPECT_TRUE(run_url_op_console_observer.Wait());
    EXPECT_TRUE(run_url_op_result.error.empty());
    const absl::optional<GURL>& observed_urn_uuid =
        config_observer.GetUrnUuid();
    EXPECT_TRUE(observed_urn_uuid.has_value());
    EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

    if (!ResolveSelectURLToConfig()) {
      EXPECT_EQ(run_url_op_result.ExtractString(), observed_urn_uuid->spec());
    }

    EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));
    EXPECT_EQ(1u, run_url_op_console_observer.messages().size());
    EXPECT_EQ(
        "Finish executing \'test-url-selection-operation\'",
        base::UTF16ToUTF8(run_url_op_console_observer.messages()[0].message));

    return content::CreateFencedFrame(
        render_frame_host,
        ResolveSelectURLToConfig()
            ? content::FencedFrameNavigationTarget("select_url_result")
            : content::FencedFrameNavigationTarget(observed_urn_uuid.value()));
  }

 private:
  base::test::ScopedFeatureList shared_storage_feature_;
  base::test::ScopedFeatureList fenced_frame_api_change_feature_;
  base::test::ScopedFeatureList fenced_frame_feature_;
};

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameChromeBrowserTest,
                       FencedFrameNavigateTop_BudgetWithdrawal) {
  GURL main_url = https_server()->GetURL(kSimpleTestHost, kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(GetActiveWebContents(), main_url));

  GURL iframe_url = https_server()->GetURL(kCrossOriginHost, kSimplePagePath);
  content::RenderFrameHost* iframe =
      CreateIframe(GetActiveWebContents()->GetPrimaryMainFrame(), iframe_url);

  content::RenderFrameHost* fenced_frame_root_node =
      SelectURLAndCreateFencedFrame(iframe);
  EXPECT_DOUBLE_EQ(RemainingBudget(iframe), kBudgetAllowed);

  GURL new_page_url = https_server()->GetURL(kThirdOriginHost, kSimplePagePath);

  content::TestNavigationObserver top_navigation_observer(
      GetActiveWebContents());
  EXPECT_TRUE(ExecJs(
      fenced_frame_root_node,
      content::JsReplace("window.open($1, '_unfencedTop')", new_page_url)));
  top_navigation_observer.Wait();

  content::RenderFrameHost* new_iframe =
      CreateIframe(GetActiveWebContents()->GetPrimaryMainFrame(), iframe_url);

  // After the top navigation, log(3) bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(RemainingBudget(new_iframe, /*should_add_module=*/true),
                   kBudgetAllowed - std::log2(3));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kTimingDocumentAddModuleHistogram, kTimingDocumentSelectUrlHistogram,
       kTimingDocumentRunHistogram, kTimingRemainingBudgetHistogram,
       kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 2);
  histogram_tester_.ExpectTotalCount(kTimingDocumentSelectUrlHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 2);
  histogram_tester_.ExpectTotalCount(kTimingRemainingBudgetHistogram, 2);

  // In the MPArch case, some additional pageloads with worklet count 0 are
  // recorded, so we do not use `ExpectUniqueSample()` here.
  histogram_tester_.ExpectBucketCount(kWorkletNumPerPageHistogram, 1, 2);
  EXPECT_EQ(2, histogram_tester_.GetTotalSum(kWorkletNumPerPageHistogram));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageFencedFrameChromeBrowserTest,
    TwoFencedFrames_DifferentURNs_EachNavigateOnce_BudgetWithdrawalTwice) {
  GURL main_url = https_server()->GetURL(kSimpleTestHost, kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(GetActiveWebContents(), main_url));

  GURL iframe_url = https_server()->GetURL(kCrossOriginHost, kSimplePagePath);
  content::RenderFrameHost* iframe1 =
      CreateIframe(GetActiveWebContents()->GetPrimaryMainFrame(), iframe_url);

  content::RenderFrameHost* fenced_frame_root_node1 =
      SelectURLAndCreateFencedFrame(iframe1);
  EXPECT_DOUBLE_EQ(RemainingBudget(iframe1), kBudgetAllowed);

  GURL new_page_url1 =
      https_server()->GetURL(kThirdOriginHost, kSimplePagePath);

  content::TestNavigationObserver top_navigation_observer1(
      GetActiveWebContents());
  EXPECT_TRUE(ExecJs(
      fenced_frame_root_node1,
      content::JsReplace("window.open($1, '_unfencedTop')", new_page_url1)));
  top_navigation_observer1.Wait();

  content::RenderFrameHost* iframe2 =
      CreateIframe(GetActiveWebContents()->GetPrimaryMainFrame(), iframe_url);

  // After the top navigation, log(3) bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(RemainingBudget(iframe2, /*should_add_module=*/true),
                   kBudgetAllowed - std::log2(3));

  content::RenderFrameHost* fenced_frame_root_node2 =
      SelectURLAndCreateFencedFrame(iframe2, /*should_add_module=*/false);
  EXPECT_DOUBLE_EQ(RemainingBudget(iframe2), kBudgetAllowed - std::log2(3));

  GURL new_page_url2 =
      https_server()->GetURL(kFourthOriginHost, kSimplePagePath);

  content::TestNavigationObserver top_navigation_observer2(
      GetActiveWebContents());
  EXPECT_TRUE(ExecJs(
      fenced_frame_root_node2,
      content::JsReplace("window.open($1, '_unfencedTop')", new_page_url2)));
  top_navigation_observer2.Wait();

  content::RenderFrameHost* iframe3 =
      CreateIframe(GetActiveWebContents()->GetPrimaryMainFrame(), iframe_url);

  // After the top navigation, another log(3) bits should have been withdrawn
  // from the original shared storage origin.
  EXPECT_DOUBLE_EQ(RemainingBudget(iframe3, /*should_add_module=*/true),
                   kBudgetAllowed - std::log2(3) - std::log2(3));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kTimingDocumentAddModuleHistogram, kTimingDocumentSelectUrlHistogram,
       kTimingDocumentRunHistogram, kTimingRemainingBudgetHistogram,
       kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 3);
  histogram_tester_.ExpectTotalCount(kTimingDocumentSelectUrlHistogram, 2);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 4);
  histogram_tester_.ExpectTotalCount(kTimingRemainingBudgetHistogram, 4);

  // In the MPArch case, some additional pageloads with worklet count 0 are
  // recorded, so we do not use `ExpectUniqueSample()` here.
  histogram_tester_.ExpectBucketCount(kWorkletNumPerPageHistogram, 1, 3);
  EXPECT_EQ(3, histogram_tester_.GetTotalSum(kWorkletNumPerPageHistogram));
}

}  // namespace storage

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/shared_storage_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"

namespace storage {

namespace {

constexpr char kSimpleTestHost[] = "a.test";
constexpr char kSimplePagePath[] = "/simple.html";

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
  return base::StrCat({"a JavaScript error:\nError: ",
                       content::GetSharedStorageDisabledMessage(), "\n"});
}

}  // namespace

struct SharedStorageChromeBrowserParams {
  bool enable_privacy_sandbox;
  bool allow_third_party_cookies;
};

// Used by `testing::PrintToStringParamName()`.
std::string PrintToString(const SharedStorageChromeBrowserParams& p) {
  return base::StrCat(
      {"PrivacySandbox", p.enable_privacy_sandbox ? "Enabled" : "Disabled",
       "_3PCookies", p.allow_third_party_cookies ? "Allowed" : "Blocked"});
}

std::vector<SharedStorageChromeBrowserParams>
GetSharedStorageChromeBrowserParams() {
  return std::vector<SharedStorageChromeBrowserParams>(
      {{true, true}, {true, false}, {false, true}, {false, false}});
}

class SharedStorageChromeBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<SharedStorageChromeBrowserParams> {
 public:
  SharedStorageChromeBrowserTest() {
    base::test::TaskEnvironment task_environment;

    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kSharedStorageAPI,
                              privacy_sandbox::kPrivacySandboxSettings3,
                              features::kPrivacySandboxAdsAPIsOverride},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    content::SetupCrossSiteRedirector(https_server());
    CHECK(https_server()->Start());

    SetPrefsAsParametrized();
  }

  ~SharedStorageChromeBrowserTest() override = default;

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  bool SuccessExpected() {
    return GetParam().enable_privacy_sandbox &&
           GetParam().allow_third_party_cookies;
  }

  void SetPrefs(bool enable_privacy_sandbox, bool allow_third_party_cookies) {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kPrivacySandboxApisEnabledV2, enable_privacy_sandbox);
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kPrivacySandboxManuallyControlledV2, enable_privacy_sandbox);

    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            allow_third_party_cookies
                ? content_settings::CookieControlsMode::kOff
                : content_settings::CookieControlsMode::kBlockThirdParty));
  }

  void SetPrefsAsParametrized() {
    SetPrefs(GetParam().enable_privacy_sandbox,
             GetParam().allow_third_party_cookies);
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
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

    add_module_console_observer.Wait();

    // Shared Storage is enabled in order to `addModule()`.
    EXPECT_EQ(1u, add_module_console_observer.messages().size());
    EXPECT_EQ(
        "Finish executing simple_module.js",
        base::UTF16ToUTF8(add_module_console_observer.messages()[0].message));

    content::SetBypassIsSharedStorageAllowed(/*allow=*/false);
  }

  bool ExecuteScriptInWorklet(
      const content::ToRenderFrameHost& execution_target,
      const std::string& script,
      const std::string& last_script_message) {
    base::StringPairs run_function_body_replacement;
    run_function_body_replacement.push_back(
        std::make_pair("{{RUN_FUNCTION_BODY}}", script));

    std::string host =
        execution_target.render_frame_host()->GetLastCommittedOrigin().host();

    content::WebContentsConsoleObserver add_module_console_observer(
        GetActiveWebContents());
    add_module_console_observer.SetFilter(
        MakeFilter({"Finish executing customizable_module.js"}));

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

    add_module_console_observer.Wait();

    EXPECT_EQ(1u, content::GetAttachedWorkletHostsCountForRenderFrameHost(
                      execution_target));
    EXPECT_EQ(0u, content::GetKeepAliveWorkletHostsCountForRenderFrameHost(
                      execution_target));
    EXPECT_EQ(1u, add_module_console_observer.messages().size());
    EXPECT_EQ(
        "Finish executing customizable_module.js",
        base::UTF16ToUTF8(add_module_console_observer.messages()[0].message));

    content::WebContentsConsoleObserver script_console_observer(
        GetActiveWebContents());
    script_console_observer.SetFilter(MakeFilter(
        {last_script_message, content::GetSharedStorageDisabledMessage()}));

    content::EvalJsResult result = content::EvalJs(execution_target, R"(
        sharedStorage.run('test-operation');
      )");

    script_console_observer.Wait();
    EXPECT_EQ(1u, script_console_observer.messages().size());

    if (SuccessExpected()) {
      EXPECT_EQ(
          last_script_message,
          base::UTF16ToUTF8(script_console_observer.messages()[0].message));
    } else {
      EXPECT_EQ(
          content::GetSharedStorageDisabledMessage(),
          base::UTF16ToUTF8(script_console_observer.messages()[0].message));
    }

    content::SetBypassIsSharedStorageAllowed(/*allow=*/false);

    return result.error.empty();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SharedStorageChromeBrowserTest,
    testing::ValuesIn(GetSharedStorageChromeBrowserParams()),
    testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, AddModule) {
  NavigateParams params(
      browser(), https_server()->GetURL(kSimpleTestHost, kSimplePagePath),
      ui::PageTransition::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  content::WebContentsConsoleObserver console_observer(GetActiveWebContents());
  console_observer.SetFilter(MakeFilter({"Finish executing simple_module.js"}));

  content::EvalJsResult result = content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )");

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ("a JavaScript error:\nError: sharedStorage is disabled\n",
              result.error);
    EXPECT_EQ(0u, console_observer.messages().size());
    return;
  }

  console_observer.Wait();

  // Privacy Sandox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(result.error.empty());
  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, RunOperation) {
  NavigateParams params(
      browser(), https_server()->GetURL(kSimpleTestHost, kSimplePagePath),
      ui::PageTransition::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

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

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageDisabledErrorMessage(), run_op_result.error);
    return;
  }

  run_op_console_observer.Wait();

  // Privacy Sandox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(run_op_result.error.empty());
  EXPECT_EQ(1u, run_op_console_observer.messages().size());
  EXPECT_EQ("Finish executing \'test-operation\'",
            base::UTF16ToUTF8(run_op_console_observer.messages()[0].message));
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       RunURLSelectionOperation) {
  NavigateParams params(
      browser(), https_server()->GetURL(kSimpleTestHost, kSimplePagePath),
      ui::PageTransition::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  AddSimpleModuleWithPermissionBypassed(GetActiveWebContents());
  content::WebContentsConsoleObserver run_url_op_console_observer(
      GetActiveWebContents());
  run_url_op_console_observer.SetFilter(
      MakeFilter({"Finish executing \'test-url-selection-operation\'"}));

  content::EvalJsResult run_url_op_result =
      content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.selectURL(
          'test-url-selection-operation',
          ["fenced_frames/title0.html", "fenced_frames/title1.html",
          "fenced_frames/title2.html"], {data: {'mockResult': 1}});
    )");

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageDisabledErrorMessage(), run_url_op_result.error);
    return;
  }

  run_url_op_console_observer.Wait();

  // Privacy Sandox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(run_url_op_result.error.empty());
  EXPECT_TRUE(
      blink::IsValidUrnUuidURL(GURL(run_url_op_result.ExtractString())));
  EXPECT_EQ(1u, run_url_op_console_observer.messages().size());
  EXPECT_EQ(
      "Finish executing \'test-url-selection-operation\'",
      base::UTF16ToUTF8(run_url_op_console_observer.messages()[0].message));
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, Set) {
  NavigateParams params(
      browser(), https_server()->GetURL(kSimpleTestHost, kSimplePagePath),
      ui::PageTransition::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  AddSimpleModuleWithPermissionBypassed(GetActiveWebContents());

  content::EvalJsResult set_result = content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.set('customKey', 'customValue');
    )");

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageDisabledErrorMessage(), set_result.error);
    return;
  }

  // Privacy Sandox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(set_result.error.empty());
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, Append) {
  NavigateParams params(
      browser(), https_server()->GetURL(kSimpleTestHost, kSimplePagePath),
      ui::PageTransition::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  AddSimpleModuleWithPermissionBypassed(GetActiveWebContents());

  content::EvalJsResult append_result =
      content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.append('customKey', 'customValue');
    )");

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageDisabledErrorMessage(), append_result.error);
    return;
  }

  // Privacy Sandox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(append_result.error.empty());
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, Delete) {
  NavigateParams params(
      browser(), https_server()->GetURL(kSimpleTestHost, kSimplePagePath),
      ui::PageTransition::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  AddSimpleModuleWithPermissionBypassed(GetActiveWebContents());

  content::EvalJsResult delete_result =
      content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.delete('customKey');
    )");

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageDisabledErrorMessage(), delete_result.error);
    return;
  }

  // Privacy Sandox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(delete_result.error.empty());
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, Clear) {
  NavigateParams params(
      browser(), https_server()->GetURL(kSimpleTestHost, kSimplePagePath),
      ui::PageTransition::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  AddSimpleModuleWithPermissionBypassed(GetActiveWebContents());

  content::EvalJsResult clear_result =
      content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.clear();
    )");

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageDisabledErrorMessage(), clear_result.error);
    return;
  }

  // Privacy Sandox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(clear_result.error.empty());
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, WorkletSet) {
  NavigateParams params(
      browser(), https_server()->GetURL(kSimpleTestHost, kSimplePagePath),
      ui::PageTransition::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  // If `set()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper `ExecuteScriptInWorklet()` rather
  // than an error message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      console.log(await sharedStorage.set('key0', 'value0'));
      console.log('Finished script');
    )",
                                     "Finished script"));
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, WorkletAppend) {
  NavigateParams params(
      browser(), https_server()->GetURL(kSimpleTestHost, kSimplePagePath),
      ui::PageTransition::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  // If `append()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper `ExecuteScriptInWorklet()` rather
  // than an error message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      console.log(await sharedStorage.append('key0', 'value0'));
      console.log('Finished script');
    )",
                                     "Finished script"));
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, WorkletDelete) {
  NavigateParams params(
      browser(), https_server()->GetURL(kSimpleTestHost, kSimplePagePath),
      ui::PageTransition::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  // If `delete()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper `ExecuteScriptInWorklet()` rather
  // than an error message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      console.log(await sharedStorage.delete('key0'));
      console.log('Finished script');
    )",
                                     "Finished script"));
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, WorkletClear) {
  NavigateParams params(
      browser(), https_server()->GetURL(kSimpleTestHost, kSimplePagePath),
      ui::PageTransition::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  // If `clear()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper `ExecuteScriptInWorklet()` rather
  // than an error message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      console.log(await sharedStorage.clear());
      console.log('Finished script');
    )",
                                     "Finished script"));
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, WorkletGet) {
  NavigateParams params(
      browser(), https_server()->GetURL(kSimpleTestHost, kSimplePagePath),
      ui::PageTransition::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

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
  // console message verified in the helper `ExecuteScriptInWorklet()` rather
  // than an error message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), script,
                                     "Finished script"));
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, WorkletKeys) {
  NavigateParams params(
      browser(), https_server()->GetURL(kSimpleTestHost, kSimplePagePath),
      ui::PageTransition::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  // If `keys()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper `ExecuteScriptInWorklet()` rather
  // than an error message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      for await (const key of sharedStorage.keys()) {
        console.log(key);
      }
      console.log('Finished script');
    )",
                                     "Finished script"));
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, WorkletEntries) {
  NavigateParams params(
      browser(), https_server()->GetURL(kSimpleTestHost, kSimplePagePath),
      ui::PageTransition::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  // If `entries()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper `ExecuteScriptInWorklet()` rather
  // than an error message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
      console.log('Finished script');
    )",
                                     "Finished script"));
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, WorkletLength) {
  NavigateParams params(
      browser(), https_server()->GetURL(kSimpleTestHost, kSimplePagePath),
      ui::PageTransition::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  // If `length()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper `ExecuteScriptInWorklet()` rather
  // than an error message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      console.log(await sharedStorage.length());
      console.log('Finished script');
    )",
                                     "Finished script"));
}

}  // namespace storage

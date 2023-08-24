// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/api/scripting/scripting_api.h"

#include "base/test/bind.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/utils/content_script_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace extensions {

constexpr const char kSimulatedResourcePath[] = "/simulated-resource.html";

class ScriptingAPITest : public ExtensionApiTest {
 public:
  ScriptingAPITest() = default;
  ScriptingAPITest(const ScriptingAPITest&) = delete;
  ScriptingAPITest& operator=(const ScriptingAPITest&) = delete;
  ~ScriptingAPITest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    controllable_http_response_.emplace(embedded_test_server(),
                                        kSimulatedResourcePath);
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  void OpenURLInCurrentTab(const GURL& url) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    content::TestNavigationObserver nav_observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(url, web_contents->GetLastCommittedURL());
  }

  void OpenURLInNewTab(const GURL& url) {
    content::TestNavigationObserver nav_observer(url);
    nav_observer.StartWatchingNewWebContents();
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(url, browser()
                       ->tab_strip_model()
                       ->GetActiveWebContents()
                       ->GetLastCommittedURL());
  }

  net::test_server::ControllableHttpResponse& controllable_http_response() {
    return *controllable_http_response_;
  }

 private:
  // A controllable HTTP response for tests that need fine-grained timing.
  // Must be constructed as part of the test suite because it needs to
  // happen before the embedded test server is initialized.
  absl::optional<net::test_server::ControllableHttpResponse>
      controllable_http_response_;
};

IN_PROC_BROWSER_TEST_F(ScriptingAPITest, MainFrameTests) {
  OpenURLInCurrentTab(embedded_test_server()->GetURL(
      "example.com", "/extensions/main_world_script_flag.html"));
  OpenURLInNewTab(
      embedded_test_server()->GetURL("chromium.org", "/title2.html"));

  ASSERT_TRUE(RunExtensionTest("scripting/main_frame", {},
                               {.ignore_manifest_warnings = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ScriptingAPITest, SubFramesTests) {
  // Open up two tabs, each with cross-site iframes, one at a.com and one at
  // d.com.
  // In both cases, the cross-site iframes point to b.com and c.com.
  OpenURLInCurrentTab(
      embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html"));
  OpenURLInNewTab(
      embedded_test_server()->GetURL("d.com", "/iframe_cross_site.html"));

  // From there, the test continues in the JS.
  ASSERT_TRUE(RunExtensionTest("scripting/sub_frames")) << message_;
}

// Test validating we don't insert content into nested WebContents.
IN_PROC_BROWSER_TEST_F(ScriptingAPITest, NestedWebContents) {
  OpenURLInCurrentTab(
      embedded_test_server()->GetURL("a.com", "/page_with_embedded_pdf.html"));

  // From there, the test continues in the JS.
  ASSERT_TRUE(RunExtensionTest("scripting/nested_web_contents")) << message_;
}

IN_PROC_BROWSER_TEST_F(ScriptingAPITest, CSSInjection) {
  OpenURLInCurrentTab(
      embedded_test_server()->GetURL("example.com", "/simple.html"));
  OpenURLInNewTab(
      embedded_test_server()->GetURL("chromium.org", "/title2.html"));
  OpenURLInNewTab(embedded_test_server()->GetURL("subframes.example",
                                                 "/iframe_cross_site.html"));

  ASSERT_TRUE(RunExtensionTest("scripting/css_injection")) << message_;
}

IN_PROC_BROWSER_TEST_F(ScriptingAPITest, CSSRemoval) {
  ASSERT_TRUE(RunExtensionTest("scripting/remove_css")) << message_;
}

IN_PROC_BROWSER_TEST_F(ScriptingAPITest, DynamicContentScripts) {
  ASSERT_TRUE(RunExtensionTest("scripting/dynamic_scripts")) << message_;
}

IN_PROC_BROWSER_TEST_F(ScriptingAPITest, DynamicContentScriptParameters) {
  // Dynamic content script parameters are currently limited to trunk.
  ScopedCurrentChannel scoped_channel(version_info::Channel::UNKNOWN);
  ASSERT_TRUE(RunExtensionTest("scripting/dynamic_script_parameters"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ScriptingAPITest, DynamicContentScriptsMainWorld) {
  ASSERT_TRUE(RunExtensionTest("scripting/dynamic_scripts_main_world"))
      << message_;
}

// Test that if an extension with persistent scripts is quickly unloaded while
// these scripts are being fetched, requests that wait on that extension's
// script load will be unblocked. Regression for crbug.com/1250575
IN_PROC_BROWSER_TEST_F(ScriptingAPITest, RapidLoadUnload) {
  ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("scripting/register_one_script"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  DisableExtension(extension->id());

  // Load another extension with a content script that is injected into all
  // sites. This extension is necessary because while the register_one_script
  // extension is loading its scripts, requests which match ANY extension's
  // scripts are throttled. When an extension unloads before its script load is
  // finished and there are no more loads in progress, we want to verify that
  // ALL throttled requests are resumed, not just the ones matching the unloaded
  // extension's scripts.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("content_scripts/css_injection")));

  // First, trigger an OnLoaded event for `extension`, then quickly trigger an
  // OnUnloaded event by calling Enable/DisableExtension. Since `extension`
  // contains persistent dynamic scripts, it must fetch them from the StateStore
  // which yields control of the thread so TriggerOnUnloaded is called
  // immediately, which unloads the extension before the Statestore fetch can
  // finish.
  extension_service()->EnableExtension(extension->id());
  extension_service()->DisableExtension(extension->id(),
                                        disable_reason::DISABLE_USER_ACTION);

  // Verify that the navigation to google.com, which matches a script in the
  // css_injection extension, will complete.
  OpenURLInCurrentTab(
      embedded_test_server()->GetURL("google.com", "/simple.html"));
}

IN_PROC_BROWSER_TEST_F(ScriptingAPITest, DynamicContentScriptsSizeLimits) {
  auto single_scripts_limit_reset =
      script_parsing::CreateScopedMaxScriptLengthForTesting(700u);
  auto extension_scripts_limit_reset =
      script_parsing::CreateScopedMaxScriptsLengthPerExtensionForTesting(1200u);
  ASSERT_TRUE(RunExtensionTest("scripting/dynamic_scripts_size_limits"))
      << message_;
}

// Tests that scripting.executeScript called with files exceeding the max size
// limit will return an error and not execute.
IN_PROC_BROWSER_TEST_F(ScriptingAPITest, ExecuteScriptSizeLimit) {
  auto single_scripts_limit_reset =
      script_parsing::CreateScopedMaxScriptLengthForTesting(700u);
  ASSERT_TRUE(RunExtensionTest("scripting/execute_script_size_limit"))
      << message_;
}

// Tests that calling scripting.executeScript works on a newly created tab
// before the initial commit has happened. Regression for crbug.com/1191971.
IN_PROC_BROWSER_TEST_F(ScriptingAPITest, ExecuteScriptBeforeInitialCommit) {
  constexpr char kManifest[] =
      R"({
           "name": "Scripting API Test",
           "manifest_version": 3,
           "version": "0.1",
           "permissions": ["scripting", "tabs"],
           "host_permissions": ["http://example.com/*"]
         })";

  constexpr char kArgTemplate[] =
      R"([{
            "target": {"tabId": %d},
            "func": "() => {
                document.title = 'Modified Title';
                return document.title;
            }"
          }])";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);

  {
    GURL target_url =
        embedded_test_server()->GetURL("example.com", "/simple.html");
    auto execute_script_function =
        base::MakeRefCounted<ScriptingExecuteScriptFunction>();

    const Extension* extension = LoadExtension(test_dir.UnpackedPath());
    ASSERT_TRUE(extension);
    execute_script_function->set_extension(extension);
    execute_script_function->set_has_callback(true);

    // We only want to wait for the tab to have loaded here and not for the
    // navigation to have ended because we want executeScript to run before the
    // navigation has committed.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), target_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    EXPECT_TRUE(web_contents->GetLastCommittedURL().is_empty());
    EXPECT_EQ(target_url, web_contents->GetVisibleURL());

    // To avoid as much async delay as possible we invoke the execute script
    // extension function manually rather than calling it in JS.
    int tab_id = ExtensionTabUtil::GetTabId(web_contents);
    std::string args = base::StringPrintf(kArgTemplate, tab_id);
    absl::optional<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            execute_script_function.get(), args, profile());

    // Now we check the function call returned what we expected in the result.
    ASSERT_TRUE(result);
    base::Value::List& result_list = result->GetList();
    ASSERT_EQ(1u, result_list.size());
    const std::string* result_returned =
        result_list[0].GetDict().FindString("result");
    ASSERT_TRUE(result_returned);
    EXPECT_EQ("Modified Title", *result_returned);

    // We also check that the tab itself was modified by the call.
    EXPECT_EQ(u"Modified Title", web_contents->GetTitle());

    // Ensure that once the page has entirely finished loading and the
    // navigation has committed, our executeScript changes have stuck.
    EXPECT_TRUE(WaitForLoadStop(web_contents));
    EXPECT_EQ(target_url, web_contents->GetLastCommittedURL());
    EXPECT_EQ(u"Modified Title", web_contents->GetTitle());
  }

  // Same as above, but for a page which the extension does not have access to.
  {
    GURL target_url =
        embedded_test_server()->GetURL("noAccess.com", "/simple.html");
    auto execute_script_function =
        base::MakeRefCounted<ScriptingExecuteScriptFunction>();

    const Extension* extension = LoadExtension(test_dir.UnpackedPath());
    ASSERT_TRUE(extension);
    execute_script_function->set_extension(extension);
    execute_script_function->set_has_callback(true);

    // We only want to wait for the tab to have loaded here and not for the
    // navigation to have ended because we want executeScript to run before the
    // navigation has committed.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), target_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    EXPECT_TRUE(web_contents->GetLastCommittedURL().is_empty());
    EXPECT_EQ(target_url, web_contents->GetVisibleURL());

    // To avoid as much async delay as possible we invoke the execute script
    // extension function manually rather than calling it in JS.
    int tab_id = ExtensionTabUtil::GetTabId(web_contents);
    std::string args = base::StringPrintf(kArgTemplate, tab_id);
    std::string error(api_test_utils::RunFunctionAndReturnError(
        execute_script_function.get(), args, profile()));

    // We should have gotten back an error that the page could not be accessed.
    // The URL for the pending navigation will be included because the extension
    // has the tabs persmission.
    std::string expected_error = base::StringPrintf(
        "Cannot access contents of url \"%s\". Extension manifest must request "
        "permission to access this host.",
        target_url.spec().c_str());
    EXPECT_EQ(expected_error, error);

    // We also need to verify the page was not modified by the execute script
    // call. Since this is a still loading page the title will be blank.
    EXPECT_EQ(u"", web_contents->GetTitle());

    // After the page has finished loading, there should be a title that is
    // still unmodified by our executeScript call.
    EXPECT_TRUE(WaitForLoadStop(web_contents));
    EXPECT_EQ(target_url, web_contents->GetLastCommittedURL());
    EXPECT_EQ(u"OK", web_contents->GetTitle());
  }
}

// Tests that extensions are able to specify that a script should be able to
// inject into a page as soon as possible. The testing for this is a bit tricky
// because we need to craft a page that is guaranteed to not load by the time
// the injections are triggered.
IN_PROC_BROWSER_TEST_F(ScriptingAPITest, InjectImmediately) {
  static constexpr char kManifest[] =
      R"({
           "name": "Scripting Extension",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "worker.js"},
           "permissions": ["scripting"],
           "host_permissions": ["http://example.com/*"]
         })";
  static constexpr char kWorker[] = "// Intentionally blank";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorker);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to a URL with a controllable response. This allows us to guarantee
  // that the page hasn't finished loading by the time we try to inject.
  const GURL page_url =
      embedded_test_server()->GetURL("example.com", kSimulatedResourcePath);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), page_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  controllable_http_response().WaitForRequest();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(web_contents->IsLoading());
  const int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // A script to call executeScript() twice - once with the default injection
  // timing and once with `injectImmediately` specified. The script with
  // `injectImmediately` should inject before the page finishes loading.
  // Upon injection completion, stores the result of the injection (which is the
  // target's URL) and sends a message.
  static constexpr char kInjectScripts[] =
      R"(function injectImmediate() {
           return 'Immediate: ' + window.location.href;
         }

         function injectDefault() {
           return 'Default: ' + window.location.href;
         }

         self.defaultResult = 'unset';
         self.immediateResult = 'unset';

         const target = {tabId: %d};
         chrome.scripting.executeScript(
             {
               target: target,
               func: injectDefault,
             },
             (results) => {
               self.defaultResult = results[0].result;
               chrome.test.sendMessage('default complete');
             });

         chrome.scripting.executeScript(
             {
               target: target,
               func: injectImmediate,
               injectImmediately: true,
             },
             (results) => {
               self.immediateResult = results[0].result;
               chrome.test.sendMessage('immediate complete');
             });)";

  std::string expected_immediate_result = "Immediate: " + page_url.spec();
  std::string expected_default_result = "Default: " + page_url.spec();

  // A helper function to run the script in the worker context.
  auto run_script_in_worker = [this, extension](const std::string& script) {
    return BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(), script,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  };

  auto get_default_result = [run_script_in_worker]() {
    return run_script_in_worker(
        "chrome.test.sendScriptResult(self.defaultResult);");
  };
  auto get_immediate_result = [run_script_in_worker]() {
    return run_script_in_worker(
        "chrome.test.sendScriptResult(self.immediateResult);");
  };

  // Send back some HTML for the request (this can only be done once the
  // request is received via WaitForRequest(), above). This doesn't complete
  // the request; the request is considered ongoing until
  // ControllableHttpResponse::Done() is called.
  controllable_http_response().Send(net::HTTP_OK, "text/html",
                                    "<html>Hello, World!</html>");

  EXPECT_TRUE(web_contents->IsLoading());

  ExtensionTestMessageListener immediate_listener("immediate complete");
  ExtensionTestMessageListener default_listener("default complete");
  BackgroundScriptExecutor::ExecuteScriptAsync(
      profile(), extension->id(), base::StringPrintf(kInjectScripts, tab_id));

  // The script with immediate injection should finish (but it's still round
  // trips to the browser and renderer, so it won't be synchronous).
  ASSERT_TRUE(immediate_listener.WaitUntilSatisfied());

  // The web contents should still be loading. The immediate injection should
  // have finished properly, and the default injection should still be pending.
  EXPECT_TRUE(web_contents->IsLoading());
  EXPECT_FALSE(default_listener.was_satisfied());
  EXPECT_EQ(base::Value("unset"), get_default_result());
  EXPECT_EQ(base::Value(expected_immediate_result), get_immediate_result());

  // Finish loading the page by completing the HTTP response.
  controllable_http_response().Done();
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  // The default injection should now be able to complete.
  ASSERT_TRUE(default_listener.WaitUntilSatisfied());

  EXPECT_EQ(base::Value(expected_default_result), get_default_result());
  EXPECT_EQ(base::Value(expected_immediate_result), get_immediate_result());
}

// Base test fixture for tests spanning multiple sessions where a custom arg is
// set before the test is run.
class PersistentScriptingAPITest : public ScriptingAPITest {
 public:
  PersistentScriptingAPITest() = default;

  // ScriptingAPITest override.
  void SetUp() override {
    // Initialize the listener object here before calling SetUp. This avoids a
    // race condition where the extension loads (as part of browser startup) and
    // sends a message before a message listener in C++ has been initialized.

    listener_ = std::make_unique<ExtensionTestMessageListener>(
        "ready", ReplyBehavior::kWillReply);
    ScriptingAPITest::SetUp();
  }

  // Reset listener before the browser gets torn down.
  void TearDownOnMainThread() override {
    listener_.reset();
    ScriptingAPITest::TearDownOnMainThread();
  }

 protected:
  // Used to wait for results from extension tests. This is initialized before
  // the test is run which avoids a race condition where the extension is loaded
  // (as part of startup) and finishes its tests before the ResultCatcher is
  // created.
  ResultCatcher result_catcher_;

  // Used to wait for the extension to load and send a ready message so the test
  // can reply which the extension waits for to start its testing functions.
  // This ensures that the testing functions will run after the browser has
  // finished initializing.
  std::unique_ptr<ExtensionTestMessageListener> listener_;
};

// Tests that registered content scripts which persist across sessions behave as
// expected. The test is run across three sessions.
IN_PROC_BROWSER_TEST_F(PersistentScriptingAPITest,
                       PRE_PRE_PersistentDynamicContentScripts) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("scripting/persistent_dynamic_scripts"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

IN_PROC_BROWSER_TEST_F(PersistentScriptingAPITest,
                       PRE_PersistentDynamicContentScripts) {
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

IN_PROC_BROWSER_TEST_F(PersistentScriptingAPITest,
                       PersistentDynamicContentScripts) {
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

class ScriptingAPIPrerenderingTest : public ScriptingAPITest {
 protected:
  ScriptingAPIPrerenderingTest() = default;
  ~ScriptingAPIPrerenderingTest() override = default;

 private:
  content::test::ScopedPrerenderFeatureList scoped_feature_list_;
};

// TODO(crbug.com/1351648): disabled on Mac due to flakiness.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Basic DISABLED_Basic
#else  // BUILDFLAG(IS_MAC)
#define MAYBE_Basic Basic
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ScriptingAPIPrerenderingTest, MAYBE_Basic) {
  ASSERT_TRUE(RunExtensionTest("scripting/prerendering")) << message_;
}

class ScriptingAndUserScriptsAPITest : public ScriptingAPITest {
 public:
  ScriptingAndUserScriptsAPITest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kApiUserScripts);
  }
  ScriptingAndUserScriptsAPITest(const ScriptingAndUserScriptsAPITest&) =
      delete;
  ScriptingAndUserScriptsAPITest& operator=(
      const ScriptingAndUserScriptsAPITest&) = delete;
  ~ScriptingAndUserScriptsAPITest() override = default;

 private:
  // The userScripts API is currently behind a channel and feature restriction.
  // TODO(crbug.com/1472902): Remove channel override when user scripts API goes
  // to stable.
  ScopedCurrentChannel current_channel_override_{
      version_info::Channel::UNKNOWN};
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ScriptingAndUserScriptsAPITest,
                       ScriptingAPIDoesNotAffectUserScripts) {
  ASSERT_TRUE(RunExtensionTest("scripting/dynamic_user_scripts")) << message_;
}

}  // namespace extensions

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/extension_builder.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class UserScriptWorldBrowserTest : public ExtensionApiTest {
 public:
  UserScriptWorldBrowserTest() = default;
  UserScriptWorldBrowserTest(const UserScriptWorldBrowserTest&) = delete;
  UserScriptWorldBrowserTest& operator=(const UserScriptWorldBrowserTest&) =
      delete;
  ~UserScriptWorldBrowserTest() override = default;

  // ExtensionApiTest:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Executes the given `script` in a user script world associated with the
  // `extension`, returning the script result. This expects the script to
  // succeed (i.e., not throw an error) and runs the script in the primary main
  // frame of the active WebContents.
  base::Value ExecuteScriptInUserScriptWorld(const std::string& script,
                                             const Extension& extension) {
    SCOPED_TRACE(script.c_str());

    ScriptExecutor script_executor(GetActiveWebContents());
    base::RunLoop run_loop;
    std::vector<ScriptExecutor::FrameResult> script_results;
    auto on_complete =
        [&run_loop, &script_results](
            std::vector<ScriptExecutor::FrameResult> frame_results) {
          script_results = std::move(frame_results);
          run_loop.Quit();
        };

    std::vector<mojom::JSSourcePtr> sources;
    sources.push_back(mojom::JSSource::New(script, GURL()));
    script_executor.ExecuteScript(
        mojom::HostID(mojom::HostID::HostType::kExtensions, extension.id()),
        mojom::CodeInjection::NewJs(mojom::JSInjection::New(
            std::move(sources), mojom::ExecutionWorld::kUserScript,
            blink::mojom::WantResultOption::kWantResult,
            blink::mojom::UserActivationOption::kDoNotActivate,
            blink::mojom::PromiseResultOption::kAwait)),
        ScriptExecutor::SPECIFIED_FRAMES, {ExtensionApiFrameIdMap::kTopFrameId},
        ScriptExecutor::DONT_MATCH_ABOUT_BLANK,
        mojom::RunLocation::kDocumentIdle, ScriptExecutor::DEFAULT_PROCESS,
        GURL() /* webview_src */, base::BindLambdaForTesting(on_complete));
    run_loop.Run();

    if (script_results.size() != 1) {
      ADD_FAILURE() << "Incorrect script execution result count: "
                    << script_results.size();
      return base::Value();
    }

    ScriptExecutor::FrameResult& frame_result = script_results[0];
    if (!frame_result.error.empty()) {
      ADD_FAILURE() << "Unexpected script error: " << frame_result.error;
      return base::Value();
    }

    if (frame_result.value.is_none()) {
      ADD_FAILURE() << "Null return value";
      return base::Value();
    }

    return std::move(frame_result.value);
  }

  // Navigates the active web contents to `url`, waiting for the navigation to
  // (successfully) complete.
  void NavigateToURL(const GURL& url) {
    content::TestNavigationObserver nav_observer(GetActiveWebContents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  }

  // Loads and returns an extension with the given `host_permission`.
  const Extension* LoadExtensionWithHostPermission(
      const std::string& host_permission) {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("extension")
            .SetManifestVersion(3)
            .SetManifestKey("host_permissions",
                            base::Value::List().Append(host_permission))
            .Build();
    extension_service()->AddExtension(extension.get());
    EXPECT_TRUE(
        extension_registry()->enabled_extensions().GetByID(extension->id()));
    return extension.get();
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

// Tests that a user script world for an extension executed in an
// isolated world and has limited access to extension APIs.
IN_PROC_BROWSER_TEST_F(UserScriptWorldBrowserTest,
                       LimitedAPIsAreAvailableInUserScriptWorlds) {
  const Extension* extension =
      LoadExtensionWithHostPermission("http://example.com/*");

  GURL example_com =
      embedded_test_server()->GetURL("example.com", "/simple.html");

  NavigateToURL(example_com);

  content::WebContents* web_contents = GetActiveWebContents();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();

  // Set a flag in the main world of the page. This will allow us to verify
  // the new script is running in an isolated world.
  constexpr char kSetFlagScript[] = "window.mainWorldFlag = 'executionFlag';";
  // NOTE: We use ExecuteScript() (and not EvalJs or ExecJs) because we
  // explicitly *need* this to happen in the main world for the test.
  EXPECT_TRUE(content::ExecuteScript(main_frame, kSetFlagScript));

  // Inject a script into a user script world. The script will return the
  // values of both the main world flag (set above) and all properties exposed
  // on `chrome.runtime`.
  static constexpr char kScriptSource[] =
      R"(let result = {};
         result.mainWorldFlag = window.mainWorldFlag || '<no flag>';
         result.chromeKeys =
             chrome ? Object.keys(chrome).sort() : '<no chrome>';
         result.runtimeKeys = chrome && chrome.runtime ?
             Object.keys(chrome.runtime).sort() : '<no runtime>';
         result;)";

  base::Value script_result =
      ExecuteScriptInUserScriptWorld(kScriptSource, *extension);

  // Verify the expected results. Since the user script world is less
  // privileged, it shouldn't have access to most runtime APIs (such as reload,
  // onStartup, getManifest, etc).
  static constexpr char kExpectedJson[] =
      R"({
           "mainWorldFlag": "<no flag>",
           "chromeKeys": ["csi", "loadTimes", "runtime"],
           "runtimeKeys": ["ContextType", "OnInstalledReason",
                           "OnRestartRequiredReason", "PlatformArch",
                           "PlatformNaclArch", "PlatformOs",
                           "RequestUpdateCheckStatus",
                           "connect", "id", "onConnect", "onMessage",
                           "sendMessage"]
         })";
  EXPECT_THAT(script_result, base::test::IsJson(kExpectedJson));
}

// Tests that, by default, the user script world's CSP is the same as the
// extension's CSP.
IN_PROC_BROWSER_TEST_F(UserScriptWorldBrowserTest,
                       DefaultCspIsExtensionDefault) {
  // Load a simple extension with permission to example.com and navigate a new
  // tab to example.com.
  const Extension* extension =
      LoadExtensionWithHostPermission("http://example.com/*");

  NavigateToURL(embedded_test_server()->GetURL("example.com", "/simple.html"));

  // Execute a script that attempts to eval() some code. This should fail, since
  // by default the user script world CSP is the same as the extension's CSP
  // (which prevents eval).
  static constexpr char kScriptSource[] =
      R"(var result;
         try {
           eval('result = "allowed eval"');
         } catch (e) {
           result = 'disallowed eval';
         }
         result;)";
  base::Value script_result =
      ExecuteScriptInUserScriptWorld(kScriptSource, *extension);

  EXPECT_EQ(script_result, "disallowed eval");
}

// Tests that the user script world CSP can be updated to allow unsafe
// directives, like `unsafe-eval`.
// TODO(https://crbug.com/1429408): This is currently a separate test than the
// above because re-setting the isolated world CSP in blink doesn't work. This
// is due to the caching code here [1], which prevents a lookup if a world has
// already been created. We'll need to fix this, since otherwise isolated world
// CSP would be sticky per renderer process.
// [1]:
// https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:third_party/blink/renderer/core/frame/local_dom_window.cc;l=374-391;drc=8ce14ef97f8607b1b57f8d02da575ed5150eea9e
IN_PROC_BROWSER_TEST_F(UserScriptWorldBrowserTest, CanSetCustomCsp) {
  // Load a simple extension with permission to example.com and navigate a new
  // tab to example.com.
  const Extension* extension =
      LoadExtensionWithHostPermission("http://example.com/*");

  NavigateToURL(embedded_test_server()->GetURL("example.com", "/simple.html"));

  // Update the user script world CSP to allow unsafe eval.
  RendererStartupHelperFactory::GetForBrowserContext(profile())
      ->SetUserScriptWorldCsp(*extension, "script-src 'unsafe-eval'");

  // Execute a script that attempts to eval() some code. This should succeed,
  // since the CSP has been updated.
  static constexpr char kScriptSource[] =
      R"(var result;
         try {
           eval('result = "allowed eval"');
         } catch (e) {
           result = 'disallowed eval';
         }
         result;)";
  base::Value script_result =
      ExecuteScriptInUserScriptWorld(kScriptSource, *extension);

  EXPECT_EQ(script_result, "allowed eval");
}

}  // namespace extensions

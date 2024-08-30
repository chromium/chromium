// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kCheckIfEvalAllowedScriptSource[] =
    R"(var result;
       try {
         eval('result = "allowed eval"');
       } catch (e) {
         result = 'disallowed eval';
       }
       result;)";

constexpr char kGetMessagingProperties[] =
    R"(let messagingProperties = [
           'sendMessage', 'onMessage', 'connect', 'onConnect'
       ];
       let runtimeProperties =
           chrome && chrome.runtime
               ? Object.keys(chrome.runtime)
               : [];
       messagingProperties =
           messagingProperties.filter((prop) => {
             return runtimeProperties.includes(prop);
           });
       messagingProperties;)";

}  // namespace

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
  // `extension` and the given `world_id`, returning the script result. This
  // expects the script to succeed (i.e., not throw an error) and runs the
  // script in the primary main frame of the active WebContents.
  base::Value ExecuteScriptInUserScriptWorld(
      const std::string& script,
      const Extension& extension,
      const std::optional<std::string>& world_id = std::nullopt) {
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
            std::move(sources), mojom::ExecutionWorld::kUserScript, world_id,
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
            .AddHostPermission(host_permission)
            .Build();
    extension_service()->AddExtension(extension.get());
    EXPECT_TRUE(
        extension_registry()->enabled_extensions().GetByID(extension->id()));
    return extension.get();
  }

  // Sets the user script world properties in the renderer(s).
  void SetDefaultUserScriptWorldProperties(const Extension& extension,
                                           std::optional<std::string> csp,
                                           bool enable_messaging) {
    SetUserScriptWorldProperties(extension, /*world_id=*/std::nullopt,
                                 std::move(csp), enable_messaging);
  }

  void SetUserScriptWorldProperties(const Extension& extension,
                                    std::optional<std::string> world_id,
                                    std::optional<std::string> csp,
                                    bool enable_messaging) {
    RendererStartupHelperFactory::GetForBrowserContext(profile())
        ->SetUserScriptWorldProperties(extension, std::move(world_id),
                                       std::move(csp), enable_messaging);
  }

  // Clears associated user script world properties in the renderer(s).
  void ClearUserScriptWorldProperties(const Extension& extension,
                                      std::optional<std::string> world_id) {
    RendererStartupHelperFactory::GetForBrowserContext(profile())
        ->ClearUserScriptWorldProperties(extension, std::move(world_id));
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

  // Enable messaging to get the full suite of possible APIs exposed to
  // user script worlds.
  SetDefaultUserScriptWorldProperties(*extension, std::nullopt,
                                      /*enable_messaging=*/true);

  GURL example_com =
      embedded_test_server()->GetURL("example.com", "/simple.html");

  NavigateToURL(example_com);

  content::WebContents* web_contents = GetActiveWebContents();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();

  // Set a flag in the main world of the page. This will allow us to verify
  // the new script is running in an isolated world.
  constexpr char kSetFlagScript[] = "window.mainWorldFlag = 'executionFlag';";
  // NOTE: We *need* this to happen in the main world for the test.
  EXPECT_TRUE(content::ExecJs(main_frame, kSetFlagScript));

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
           "chromeKeys": ["csi", "loadTimes", "runtime", "test"],
           "runtimeKeys": ["ContextType", "OnInstalledReason",
                           "OnRestartRequiredReason", "PlatformArch",
                           "PlatformNaclArch", "PlatformOs",
                           "RequestUpdateCheckStatus",
                           "connect", "dynamicId", "id", "onConnect",
                           "onMessage", "sendMessage"]
         })";
  EXPECT_THAT(script_result, base::test::IsJson(kExpectedJson));
}

// Tests that, by default, the user script world's CSP is the same as the
// extension's CSP, but it can be updated to a more relaxed value.
IN_PROC_BROWSER_TEST_F(UserScriptWorldBrowserTest,
                       UserScriptWorldCspDefaultsToExtensionsAndCanBeUpdated) {
  // Load a simple extension with permission to example.com and navigate a new
  // tab to example.com.
  const Extension* extension =
      LoadExtensionWithHostPermission("http://example.com/*");

  NavigateToURL(embedded_test_server()->GetURL("example.com", "/simple.html"));

  // Execute a script that attempts to eval() some code.
  base::Value script_result = ExecuteScriptInUserScriptWorld(
      kCheckIfEvalAllowedScriptSource, *extension);

  // This should fail, since by default the user script world CSP is the same
  // as the extension's CSP (which prevents eval).
  EXPECT_EQ(script_result, "disallowed eval");

  // Update the user script world CSP to allow unsafe eval.
  SetDefaultUserScriptWorldProperties(*extension, "script-src 'unsafe-eval'",
                                      /*enable_messaging=*/true);
  // Navigate to create a new isolated world.
  NavigateToURL(embedded_test_server()->GetURL("example.com", "/simple.html"));

  // Now, eval should be allowed.
  script_result = ExecuteScriptInUserScriptWorld(
      kCheckIfEvalAllowedScriptSource, *extension);
  EXPECT_EQ(script_result, "allowed eval");
}

// Tests that an update to the user script world's CSP does not apply to any
// already-created user script worlds.
IN_PROC_BROWSER_TEST_F(UserScriptWorldBrowserTest,
                       CspUpdatesDoNotApplyToExistingUserScriptWorlds) {
  // Load a simple extension with permission to example.com and navigate a new
  // tab to example.com.
  const Extension* extension =
      LoadExtensionWithHostPermission("http://example.com/*");

  NavigateToURL(embedded_test_server()->GetURL("example.com", "/simple.html"));

  base::Value script_result = ExecuteScriptInUserScriptWorld(
      kCheckIfEvalAllowedScriptSource, *extension);
  EXPECT_EQ(script_result, "disallowed eval");

  // Update the user script world CSP to allow unsafe eval.
  SetDefaultUserScriptWorldProperties(*extension, "script-src 'unsafe-eval'",
                                      /*enable_messaging=*/true);

  // Re-evaluate the script. Eval should still be disallowed since CSP updates
  // do not apply to existing isolated worlds (by design).
  script_result = ExecuteScriptInUserScriptWorld(
      kCheckIfEvalAllowedScriptSource, *extension);
  EXPECT_EQ(script_result, "disallowed eval");
}

// Tests that newly-created documents may greedily initialize isolated world CSP
// values.
IN_PROC_BROWSER_TEST_F(UserScriptWorldBrowserTest,
                       CspMayBeGreedilyInitializedOnDocumentCreation) {
  // Load a simple extension with permission to example.com and navigate a new
  // tab to example.com.
  const Extension* extension =
      LoadExtensionWithHostPermission("http://example.com/*");

  NavigateToURL(embedded_test_server()->GetURL("example.com", "/simple.html"));

  base::Value script_result = ExecuteScriptInUserScriptWorld(
      kCheckIfEvalAllowedScriptSource, *extension);
  EXPECT_EQ(script_result, "disallowed eval");

  // Navigate to create a new document. At this point, no user script code has
  // injected in this new document.
  NavigateToURL(embedded_test_server()->GetURL("example.com", "/simple.html"));
  // Update the user script world CSP to allow unsafe eval.
  SetDefaultUserScriptWorldProperties(*extension, "script-src 'unsafe-eval'",
                                      /*enable_messaging=*/true);

  // Re-evaluate the script. Somewhat surprisingly, eval is still disallowed.
  // This is because the new document greedily instantiates CSP for the current
  // execution world, which, in this case, is the isolated world. This results
  // in the isolated world CSP for the document being set when we navigate,
  // which is before the new CSP is set. While not necessarily desirable, this
  // is largely okay -- the proper CSP will be set whenever a new world is
  // created, and we document that setting the CSP doesn't affect any existing
  // isolated worlds. This test is mostly here for documentation and to
  // highlight behavior changes.
  script_result = ExecuteScriptInUserScriptWorld(
      kCheckIfEvalAllowedScriptSource, *extension);
  EXPECT_EQ(script_result, "disallowed eval");
}

// Tests sending a message from a user script. This is sent via
// runtime.sendMessage from the user script, and should be received via
// runtime.onUserScriptMessage in the background script.
IN_PROC_BROWSER_TEST_F(UserScriptWorldBrowserTest, SendMessageAPI) {
  static constexpr char kManifest[] =
      R"({
           "name": "User Script Extension",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"},
           "host_permissions": ["http://example.com/*"]
         })";
  // The background script will listen for a message from a user script.
  // Upon receiving one, it will validate the message and sender and respond
  // with 'pong'.
  static constexpr char kBackgroundJs[] =
      R"(chrome.runtime.onMessage.addListener((msg, sender, sendResponse) => {
           chrome.test.fail(`Unexpected message received: ${msg}`);
         });
         chrome.runtime.onMessageExternal.addListener(
             (msg, sender, sendResponse) => {
               chrome.test.fail(`Unexpected external message received: ${msg}`);
             });
         chrome.runtime.onUserScriptMessage.addListener(
             (msg, sender, sendResponse) => {
               chrome.test.assertEq('ping', msg);
               const url = new URL(sender.url);
               chrome.test.assertEq('example.com', url.hostname);
               chrome.test.assertEq('/simple.html', url.pathname);
               chrome.test.assertEq(0, sender.frameId);
               chrome.test.assertTrue(!!sender.tab);
               sendResponse('pong');
               chrome.test.succeed();
             });)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Enable messaging.
  SetDefaultUserScriptWorldProperties(*extension, std::nullopt,
                                      /*enable_messaging=*/true);

  NavigateToURL(embedded_test_server()->GetURL("example.com", "/simple.html"));

  // A bit overly nifty: here, we execute a user script that sends a message.
  // Because this an MV3 extension, sendMessage() will return a promise that
  // resolves when the other end responds. The ScriptExecutor will wait for
  // that promise to resolve, so the end value of this script is the response
  // from the background script.
  static constexpr char kScriptSource[] =
      R"(chrome.runtime.sendMessage('ping');)";

  // The ResultCatcher validates the background script checks...
  ResultCatcher result_catcher;

  base::Value script_result =
      ExecuteScriptInUserScriptWorld(kScriptSource, *extension);

  // ...And the script result validates the user script expectation.
  EXPECT_EQ(script_result, "pong");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// Tests opening a message port from a user script. This is sent via
// runtime.connect() from the user script, and should be received via
// runtime.onUserScriptConnect in the background script.
IN_PROC_BROWSER_TEST_F(UserScriptWorldBrowserTest, ConnectAPI) {
  static constexpr char kManifest[] =
      R"({
           "name": "User Script Extension",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"},
           "host_permissions": ["http://example.com/*"]
         })";
  // The background script will listen for a new connection from a user script.
  // Upon one opening, it validates the opener and waits for a new message,
  // then validating the message and responding with 'pong', and then
  // succeeds when the port is disconnected (after having received the message).
  static constexpr char kBackgroundJs[] =
      R"(chrome.runtime.onConnect.addListener((port) => {
           chrome.test.fail(`Unexpected connection received`);
         });
         chrome.runtime.onConnectExternal.addListener((port) => {
           chrome.test.fail(`Unexpected external connection received`);
         });
         chrome.runtime.onUserScriptConnect.addListener((port) => {
           chrome.test.assertEq('myport', port.name);
           const sender = port.sender;
           chrome.test.assertTrue(!!sender);
           const url = new URL(sender.url);
           chrome.test.assertEq('example.com', url.hostname);
           chrome.test.assertEq('/simple.html', url.pathname);
           chrome.test.assertEq(0, sender.frameId);
           chrome.test.assertTrue(!!sender.tab);
           let receivedMsg = false;
           port.onMessage.addListener((msg) => {
             receivedMsg = true;
             chrome.test.assertEq('ping', msg);
             port.postMessage('pong');
           });
           port.onDisconnect.addListener(() => {
             chrome.test.assertTrue(receivedMsg);
             chrome.test.succeed();
           });
         });)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Enable messaging.
  SetDefaultUserScriptWorldProperties(*extension, std::nullopt,
                                      /*enable_messaging=*/true);

  NavigateToURL(embedded_test_server()->GetURL("example.com", "/simple.html"));

  // The user script will open a port, post 'ping', wait for the responding
  // 'pong', and then disconnect the port. We execute this in a promise with
  // the expected resolved value of 'success'.
  static constexpr char kScriptSource[] =
      R"(new Promise((resolve) => {
           let port = chrome.runtime.connect({name: 'myport'});
           port.onMessage.addListener((msg) => {
             if (msg != 'pong') {
               resolve(`Unexpected message: ${msg}`);
               return;
             }
             port.disconnect();
             resolve('success');
           });
           port.postMessage('ping');
         });)";

  // The ResultCatcher validates the background script checks...
  ResultCatcher result_catcher;

  base::Value script_result =
      ExecuteScriptInUserScriptWorld(kScriptSource, *extension);

  // ...And the script result validates the user script expectation.
  EXPECT_EQ(script_result, "success");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// Tests that attempting to message another extension from a user script throws
// an error.
IN_PROC_BROWSER_TEST_F(UserScriptWorldBrowserTest,
                       TryingToSendMessageToOtherExtensionTriggersError) {
  static constexpr char kManifest[] =
      R"({
           "name": "User Script Extension",
           "manifest_version": 3,
           "version": "0.1",
           "host_permissions": ["http://example.com/*"]
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Enable messaging.
  SetDefaultUserScriptWorldProperties(*extension, std::nullopt,
                                      /*enable_messaging=*/true);

  NavigateToURL(embedded_test_server()->GetURL("example.com", "/simple.html"));

  static constexpr char kTrySendMessage[] =
      R"(let targetId = 'a'.repeat(32);
         let errorMsg = /User scripts may not message external extensions./;
         chrome.test.runTests([
           function sendMessageToExternalExtensionThrowsError() {
             chrome.test.assertThrows(chrome.runtime.sendMessage, null,
                                      [targetId, 'test message'], errorMsg);
             chrome.test.succeed();
           },
           function connectToExternalExtensionThrowsError() {
             chrome.test.assertThrows(chrome.runtime.connect, null,
                                      [targetId], errorMsg);
             chrome.test.succeed();
           },
         ]);
         // Eval the script to a non-null value.
         'success';)";

  ResultCatcher result_catcher;
  base::Value script_result =
      ExecuteScriptInUserScriptWorld(kTrySendMessage, *extension);
  EXPECT_EQ(script_result, "success");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// Verifies that messaging APIs are exposed if and only if the user script world
// is configured to allow them.
IN_PROC_BROWSER_TEST_F(UserScriptWorldBrowserTest,
                       MessagingAPIsAreNotExposedIfEnableMessagingIsFalse) {
  const Extension* extension =
      LoadExtensionWithHostPermission("http://example.com/*");

  GURL example_com =
      embedded_test_server()->GetURL("example.com", "/simple.html");

  NavigateToURL(example_com);

  // By default, messaging APIs are not allowed.
  {
    base::Value script_result =
        ExecuteScriptInUserScriptWorld(kGetMessagingProperties, *extension);
    EXPECT_THAT(script_result, base::test::IsJson("[]"));
  }

  // Flip the bit to allow messaging APIs and refresh the page.
  SetDefaultUserScriptWorldProperties(*extension, std::nullopt,
                                      /*enable_messaging=*/true);
  NavigateToURL(example_com);

  // Now, all messaging APIs should be exposed.
  {
    base::Value script_result =
        ExecuteScriptInUserScriptWorld(kGetMessagingProperties, *extension);
    EXPECT_THAT(script_result,
                base::test::IsJson(
                    R"(["sendMessage","onMessage","connect","onConnect"])"));
  }
}

// Tests injection into different user script worlds in the renderer.
IN_PROC_BROWSER_TEST_F(UserScriptWorldBrowserTest,
                       DifferentUserScriptWorldsAreIsolated) {
  const Extension* extension =
      LoadExtensionWithHostPermission("http://example.com/*");

  GURL example_com =
      embedded_test_server()->GetURL("example.com", "/simple.html");

  NavigateToURL(example_com);

  // Code that either sets `flag` to track the number of times script has
  // injected in this world (either initializing to `1` or incrementing the
  // previous value).
  constexpr char kCode[] =
      R"(window.flag = window.flag ? window.flag + 1 : 1;
         window.flag;)";

  // First, inject into "world 1". The return result should be `1`, since this
  // is the first time we're injecting in "world 1".
  EXPECT_EQ(base::Value(1),
            ExecuteScriptInUserScriptWorld(kCode, *extension, "world 1"));

  // Next, inject into "world 2". Since this is a different user script world,
  // the return result should again be `1` -- user scripts worlds are isolated
  // from each other.
  EXPECT_EQ(base::Value(1),
            ExecuteScriptInUserScriptWorld(kCode, *extension, "world 2"));

  // Finally, inject into "world 1" again. The return result should be `2`,
  // since this is the second time we're injecting into this user script world.
  EXPECT_EQ(base::Value(2),
            ExecuteScriptInUserScriptWorld(kCode, *extension, "world 1"));
}

// Tests that different user script worlds have unique configurations for CSP.
IN_PROC_BROWSER_TEST_F(UserScriptWorldBrowserTest,
                       UniquePropertiesPerUserScriptWorld_CSP) {
  // Load a simple extension with permission to example.com and navigate a new
  // tab to example.com.
  const Extension* extension =
      LoadExtensionWithHostPermission("http://example.com/*");

  NavigateToURL(embedded_test_server()->GetURL("example.com", "/simple.html"));

  // Check whether eval is allowed in either "world 1" or "world 2". Neither
  // have been configured, so both should default to disallowing eval.
  EXPECT_EQ(ExecuteScriptInUserScriptWorld(kCheckIfEvalAllowedScriptSource,
                                           *extension, "world 1"),
            "disallowed eval");
  EXPECT_EQ(ExecuteScriptInUserScriptWorld(kCheckIfEvalAllowedScriptSource,
                                           *extension, "world 2"),
            "disallowed eval");

  // Allow eval in "world 1", but leave "world 2" as default (disallowing eval).
  SetUserScriptWorldProperties(*extension, "world 1",
                               "script-src 'unsafe-eval'",
                               /*enable_messaging=*/false);

  // Navigate to create a new isolated world.
  NavigateToURL(embedded_test_server()->GetURL("example.com", "/simple.html"));

  // Check whether eval is allowed again. It should be allowed in world 1,
  // but not in world 2.
  EXPECT_EQ(ExecuteScriptInUserScriptWorld(kCheckIfEvalAllowedScriptSource,
                                           *extension, "world 1"),
            "allowed eval");
  EXPECT_EQ(ExecuteScriptInUserScriptWorld(kCheckIfEvalAllowedScriptSource,
                                           *extension, "world 2"),
            "disallowed eval");
}

// Tests clearing configurations for user script worlds.
IN_PROC_BROWSER_TEST_F(UserScriptWorldBrowserTest,
                       ClearingUserScriptWorldConfigurations) {
  // Load a simple extension with permission to example.com.
  const Extension* extension =
      LoadExtensionWithHostPermission("http://example.com/*");

  // Allow eval in "world 1".
  SetUserScriptWorldProperties(*extension, "world 1",
                               "script-src 'unsafe-eval'",
                               /*enable_messaging=*/false);

  // Navigate to example.com and check the world's CSP.
  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  NavigateToURL(url);
  EXPECT_EQ(ExecuteScriptInUserScriptWorld(kCheckIfEvalAllowedScriptSource,
                                           *extension, "world 1"),
            "allowed eval");

  // Now, clear the configuration for "world 1".
  ClearUserScriptWorldProperties(*extension, "world 1");
  // Ensure the roundtrip to the renderer completes by sending another script
  // to inject. Otherwise, the call to clear the configuration may race with the
  // new document creation and, as described in
  // `CspMayBeGreedilyInitializedOnDocumentCreation`, this may initialize the
  // CSP to the old one.
  EXPECT_EQ("foo",
            ExecuteScriptInUserScriptWorld("'foo';", *extension, "world 1"));

  // Navigate to create a new isolated world.
  NavigateToURL(url);

  // Eval should be disallowed (the default) since the configuration was
  // cleared.
  EXPECT_EQ(ExecuteScriptInUserScriptWorld(kCheckIfEvalAllowedScriptSource,
                                           *extension, "world 1"),
            "disallowed eval");
}

// Tests that different user script worlds have unique configurations for
// enabling messaging.
IN_PROC_BROWSER_TEST_F(UserScriptWorldBrowserTest,
                       UniquePropertiesPerUserScriptWorld_Messaging) {
  const Extension* extension =
      LoadExtensionWithHostPermission("http://example.com/*");

  GURL example_com =
      embedded_test_server()->GetURL("example.com", "/simple.html");

  NavigateToURL(example_com);

  // Check whether messaging APIs are allowed in either "world 1" or "world 2".
  // Neither have been configured, so both should default to disallowing
  // messaging.
  EXPECT_THAT(ExecuteScriptInUserScriptWorld(kGetMessagingProperties,
                                             *extension, "world 1"),
              base::test::IsJson("[]"));
  EXPECT_THAT(ExecuteScriptInUserScriptWorld(kGetMessagingProperties,
                                             *extension, "world 2"),
              base::test::IsJson("[]"));

  // Allow messaging in "world 1", but leave "world 2" as default (disallowing
  // messaging).
  SetUserScriptWorldProperties(*extension, "world 1",
                               /*csp=*/std::nullopt,
                               /*enable_messaging=*/true);

  // Navigate to create a new isolated world.
  NavigateToURL(embedded_test_server()->GetURL("example.com", "/simple.html"));

  // Check whether messaging is allowed again. It should be allowed in world 1,
  // but not in world 2.
  EXPECT_THAT(ExecuteScriptInUserScriptWorld(kGetMessagingProperties,
                                             *extension, "world 1"),
              base::test::IsJson(
                  R"(["sendMessage","onMessage","connect","onConnect"])"));
  EXPECT_THAT(ExecuteScriptInUserScriptWorld(kGetMessagingProperties,
                                             *extension, "world 2"),
              base::test::IsJson("[]"));
}

}  // namespace extensions

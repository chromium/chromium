// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains holistic tests of the bindings infrastructure

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/platform/web_mouse_event.h"

namespace extensions {
namespace {

void MouseDownInWebContents(content::WebContents* web_contents) {
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(10, 10);
  mouse_event.click_count = 1;
  web_contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);
}

void MouseUpInWebContents(content::WebContents* web_contents) {
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(10, 10);
  mouse_event.click_count = 1;
  web_contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);
}

class ExtensionBindingsApiTest : public ExtensionApiTest {
 public:
  ExtensionBindingsApiTest() {}
  ~ExtensionBindingsApiTest() override {}

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionBindingsApiTest);
};

IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest,
                       UnavailableBindingsNeverRegistered) {
  // Test will request the 'storage' permission.
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  ASSERT_TRUE(RunExtensionTest(
      "bindings/unavailable_bindings_never_registered")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest,
                       ExceptionInHandlerShouldNotCrash) {
  ASSERT_TRUE(RunExtensionSubtest(
      "bindings/exception_in_handler_should_not_crash",
      "page.html")) << message_;
}

// Tests that an error raised during an async function still fires
// the callback, but sets chrome.runtime.lastError.
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest, LastError) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bindings").AppendASCII("last_error")));

  // Get the ExtensionHost that is hosting our background page.
  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(browser()->profile());
  extensions::ExtensionHost* host = FindHostWithPath(manager, "/bg.html", 1);

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(host->host_contents(),
                                                   "testLastError()", &result));
  EXPECT_TRUE(result);
}

// Regression test that we don't delete our own bindings with about:blank
// iframes.
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest, AboutBlankIframe) {
  ResultCatcher catcher;
  ExtensionTestMessageListener listener("load", true);

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("bindings")
                                          .AppendASCII("about_blank_iframe")));

  ASSERT_TRUE(listener.WaitUntilSatisfied());

  const Extension* extension = LoadExtension(
        test_data_dir_.AppendASCII("bindings")
                      .AppendASCII("internal_apis_not_on_chrome_object"));
  ASSERT_TRUE(extension);
  listener.Reply(extension->id());

  ASSERT_TRUE(catcher.GetNextResult()) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest,
                       InternalAPIsNotOnChromeObject) {
  ASSERT_TRUE(RunExtensionSubtest(
      "bindings/internal_apis_not_on_chrome_object",
      "page.html")) << message_;
}

// Tests that we don't override events when bindings are re-injected.
// Regression test for http://crbug.com/269149.
// Regression test for http://crbug.com/436593.
// Flaky on Mac. http://crbug.com/733064.
#if defined(OS_MACOSX)
#define MAYBE_EventOverriding DISABLED_EventOverriding
#else
#define MAYBE_EventOverriding EventOverriding
#endif
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest, MAYBE_EventOverriding) {
  ASSERT_TRUE(RunExtensionTest("bindings/event_overriding")) << message_;
  // The extension test removes a window and, during window removal, sends the
  // success message. Make sure we flush all pending tasks.
  base::RunLoop().RunUntilIdle();
}

// Tests the effectiveness of the 'nocompile' feature file property.
// Regression test for http://crbug.com/356133.
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest, Nocompile) {
  ASSERT_TRUE(RunExtensionSubtest("bindings/nocompile", "page.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest, ApiEnums) {
  ASSERT_TRUE(RunExtensionTest("bindings/api_enums")) << message_;
}

// Regression test for http://crbug.com/504011 - proper access checks on
// getModuleSystem().
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest, ModuleSystem) {
  ASSERT_TRUE(RunExtensionTest("bindings/module_system")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest, NoExportOverriding) {
  // We need to create runtime bindings in the web page. An extension that's
  // externally connectable will do that for us.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bindings")
                    .AppendASCII("externally_connectable_everywhere")));

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/extensions/api_test/bindings/override_exports.html"));

  // See chrome/test/data/extensions/api_test/bindings/override_exports.html.
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send("
          "document.getElementById('status').textContent.trim());",
      &result));
  EXPECT_EQ("success", result);
}

IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest, NoGinDefineOverriding) {
  // We need to create runtime bindings in the web page. An extension that's
  // externally connectable will do that for us.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bindings")
                    .AppendASCII("externally_connectable_everywhere")));

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/extensions/api_test/bindings/override_gin_define.html"));
  ASSERT_FALSE(
      browser()->tab_strip_model()->GetActiveWebContents()->IsCrashed());

  // See chrome/test/data/extensions/api_test/bindings/override_gin_define.html.
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send("
          "document.getElementById('status').textContent.trim());",
      &result));
  EXPECT_EQ("success", result);
}

IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest, HandlerFunctionTypeChecking) {
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/extensions/api_test/bindings/handler_function_type_checking.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(web_contents->IsCrashed());
  // See handler_function_type_checking.html.
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send("
          "document.getElementById('status').textContent.trim());",
      &result));
  EXPECT_EQ("success", result);
}

IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest,
                       MoreNativeFunctionInterceptionTests) {
  // We need to create runtime bindings in the web page. An extension that's
  // externally connectable will do that for us.
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("bindings")
                        .AppendASCII("externally_connectable_everywhere")));

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/extensions/api_test/bindings/function_interceptions.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(web_contents->IsCrashed());
  // See function_interceptions.html.
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents, "window.domAutomationController.send(window.testStatus);",
      &result));
  EXPECT_EQ("success", result);
}

class FramesExtensionBindingsApiTest : public ExtensionBindingsApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBindingsApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kDisablePopupBlocking);
  }
};

// This tests that web pages with iframes or child windows pointing at
// chrome-extenison:// urls, both web_accessible and nonexistent pages, don't
// get improper extensions bindings injected while they briefly still point at
// about:blank and are still scriptable by their parent.
//
// The general idea is to load up 2 extensions, one which listens for external
// messages ("receiver") and one which we'll try first faking messages from in
// the web page's iframe, as well as actually send a message from later
// ("sender").
IN_PROC_BROWSER_TEST_F(FramesExtensionBindingsApiTest, FramesBeforeNavigation) {
  // Load the sender and receiver extensions, and make sure they are ready.
  ExtensionTestMessageListener sender_ready("sender_ready", true);
  const Extension* sender = LoadExtension(
      test_data_dir_.AppendASCII("bindings").AppendASCII("message_sender"));
  ASSERT_NE(nullptr, sender);
  ASSERT_TRUE(sender_ready.WaitUntilSatisfied());

  ExtensionTestMessageListener receiver_ready("receiver_ready", false);
  const Extension* receiver =
      LoadExtension(test_data_dir_.AppendASCII("bindings")
                        .AppendASCII("external_message_listener"));
  ASSERT_NE(nullptr, receiver);
  ASSERT_TRUE(receiver_ready.WaitUntilSatisfied());

  // Load the web page which tries to impersonate the sender extension via
  // scripting iframes/child windows before they finish navigating to pages
  // within the sender extension.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/extensions/api_test/bindings/frames_before_navigation.html"));

  bool page_success = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetWebContentsAt(0), "getResult()",
      &page_success));
  EXPECT_TRUE(page_success);

  // Reply to |sender|, causing it to send a message over to |receiver|, and
  // then ask |receiver| for the total message count. It should be 1 since
  // |receiver| should not have received any impersonated messages.
  sender_ready.Reply(receiver->id());
  int message_count = 0;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      ProcessManager::Get(profile())
          ->GetBackgroundHostForExtension(receiver->id())
          ->host_contents(),
      "getMessageCountAfterReceivingRealSenderMessage()", &message_count));
  EXPECT_EQ(1, message_count);
}

IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest, TestFreezingChrome) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/extensions/api_test/bindings/freeze.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_FALSE(web_contents->IsCrashed());
}

// Tests interaction with event filter parsing.
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest, TestEventFilterParsing) {
  ExtensionTestMessageListener listener("ready", false);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("bindings/event_filter")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  ResultCatcher catcher;
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("example.com", "/title1.html"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// crbug.com/733337
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest, ValidationInterception) {
  // We need to create runtime bindings in the web page. An extension that's
  // externally connectable will do that for us.
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("bindings")
                        .AppendASCII("externally_connectable_everywhere")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/extensions/api_test/bindings/validation_interception.html"));
  content::WaitForLoadStop(web_contents);
  ASSERT_FALSE(web_contents->IsCrashed());
  bool caught = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, "domAutomationController.send(caught)", &caught));
  EXPECT_TRUE(caught);
}

IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest, UncaughtExceptionLogging) {
  ASSERT_TRUE(RunExtensionTest("bindings/uncaught_exception_logging"))
      << message_;
}

// Verify that when a web frame embeds an extension subframe, and that subframe
// is the only active portion of the extension, the subframe gets proper JS
// bindings. See https://crbug.com/760341.
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest,
                       ExtensionSubframeGetsBindings) {
  // Load an extension that does not have a background page or popup, so it
  // won't be activated just yet.
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("bindings")
                        .AppendASCII("extension_subframe_gets_bindings"));
  ASSERT_TRUE(extension);

  // Navigate current tab to a web URL with a subframe.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/iframe.html"));

  // Navigate the subframe to the extension URL, which should activate the
  // extension.
  GURL extension_url(extension->GetResourceURL("page.html"));
  ResultCatcher catcher;
  content::NavigateIframeToURL(web_contents, "test", extension_url);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest,
                       ExtensionListenersRemoveContext) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("bindings/listeners_destroy_context"));
  ASSERT_TRUE(extension);

  ExtensionTestMessageListener listener("ready", true);

  // Navigate to a web page with an iframe (the iframe is title1.html).
  GURL main_frame_url = embedded_test_server()->GetURL("a.com", "/iframe.html");
  ui_test_utils::NavigateToURL(browser(), main_frame_url);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = tab->GetMainFrame();
  content::RenderFrameHost* subframe = ChildFrameAt(main_frame, 0);
  content::RenderFrameDeletedObserver subframe_deleted(subframe);

  // Wait for the extension's content script to be ready.
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // It's actually critical to the test that these frames are in the same
  // process, because otherwise a crash in the iframe wouldn't be detectable
  // (since we rely on JS execution in the main frame to tell if the renderer
  // crashed - see comment below).
  content::RenderProcessHost* main_frame_process = main_frame->GetProcess();
  EXPECT_EQ(main_frame_process, subframe->GetProcess());

  ExtensionTestMessageListener failure_listener("failed", false);

  // Tell the extension to register listeners that will remove the iframe, and
  // trigger them.
  listener.Reply("go!");

  // The frame will be deleted.
  subframe_deleted.WaitUntilDeleted();

  // Unfortunately, we don't have a good way of checking if something crashed
  // after the frame was removed. WebContents::IsCrashed() seems like it should
  // work, but is insufficient. Instead, use JS execution as the source of
  // true.
  EXPECT_FALSE(tab->IsCrashed());
  EXPECT_EQ(main_frame_url, main_frame->GetLastCommittedURL());
  EXPECT_EQ(main_frame_process, main_frame->GetProcess());
  bool renderer_valid = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      main_frame, "domAutomationController.send(true);", &renderer_valid));
  EXPECT_TRUE(renderer_valid);
  EXPECT_FALSE(failure_listener.was_satisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest, UseAPIsAfterContextRemoval) {
  EXPECT_TRUE(RunExtensionTest("bindings/invalidate_context")) << message_;
}

// Tests that we don't crash if the extension invalidates the context in a
// callback with a runtime.lastError present. Regression test for
// https://crbug.com/944014.
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest,
                       InvalidateContextInCallbackWithLastError) {
  TestExtensionDir dir;
  dir.WriteManifest(
      R"({
           "name": "Invalidate Context in onDisconnect",
           "version": "0.1",
           "manifest_version": 2,
           "background": {"scripts": ["background.js"]}
         })");

  constexpr char kFrameHtml[] =
      R"(<html>
           <body></body>
           <script src="frame.js"></script>
         </html>)";
  constexpr char kFrameJs[] =
      R"(chrome.tabs.executeScript({code: ''}, () => {
           // We expect a last error to be present, since we don't have access
           // to the tab.
           chrome.test.assertTrue(!!chrome.runtime.lastError);
           // Remove the frame from the DOM. This causes blink to remove the
           // associated script contexts.
           parent.document.body.removeChild(
               parent.document.body.querySelector('iframe'));
         });)";
  constexpr char kBackgroundJs[] =
      R"(let frame = document.createElement('iframe');
         frame.src = 'frame.html';
         let observer = new MutationObserver((mutationList) => {
           for (let mutation of mutationList) {
             if (mutation.removedNodes.length == 0)
               continue;
             chrome.test.assertEq(1, mutation.removedNodes.length);
             chrome.test.assertEq('IFRAME', mutation.removedNodes[0].tagName);
             chrome.test.notifyPass();
             break;
           }
         });
         observer.observe(document.body, {childList: true});
         document.body.appendChild(frame);)";
  dir.WriteFile(FILE_PATH_LITERAL("frame.html"), kFrameHtml);
  dir.WriteFile(FILE_PATH_LITERAL("frame.js"), kFrameJs);
  dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);

  ResultCatcher catcher;
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// TODO(devlin): Can this be combined with
// ExtensionBindingsApiTest.UseAPIsAfterContextRemoval?
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest, UseAppAPIAfterFrameRemoval) {
  ASSERT_TRUE(RunExtensionTest("crazy_extension"));
}

// Tests attaching two listeners from the same extension but different pages,
// then removing one, and ensuring the second is still notified.
// Regression test for https://crbug.com/868763.
IN_PROC_BROWSER_TEST_F(
    ExtensionBindingsApiTest,
    MultipleEventListenersFromDifferentContextsAndTheSameExtension) {
  // A script that listens for tab creation and populates the result in a
  // global variable.
  constexpr char kTestPageScript[] = R"(
    window.tabEventId = -1;
    function registerListener() {
      chrome.tabs.onCreated.addListener((tab) => {
        window.tabEventId = tab.id;
      });
    }
  )";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"(
    {
      "name": "Duplicate event listeners",
      "manifest_version": 2,
      "version": "0.1"
    })");
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"),
                     R"(<html><script src="page.js"></script></html>)");
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), kTestPageScript);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Set up: open two tabs to the same extension page, and wait for each to
  // load.
  const GURL page_url = extension->GetResourceURL("page.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), page_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* first_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), page_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* second_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Initially, there are no listeners registered.
  EventRouter* event_router = EventRouter::Get(profile());
  EXPECT_FALSE(event_router->ExtensionHasEventListener(extension->id(),
                                                       "tabs.onCreated"));

  // Register both lsiteners, and verify they were added.
  ASSERT_TRUE(content::ExecuteScript(first_tab, "registerListener()"));
  ASSERT_TRUE(content::ExecuteScript(second_tab, "registerListener()"));
  EXPECT_TRUE(event_router->ExtensionHasEventListener(extension->id(),
                                                      "tabs.onCreated"));

  // Close one of the extension pages.
  constexpr bool add_to_history = false;
  content::WebContentsDestroyedWatcher watcher(second_tab);
  chrome::CloseWebContents(browser(), second_tab, add_to_history);
  watcher.Wait();
  // Hacky round trip to the renderer to flush IPCs.
  ASSERT_TRUE(content::ExecuteScript(first_tab, ""));

  // Since the second page is still open, the extension should still be
  // registered as a listener.
  EXPECT_TRUE(event_router->ExtensionHasEventListener(extension->id(),
                                                      "tabs.onCreated"));

  // Open a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://newtab"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* new_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // The extension should have been notified about the new tab, and have
  // recorded the result.
  int result_tab_id = -1;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      first_tab, "domAutomationController.send(window.tabEventId)",
      &result_tab_id));
  EXPECT_EQ(SessionTabHelper::IdForTab(new_tab).id(), result_tab_id);
}

// Verifies that user gestures are carried through extension messages.
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest,
                       UserGestureFromExtensionMessageTest) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "User Gesture Content Script",
           "manifest_version": 2,
           "version": "0.1",
           "background": { "scripts": ["background.js"] },
           "content_scripts": [{
             "matches": ["*://*.example.com:*/*"],
             "js": ["content_script.js"],
             "run_at": "document_end"
           }]
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"),
                     R"(const button = document.getElementById('go-button');
                        button.addEventListener('click', () => {
                          chrome.runtime.sendMessage('clicked');
                        });)");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     R"(chrome.runtime.onMessage.addListener((message) => {
                        chrome.test.sendMessage(
                            'Clicked: ' +
                            chrome.test.isProcessingUserGesture());
                        });)");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const GURL url = embedded_test_server()->GetURL(
      "example.com", "/extensions/page_with_button.html");
  ui_test_utils::NavigateToURL(browser(), url);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  {
    // Passing a message without an active user gesture shouldn't result in a
    // gesture being active on the receiving end.
    ExtensionTestMessageListener listener(false);
    content::EvalJsResult result =
        content::EvalJs(tab, "document.getElementById('go-button').click()",
                        content::EXECUTE_SCRIPT_NO_USER_GESTURE);
    EXPECT_TRUE(result.value.is_none());

    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("Clicked: false", listener.message());
  }

  {
    // If there is an active user gesture when the message is sent, we should
    // synthesize a user gesture on the receiving end.
    ExtensionTestMessageListener listener(false);
    content::EvalJsResult result =
        content::EvalJs(tab, "document.getElementById('go-button').click()");
    EXPECT_TRUE(result.value.is_none());

    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("Clicked: true", listener.message());
  }
}

// Verifies that user gestures from API calls are active when the callback is
// triggered.
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest,
                       UserGestureInExtensionAPICallback) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "User Gesture Extension API Callback",
           "manifest_version": 2,
           "version": "0.1"
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), "<html></html>");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const GURL extension_page = extension->GetResourceURL("page.html");
  ui_test_utils::NavigateToURL(browser(), extension_page);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  constexpr char kScript[] =
      R"(chrome.tabs.query({}, (tabs) => {
           let message;
           if (chrome.runtime.lastError)
             message = 'Unexpected error: ' + chrome.runtime.lastError;
           else
             message = 'Has gesture: ' + chrome.test.isProcessingUserGesture();
           domAutomationController.send(message);
         });)";

  {
    // Triggering an API without an active gesture shouldn't result in a
    // gesture in the callback.
    std::string message;
    EXPECT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractString(
        tab, kScript, &message));
    EXPECT_EQ("Has gesture: false", message);
  }
  {
    // If there was an active gesture at the time of the API call, there should
    // be an active gesture in the callback.
    std::string message;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(tab, kScript, &message));
    EXPECT_EQ("Has gesture: true", message);
  }
}

// Tests that a web page can consume a user gesture after an extension sends and
// receives a reply during the same user gesture.
// Regression test for https://crbug.com/921141.
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest,
                       WebUserGestureAfterMessagingCallback) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "User Gesture Messaging Test",
           "version": "0.1",
           "manifest_version": 2,
           "content_scripts": [{
             "matches": ["*://*/*"],
             "js": ["content_script.js"],
             "run_at": "document_start"
           }],
           "background": {
             "scripts": ["background.js"]
           }
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"),
                     R"(window.addEventListener('mousedown', () => {
           chrome.runtime.sendMessage('hello', () => {
             let message = chrome.test.isProcessingUserGesture() ?
                 'got reply' : 'no user gesture';
             chrome.test.sendMessage(message);
           });
         });)");
  test_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      R"(chrome.runtime.onMessage.addListener((message, sender, respond) => {
           respond('reply');
         });
         chrome.test.sendMessage('ready');)");

  const Extension* extension = nullptr;
  {
    ExtensionTestMessageListener listener("ready", false);
    extension = LoadExtension(test_dir.UnpackedPath());
    ASSERT_TRUE(extension);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/extensions/api_test/bindings/user_gesture_test.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  {
    ExtensionTestMessageListener listener("got reply", false);
    listener.set_failure_message("no user gesture");
    MouseDownInWebContents(web_contents);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  MouseUpInWebContents(web_contents);

  EXPECT_EQ("success",
            content::EvalJs(web_contents, "window.getEnteredFullscreen",
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Tests that a web page can consume a user gesture after an extension calls a
// method and receives the response in the callback.
// Regression test for https://crbug.com/921141.
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest,
                       WebUserGestureAfterApiCallback) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "User Gesture Messaging Test",
           "version": "0.1",
           "manifest_version": 2,
           "content_scripts": [{
             "matches": ["*://*/*"],
             "js": ["content_script.js"],
             "run_at": "document_start"
           }],
           "permissions": ["storage"]
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"),
                     R"(window.addEventListener('mousedown', () => {
           chrome.storage.local.get('foo', () => {
             let message = chrome.test.isProcessingUserGesture() ?
                 'got reply' : 'no user gesture';
             chrome.test.sendMessage(message);
           });
         });)");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/extensions/api_test/bindings/user_gesture_test.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  {
    ExtensionTestMessageListener listener("got reply", false);
    listener.set_failure_message("no user gesture");
    MouseDownInWebContents(web_contents);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  MouseUpInWebContents(web_contents);

  EXPECT_EQ("success",
            content::EvalJs(web_contents, "window.getEnteredFullscreen",
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Tests that bindings are properly instantiated for a window navigated to an
// extension URL after being opened with an undefined URL.
// Regression test for https://crbug.com/925118.
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest,
                       TestBindingsAvailableWithNavigatedBlankWindow) {
  constexpr char kManifest[] =
      R"({
           "name": "chrome.runtime bug checker",
           "description": "test case for crbug.com/925118",
           "version": "0",
           "manifest_version": 2
         })";
  constexpr char kOpenerHTML[] =
      R"(<!DOCTYPE html>
         <html>
           <head>
             <script src='opener.js'></script>
           </head>
           <body>
           </body>
         </html>)";
  // opener.js opens a blank window and then navigates it to an extension URL
  // (where extension APIs should be available).
  constexpr char kOpenerJS[] =
      R"(const url = chrome.runtime.getURL('/page.html');
         const win = window.open(undefined, '');
         win.location = url;
         chrome.test.notifyPass())";
  constexpr char kPageHTML[] =
      R"(<!DOCTYPE html>
         <html>
           This space intentionally left blank.
         </html>)";
  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("opener.html"), kOpenerHTML);
  extension_dir.WriteFile(FILE_PATH_LITERAL("opener.js"), kOpenerJS);
  extension_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHTML);

  const Extension* extension = LoadExtension(extension_dir.UnpackedPath());
  const GURL target_url = extension->GetResourceURL("page.html");

  ResultCatcher catcher;
  content::TestNavigationObserver observer(target_url);
  observer.StartWatchingNewWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               extension->GetResourceURL("opener.html"));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(target_url, web_contents->GetLastCommittedURL());

  // Check whether bindings are available. They should be.
  constexpr char kScript[] =
      R"(let message;
         if (!chrome.runtime)
           message = 'Runtime not defined';
         else if (!chrome.tabs)
           message = 'Tabs not defined';
         else
           message = 'success';
         domAutomationController.send(message);)";
  std::string result;
  // Note: Can't use EvalJs() because of CSP in extension pages.
  EXPECT_TRUE(
      content::ExecuteScriptAndExtractString(web_contents, kScript, &result));
  EXPECT_EQ("success", result);
}

// Tests the aliasing of chrome.extension methods to their chrome.runtime
// equivalents.
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest,
                       ChromeExtensionIsAliasedToChromeRuntime) {
  constexpr char kManifest[] =
      R"({
           "name": "Test",
           "version": "0.1",
           "manifest_version": 2,
           "background": { "scripts": ["background.js"] }
         })";
  constexpr char kBackground[] =
      R"(chrome.test.runTests([
           function chromeExtensionIsAliased() {
             // Sanity check: chrome.extension is directly aliased to
             // chrome.runtime.
             chrome.test.assertTrue(!!chrome.runtime);
             chrome.test.assertTrue(!!chrome.runtime.sendMessage);
             chrome.test.assertEq(chrome.runtime.sendMessage,
                                  chrome.extension.sendMessage);
             chrome.test.succeed();
           },
           function testOverridingFailsGracefully() {
             let intercepted = false;
             // Modify the chrome.runtime object, which is the source for the
             // chrome.extension API, to throw an error when sendMessage is
             // accessed. Nothing should blow up.
             // Regression test for https://crbug.com/949170.
             Object.defineProperty(
                 chrome.runtime,
                 'sendMessage',
                 {
                   get() {
                     intercepted = true;
                     throw new Error('Mwahaha');
                   }
                 });
             chrome.extension.sendMessage;
             chrome.test.assertTrue(intercepted);
             chrome.test.succeed();
           }
         ]);)";

  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  ResultCatcher catcher;
  ASSERT_TRUE(LoadExtension(extension_dir.UnpackedPath()));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace
}  // namespace extensions

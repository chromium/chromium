// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/offscreen_document_host.h"
#include "extensions/common/mojom/context_type.mojom.h"

#include "base/test/bind.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/script_result_queue.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

class OffscreenDocumentBrowserTest : public ExtensionApiTest {
 public:
  OffscreenDocumentBrowserTest() = default;
  ~OffscreenDocumentBrowserTest() override = default;

  // Creates a new OffscreenDocumentHost and waits for it to load.
  std::unique_ptr<OffscreenDocumentHost> CreateOffscreenDocument(
      const Extension& extension,
      const GURL& url) {
    scoped_refptr<content::SiteInstance> site_instance =
        ProcessManager::Get(profile())->GetSiteInstanceForURL(url);

    content::TestNavigationObserver navigation_observer(url);
    navigation_observer.StartWatchingNewWebContents();
    auto offscreen_document = std::make_unique<OffscreenDocumentHost>(
        extension, site_instance.get(), url);
    offscreen_document->CreateRendererSoon();
    navigation_observer.Wait();
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());

    return offscreen_document;
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// Test basic properties of offscreen documents.
IN_PROC_BROWSER_TEST_F(OffscreenDocumentBrowserTest,
                       CreateBasicOffscreenDocument) {
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1"
         })";
  static constexpr char kOffscreenDocumentHtml[] =
      R"(<html>
           <body>
             <div id="signal">Hello, World</div>
           </body>
         </html>)";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                     kOffscreenDocumentHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("other.html"), "<html>Empty</html>");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const GURL offscreen_url = extension->GetResourceURL("offscreen.html");
  ProcessManager* const process_manager = ProcessManager::Get(profile());

  std::unique_ptr<OffscreenDocumentHost> offscreen_document =
      CreateOffscreenDocument(*extension, offscreen_url);

  // Check basic properties:
  content::WebContents* contents = offscreen_document->host_contents();
  ASSERT_TRUE(contents);
  // - The URL should match the extension's URL.
  EXPECT_EQ(offscreen_url, contents->GetLastCommittedURL());
  // - The offscreen document should be, well, offscreen; it should not be
  //   contained within any Browser window.
  EXPECT_EQ(nullptr, chrome::FindBrowserWithTab(contents));
  // - The view type should be correctly set (it should not be considered a
  //   background page, tab, or other type of view).
  EXPECT_EQ(mojom::ViewType::kOffscreenDocument,
            offscreen_document->extension_host_type());
  EXPECT_EQ(mojom::ViewType::kOffscreenDocument, GetViewType(contents));
  // The offscreen document should be marked as never composited, excluding it
  // from certain a11y considerations.
  EXPECT_TRUE(contents->GetDelegate()->IsNeverComposited(contents));

  {
    // Check the registration in the ProcessManager: the offscreen document
    // should be associated with the extension and have a registered frame.
    ProcessManager::FrameSet frames_for_extension =
        process_manager->GetRenderFrameHostsForExtension(extension->id());
    ASSERT_EQ(1u, frames_for_extension.size());
    content::RenderFrameHost* frame_host = *frames_for_extension.begin();
    EXPECT_EQ(offscreen_url, frame_host->GetLastCommittedURL());
    EXPECT_EQ(contents, content::WebContents::FromRenderFrameHost(frame_host));
    EXPECT_EQ(extension, process_manager->GetExtensionForWebContents(contents));
  }

  {
    // Check the registration in the ExtensionHostRegistry.
    ExtensionHostRegistry* host_registry =
        ExtensionHostRegistry::Get(profile());
    std::vector<ExtensionHost*> hosts =
        host_registry->GetHostsForExtension(extension->id());
    EXPECT_THAT(hosts, testing::ElementsAre(offscreen_document.get()));
    EXPECT_EQ(offscreen_document.get(),
              host_registry->GetExtensionHostForPrimaryMainFrame(
                  offscreen_document->main_frame_host()));
  }

  {
    mojom::ContextType context_type =
        ProcessMap::Get(profile())->GetMostLikelyContextType(
            extension, contents->GetPrimaryMainFrame()->GetProcess()->GetID(),
            &offscreen_url);
    // TODO(crbug.com/40849649): The following check should be:
    //   EXPECT_EQ(mojom::ContextType::kOffscreenExtension, context_type);
    // However, currently the ProcessMap can't differentiate between a
    // privileged extension context and an offscreen document, as both run in
    // the primary extension process and have committed to the extension origin.
    // This is okay (this boundary isn't a security boundary), but is
    // technically incorrect.
    // See also comment in ProcessMap::GetMostLikelyContextType().
    EXPECT_EQ(mojom::ContextType::kPrivilegedExtension, context_type);
  }

  {
    // Check the document loaded properly (and, implicitly check that it does,
    // in fact, have a DOM).
    static constexpr char kScript[] =
        R"({
             let div = document.getElementById('signal');
             div ? div.innerText : '<no div>';
           })";
    EXPECT_EQ("Hello, World", EvalJs(contents, kScript));
  }

  {
    // Check that the offscreen document runs in the same process as other
    // extension frames. Do this by comparing it to another extension page in
    // a tab.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), extension->GetResourceURL("other.html")));
    content::WebContents* tab_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(tab_contents->GetPrimaryMainFrame()->GetProcess(),
              contents->GetPrimaryMainFrame()->GetProcess());
  }
}

// Tests that extension API access in offscreen documents is extremely limited.
IN_PROC_BROWSER_TEST_F(OffscreenDocumentBrowserTest, APIAccessIsLimited) {
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1",
           "permissions": ["storage", "tabs"]
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                     "<html>Offscreen</html>");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  const GURL offscreen_url = extension->GetResourceURL("offscreen.html");

  std::unique_ptr<OffscreenDocumentHost> offscreen_document =
      CreateOffscreenDocument(*extension, offscreen_url);
  content::WebContents* contents = offscreen_document->host_contents();

  {
    // Offscreen documents have very limited API access. Even though the
    // extension has the storage and tabs permissions, the only extension API
    // exposed should be `runtime` (and our test API).
    constexpr char kScript[] =
        R"({
             let keys = Object.keys(chrome);
             JSON.stringify(keys.sort());
           })";
    EXPECT_EQ(R"(["csi","loadTimes","runtime","test"])",
              EvalJs(contents, kScript));
  }

  {
    // Even runtime should be fairly restricted. Enums are always exposed, and
    // offscreen documents have access to message passing capabilities and their
    // own extension ID and URL. Intentionally absent are methods like
    // `runtime.getViews()`.
    constexpr char kScript[] =
        R"({
             let keys = Object.keys(chrome.runtime);
             JSON.stringify(keys.sort());
           })";
    static constexpr char kExpectedProperties[] =
        // Enums.
        R"(["ContextType","OnInstalledReason","OnRestartRequiredReason",)"
        R"("PlatformArch","PlatformNaclArch","PlatformOs",)"
        R"("RequestUpdateCheckStatus",)"
        // Methods and events.
        R"("connect","dynamicId","getURL","id","onConnect",)"
        R"("onConnectExternal","onMessage","onMessageExternal",)"
        R"("sendMessage"])";
    EXPECT_EQ(kExpectedProperties, EvalJs(contents, kScript));
  }
}

// Exercise message passing between the offscreen document and a corresponding
// service worker.
IN_PROC_BROWSER_TEST_F(OffscreenDocumentBrowserTest, MessagingTest) {
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1",
           "background": { "service_worker": "background.js" }
         })";
  static constexpr char kOffscreenDocumentHtml[] =
      R"(<html>
           Offscreen
           <script src="offscreen.js"></script>
         </html>)";
  // Both the offscreen document and the service worker have methods to send a
  // message and to echo back arguments with a reply.
  static constexpr char kOffscreenDocumentJs[] =
      R"(chrome.runtime.onMessage.addListener((msg, sender, sendResponse) => {
           sendResponse({msg, sender, reply: 'offscreen reply'});
         });
         function sendMessageFromOffscreen() {
           chrome.runtime.sendMessage('message from offscreen', (response) => {
             chrome.test.sendScriptResult(response);
           });
         })";
  static constexpr char kBackgroundJs[] =
      R"(chrome.runtime.onMessage.addListener((msg, sender, sendResponse) => {
           sendResponse({msg, sender, reply: 'background reply'});
         });
         function sendMessageFromBackground() {
           chrome.runtime.sendMessage('message from background', (response) => {
             chrome.test.sendScriptResult(response);
           });
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                     kOffscreenDocumentHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.js"), kOffscreenDocumentJs);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const GURL offscreen_url = extension->GetResourceURL("offscreen.html");

  std::unique_ptr<OffscreenDocumentHost> offscreen_document =
      CreateOffscreenDocument(*extension, offscreen_url);

  {
    // First, try sending a message from the service worker to the offscreen
    // document.
    std::string expected = content::JsReplace(
        R"({
             "msg": "message from background",
             "reply": "offscreen reply",
             "sender": {
               "id": $1,
               "url": $2
             }
           })",
        extension->id(), extension->GetResourceURL("background.js"));
    base::Value result = BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(), "sendMessageFromBackground();",
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    EXPECT_THAT(result, base::test::IsJson(expected));
  }

  {
    // Next, send a message in the other direction, from the offscreen document
    // to the service worker.
    std::string expected = content::JsReplace(
        R"({
             "msg": "message from offscreen",
             "reply": "background reply",
             "sender": {
               "id": $1,
               "origin": $2,
               "url": $3
             }
           })",
        extension->id(), extension->origin(), offscreen_url);
    content::WebContents* contents = offscreen_document->host_contents();
    ScriptResultQueue result_queue;
    content::ExecuteScriptAsync(contents, "sendMessageFromOffscreen();");
    base::Value result = result_queue.GetNextResult();
    EXPECT_THAT(result, base::test::IsJson(expected));
  }
}

// Tests the cross-origin permissions of offscreen documents. While offscreen
// documents have limited API access, they *should* retain the ability to
// bypass CORS requirements if they have the corresponding host permission.
// This is because one of the primary use cases for offscreen documents is
// DOM parsing, which may be done via a fetch() + DOMParser.
IN_PROC_BROWSER_TEST_F(OffscreenDocumentBrowserTest,
                       CrossOriginFetchPermissions) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1",
           "host_permissions": ["http://allowed.example/*"]
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                     "<html>Offscreen</html>");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const GURL offscreen_url = extension->GetResourceURL("offscreen.html");
  std::unique_ptr<OffscreenDocumentHost> offscreen_document =
      CreateOffscreenDocument(*extension, offscreen_url);

  const GURL allowed_url = embedded_test_server()->GetURL(
      "allowed.example", "/extensions/fetch1.html");
  const GURL restricted_url = embedded_test_server()->GetURL(
      "restricted.example", "/extensions/fetch2.html");

  // Sanity check the permissions are as we expect them to be for the given
  // URLs, independent of tab ID.
  const int kTabId = extension_misc::kUnknownTabId;
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            extension->permissions_data()->GetPageAccess(allowed_url, kTabId,
                                                         nullptr));
  EXPECT_EQ(PermissionsData::PageAccess::kDenied,
            extension->permissions_data()->GetPageAccess(restricted_url, kTabId,
                                                         nullptr));

  content::WebContents* contents = offscreen_document->host_contents();
  static constexpr char kFetchScript[] =
      R"((async () => {
           let msg;
           try {
             let res = await fetch($1);
             msg = await res.text();
           } catch (e) {
             msg = e.toString();
           }
           return msg;
         })();)";

  EXPECT_EQ("fetch1 - cat\n",
            EvalJs(contents, content::JsReplace(kFetchScript, allowed_url)));
  EXPECT_EQ("TypeError: Failed to fetch",
            EvalJs(contents, content::JsReplace(kFetchScript, restricted_url)));
}

// Tests that content scripts matching iframes contained within an offscreen
// document execute.
IN_PROC_BROWSER_TEST_F(OffscreenDocumentBrowserTest,
                       ContentScriptsInNestedIframes) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Load an extension that executes a content script on http://allowed.example.
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1",
           "content_scripts": [{
             "matches": ["http://allowed.example/*"],
             "all_frames": true,
             "run_at": "document_end",
             "js": ["content_script.js"]
           }]
         })";
  static constexpr char kOffscreenHtml[] =
      R"(<html>
           <iframe id="allowed-frame" name="allowed-frame"></iframe>
           <iframe id="restricted-frame" name="restricted-frame"></iframe>
         </html>)";
  static constexpr char kContentScriptJs[] =
      R"(let d = document.createElement('div');
         d.id = 'script-div';
         d.textContent = 'injection';
         document.body.appendChild(d);)";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"), kOffscreenHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScriptJs);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const GURL offscreen_url = extension->GetResourceURL("offscreen.html");
  std::unique_ptr<OffscreenDocumentHost> offscreen_document =
      CreateOffscreenDocument(*extension, offscreen_url);
  content::WebContents* contents = offscreen_document->host_contents();

  const GURL allowed_url =
      embedded_test_server()->GetURL("allowed.example", "/title1.html");
  const GURL restricted_url =
      embedded_test_server()->GetURL("restricted.example", "/title2.html");

  // Returns the frame with the matching name within the offscreen document.
  auto get_frame_with_name = [contents](const std::string& name) {
    return content::FrameMatchingPredicate(
        contents->GetPrimaryPage(),
        base::BindRepeating(&content::FrameMatchesName, name));
  };

  // We annoyingly cannot use content::NavigateIframeToURL() because it
  // internally uses eval(), which violates the offscreen document's CSP. So,
  // we roll our own navigation helper.
  auto navigate_frame = [contents](const std::string& frame_id,
                                   const GURL& target_url) {
    static constexpr char kNavigateScript[] =
        R"({
             let iframe = document.getElementById($1);
             iframe.src = $2;
           })";
    content::TestNavigationObserver load_observer(contents);
    content::ExecuteScriptAsyncWithoutUserGesture(
        contents, content::JsReplace(kNavigateScript, frame_id, target_url));
    load_observer.Wait();
  };

  // A helper function to retrieve the text content of the expected injected
  // div, if the div exists.
  auto get_script_div_in_frame = [](content::RenderFrameHost* frame) {
    static constexpr char kGetScriptDiv[] =
        R"(var d = document.getElementById('script-div');
           d ? d.textContent : '<no div>';)";
    return content::EvalJs(frame, kGetScriptDiv).ExtractString();
  };

  // Navigate a frame to a URL that matches an extension content script; the
  // content script should inject.
  {
    navigate_frame("allowed-frame", allowed_url);
    content::RenderFrameHost* allowed_frame =
        get_frame_with_name("allowed-frame");
    ASSERT_TRUE(allowed_frame);
    EXPECT_EQ(allowed_url, allowed_frame->GetLastCommittedURL());
    EXPECT_EQ("injection", get_script_div_in_frame(allowed_frame));
  }

  // Now, navigate a frame to a URL that does *not* match the script; the
  // script shouldn't inject.
  {
    navigate_frame("restricted-frame", restricted_url);
    content::RenderFrameHost* restricted_frame =
        get_frame_with_name("restricted-frame");
    ASSERT_TRUE(restricted_frame);
    EXPECT_EQ(restricted_url, restricted_frame->GetLastCommittedURL());
    EXPECT_EQ("<no div>", get_script_div_in_frame(restricted_frame));
  }
}

// Tests attaching and detaching a devtools window to the offscreen document.
IN_PROC_BROWSER_TEST_F(OffscreenDocumentBrowserTest,
                       AttachingDevToolsInspector) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1"
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                     "<html>offscreen</html>");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const GURL offscreen_url = extension->GetResourceURL("offscreen.html");
  std::unique_ptr<OffscreenDocumentHost> offscreen_document =
      CreateOffscreenDocument(*extension, offscreen_url);
  content::WebContents* contents = offscreen_document->host_contents();

  DevToolsWindowTesting::OpenDevToolsWindowSync(contents, profile(),
                                                /*is_docked=*/true);
  DevToolsWindow* dev_tools_window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  ASSERT_TRUE(dev_tools_window);

  DevToolsWindowTesting::CloseDevToolsWindowSync(dev_tools_window);
  EXPECT_FALSE(DevToolsWindow::GetInstanceForInspectedWebContents(contents));
}

// Tests that navigation is disallowed in offscreen documents.
IN_PROC_BROWSER_TEST_F(OffscreenDocumentBrowserTest, NavigationIsDisallowed) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1"
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                     "<html>offscreen</html>");
  test_dir.WriteFile(FILE_PATH_LITERAL("other.html"),
                     "<html>other page</html>");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const GURL offscreen_url = extension->GetResourceURL("offscreen.html");
  std::unique_ptr<OffscreenDocumentHost> offscreen_document =
      CreateOffscreenDocument(*extension, offscreen_url);
  content::WebContents* contents = offscreen_document->host_contents();

  auto expect_navigation_failure = [contents, offscreen_url](const GURL& url) {
    content::TestNavigationObserver navigation_observer(contents);
    content::ExecuteScriptAsync(
        contents, content::JsReplace("window.location.href = $1;", url));
    navigation_observer.Wait();
    EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
    EXPECT_EQ(offscreen_url,
              contents->GetPrimaryMainFrame()->GetLastCommittedURL());
  };

  // Try to navigate the offscreen document to a web URL. The navigation
  // should fail (it's canceled).
  expect_navigation_failure(
      embedded_test_server()->GetURL("example.com", "/title1.html"));
  // Repeat with an extension resource. This should also fail - we don't allow
  // offscreen documents to navigate themselves, even to another extension
  // resource.
  expect_navigation_failure(extension->GetResourceURL("other.html"));
}

// Tests calling window.close() in an offscreen document.
IN_PROC_BROWSER_TEST_F(OffscreenDocumentBrowserTest, CallWindowClose) {
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1"
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                     "<html>offscreen</html>");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  const GURL offscreen_url = extension->GetResourceURL("offscreen.html");

  {
    std::unique_ptr<OffscreenDocumentHost> offscreen_document =
        CreateOffscreenDocument(*extension, offscreen_url);
    // Create a simple handler for the window.close() call that deletes the
    // document.
    base::RunLoop run_loop;
    auto close_handler = [&run_loop, &offscreen_document](ExtensionHost* host) {
      ASSERT_EQ(offscreen_document.get(), host);
      offscreen_document.reset();
      run_loop.Quit();
    };
    offscreen_document->SetCloseHandler(
        base::BindLambdaForTesting(close_handler));
    content::ExecuteScriptAsync(offscreen_document->host_contents(),
                                "window.close();");
    run_loop.Run();
    // The close handler should have been invoked.
    EXPECT_EQ(nullptr, offscreen_document);
  }

  {
    std::unique_ptr<OffscreenDocumentHost> offscreen_document =
        CreateOffscreenDocument(*extension, offscreen_url);

    // Repeat the test, but don't actually close the document in response to
    // the call (which simulates an asynchronous close). This allows the
    // window to call close() multiple times. Even though it does so, we should
    // only receive the signal from the OffscreenDocumentHost once.
    size_t close_count = 0;
    auto close_handler = [&close_count,
                          &offscreen_document](ExtensionHost* host) {
      ASSERT_EQ(offscreen_document.get(), host);
      ++close_count;
    };
    offscreen_document->SetCloseHandler(
        base::BindLambdaForTesting(close_handler));

    content::WebContents* contents = offscreen_document->host_contents();
    // WebContentsDelegate::CloseContents() isn't guaranteed to be called by the
    // time an ExecuteScript() call returns. Since we're waiting on a callback
    // to *not* be called, we can't use a RunLoop + quit closure. Instead,
    // execute script in the renderer multiple times to ensure all the pipes
    // are appropriately flushed.
    for (int i = 0; i < 20; ++i)
      ASSERT_TRUE(content::ExecJs(contents, "window.close();"));
    // Even though `window.close()` was called 20 times, the close handler
    // should only be invoked once.
    EXPECT_EQ(1u, close_count);
  }
}

}  // namespace extensions

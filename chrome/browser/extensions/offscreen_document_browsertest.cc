// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/offscreen_document_host.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
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
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/script_result_queue.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

class OffscreenDocumentBrowserTest : public ExtensionApiTest {
 public:
  OffscreenDocumentBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsOffscreenDocuments);
  }
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

  // Executes a script in `web_contents` and extracts a string from the
  // result.
  std::string ExecuteScriptSync(content::WebContents* web_contents,
                                const std::string& script) {
    std::string result;
    EXPECT_TRUE(
        content::ExecuteScriptAndExtractString(web_contents, script, &result))
        << script;
    return result;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
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
  EXPECT_EQ(nullptr, chrome::FindBrowserWithWebContents(contents));
  // - The view type should be correctly set (it should not be considered a
  //   background page, tab, or other type of view).
  EXPECT_EQ(mojom::ViewType::kOffscreenDocument,
            offscreen_document->extension_host_type());
  EXPECT_EQ(mojom::ViewType::kOffscreenDocument, GetViewType(contents));

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
    Feature::Context context_type =
        ProcessMap::Get(profile())->GetMostLikelyContextType(
            extension, contents->GetPrimaryMainFrame()->GetProcess()->GetID(),
            &offscreen_url);
    // TODO(https://crbug.com/1339382): The following check should be:
    //   EXPECT_EQ(Feature::OFFSCREEN_EXTENSION_CONTEXT, context_type);
    // However, currently the ProcessMap can't differentiate between a
    // blessed extension context and an offscreen document, as both run in the
    // primary extension process and have committed to the extension origin.
    // This is okay (this boundary isn't a security boundary), but is
    // technically incorrect.
    // See also comment in ProcessMap::GetMostLikelyContextType().
    EXPECT_EQ(Feature::BLESSED_EXTENSION_CONTEXT, context_type);
  }

  {
    // Check the document loaded properly (and, implicitly check that it does,
    // in fact, have a DOM).
    static constexpr char kScript[] =
        R"({
             let div = document.getElementById('signal');
             domAutomationController.send(div ? div.innerText : '<no div>');
           })";
    EXPECT_EQ("Hello, World", ExecuteScriptSync(contents, kScript));
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
             domAutomationController.send(JSON.stringify(keys.sort()));
           })";
    EXPECT_EQ(R"(["csi","loadTimes","runtime","test"])",
              ExecuteScriptSync(contents, kScript));
  }

  {
    // Even runtime should be fairly restricted. Enums are always exposed, and
    // offscreen documents have access to message passing capabilities and their
    // own extension ID and URL. Intentionally absent are methods like
    // `runtime.getViews()`.
    constexpr char kScript[] =
        R"({
             let keys = Object.keys(chrome.runtime);
             domAutomationController.send(JSON.stringify(keys.sort()));
           })";
    static constexpr char kExpectedProperties[] =
        R"(["OnInstalledReason","OnRestartRequiredReason","PlatformArch",)"
        R"("PlatformNaclArch","PlatformOs","RequestUpdateCheckStatus",)"
        R"("connect","getURL","id","onConnect","onMessage","sendMessage"])";
    EXPECT_EQ(kExpectedProperties, ExecuteScriptSync(contents, kScript));
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

}  // namespace extensions

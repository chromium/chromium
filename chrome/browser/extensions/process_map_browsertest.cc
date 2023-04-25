// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ProcessMapBrowserTest : public ExtensionBrowserTest {
 public:
  ProcessMapBrowserTest() = default;
  ProcessMapBrowserTest(const ProcessMapBrowserTest&) = delete;
  ProcessMapBrowserTest& operator=(const ProcessMapBrowserTest&) = delete;
  ~ProcessMapBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Returns the WebContents of the currently-active tab.
  content::WebContents* GetActiveTab() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  int GetActiveMainFrameProcessID() {
    return GetActiveTab()->GetPrimaryMainFrame()->GetProcess()->GetID();
  }

  // Adds a new extension with the given `extension_name` and host permission to
  // the given `host_pattern`.
  const Extension* AddExtensionWithHostPermission(
      base::StringPiece extension_name,
      base::StringPiece host_pattern) {
    static constexpr char kManifestTemplate[] =
        R"({
             "name": "%s",
             "manifest_version": 3,
             "version": "0.1",
             "host_permissions": ["%s"]
           })";
    auto extension_dir = std::make_unique<TestExtensionDir>();
    extension_dir->WriteManifest(base::StringPrintf(
        kManifestTemplate, extension_name.data(), host_pattern.data()));
    const Extension* extension = LoadExtension(extension_dir->UnpackedPath());
    extension_dirs_.push_back(std::move(extension_dir));
    return extension;
  }

  // Adds a new extension with the given `extension_name` and a content script
  // that runs on `content_script_pattern`, sending a message when the script
  // injects.
  const Extension* AddExtensionWithContentScript(
      base::StringPiece extension_name,
      base::StringPiece content_script_pattern) {
    static constexpr char kManifestTemplate[] =
        R"({
             "name": "%s",
             "manifest_version": 3,
             "version": "0.1",
             "content_scripts": [{
               "matches": ["%s"],
               "js": ["script.js"]
             }]
           })";
    auto extension_dir = std::make_unique<TestExtensionDir>();
    extension_dir->WriteManifest(
        base::StringPrintf(kManifestTemplate, extension_name.data(),
                           content_script_pattern.data()));
    extension_dir->WriteFile(FILE_PATH_LITERAL("script.js"),
                             "chrome.test.sendMessage('script injected');");
    const Extension* extension = LoadExtension(extension_dir->UnpackedPath());
    extension_dirs_.push_back(std::move(extension_dir));
    return extension;
  }

  // Adds a new extension with a sandboxed frame, `sandboxed.html`, and a parent
  // page, `parent.html` to host it.
  const Extension* AddExtensionWithSandboxedFrame() {
    static constexpr char kManifest[] =
        R"({
             "name": "Sandboxed Page",
             "manifest_version": 3,
             "version": "0.1",
             "sandbox": {
               "pages": [ "sandboxed.html" ]
             }
           })";
    auto extension_dir = std::make_unique<TestExtensionDir>();
    extension_dir->WriteManifest(kManifest);
    extension_dir->WriteFile(FILE_PATH_LITERAL("sandboxed.html"),
                             "<html>Sandboxed</html>");
    extension_dir->WriteFile(
        FILE_PATH_LITERAL("parent.html"),
        R"(<html><iframe src="sandboxed.html"></iframe></html>)");
    const Extension* extension = LoadExtension(extension_dir->UnpackedPath());
    extension_dirs_.push_back(std::move(extension_dir));
    return extension;
  }

  const Extension* AddExtensionWithWebViewAndOpen() {
    static constexpr char kManifest[] =
        R"({
             "name": "Web View",
             "manifest_version": 2,
             "version": "0.1",
             "app": {
               "background": { "scripts": ["background.js"] }
             },
             "webview": {
               "partitions": [{
                 "name": "foo",
                 "accessible_resources": ["accessible.html"]
               }]
             },
             "permissions": ["webview"]
           })";
    static constexpr char kBackgroundJs[] =
        R"(chrome.app.runtime.onLaunched.addListener(() => {
             chrome.app.window.create('embedder.html', {}, function () {});
           });)";
    static constexpr char kEmbedderHtml[] =
        R"(<html>
           <body>
             <webview partition="foo"></webview>
             <script src="embedder.js"></script>
           </body>
           </html>)";
    static constexpr char kEmbedderJs[] =
        R"(onload = () => {
             let webview = document.querySelector('webview');
             webview.addEventListener('loadstop', () => {
               chrome.test.sendMessage('webview loaded');
             });
             webview.addEventListener('loadabort', (e) => {
               console.error('Webview aborted load: ' + e.toString());
             });
             webview.src = 'accessible.html';
           };)";
    auto extension_dir = std::make_unique<TestExtensionDir>();
    extension_dir->WriteManifest(kManifest);
    extension_dir->WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
    extension_dir->WriteFile(FILE_PATH_LITERAL("embedder.html"), kEmbedderHtml);
    extension_dir->WriteFile(FILE_PATH_LITERAL("embedder.js"), kEmbedderJs);
    extension_dir->WriteFile(FILE_PATH_LITERAL("accessible.html"), "hello");

    ExtensionTestMessageListener webview_listener("webview loaded");
    const Extension* extension = LoadAndLaunchApp(extension_dir->UnpackedPath(),
                                                  /*uses_guest_view=*/true);
    extension_dirs_.push_back(std::move(extension_dir));
    EXPECT_TRUE(webview_listener.WaitUntilSatisfied());

    return extension;
  }

  content::WebContents* GetAppWindowContents() {
    AppWindowRegistry* registry = AppWindowRegistry::Get(profile());
    if (registry->app_windows().size() != 1) {
      ADD_FAILURE() << "Incorrect number of app windows: "
                    << registry->app_windows().size();
      return nullptr;
    }

    return (*registry->app_windows().begin())->web_contents();
  }

  content::WebContents* GetWebViewFromEmbedder(content::WebContents* embedder) {
    std::vector<content::WebContents*> inner_web_contents =
        embedder->GetInnerWebContents();
    if (inner_web_contents.size() != 1) {
      ADD_FAILURE() << "Unexpected number of inner web contents: "
                    << inner_web_contents.size();
      return nullptr;
    }

    content::WebContents* inner_contents = inner_web_contents[0];
    if (!WebViewGuest::FromWebContents(inner_contents)) {
      return nullptr;
    }

    return inner_contents;
  }

  // Opens a new tab to the given `domain`.
  void OpenDomain(base::StringPiece domain) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(domain, "/simple.html")));
  }

  // Opens a new tab to a Web UI page.
  void OpenWebUi() {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://settings")));
  }

  // Opens a new tab to a page in the given `extension`.
  void OpenExtensionPage(const Extension& extension) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), extension.GetResourceURL("manifest.json")));
  }

  // Opens a new tab to the given `domain` and waits for a content script to
  // inject.
  void OpenDomainAndWaitForContentScript(base::StringPiece domain) {
    ExtensionTestMessageListener listener("script injected");
    OpenDomain(domain);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  // Opens a new tab to the page with a sandboxed frame in the given
  // `extension`.
  void OpenExtensionPageWithSandboxedFrame(const Extension& extension) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), extension.GetResourceURL("parent.html")));
  }

  // Determines if a given `frame` is sandboxed. Sandboxed frames don't
  // have access to any special extension APIs, even those that require no
  // specific permissions (like chrome.tabs).
  bool ExtensionFrameIsSandboxed(content::RenderFrameHost* frame) {
    EXPECT_TRUE(frame->GetLastCommittedURL().SchemeIs(kExtensionScheme));

    // Note: it's okay for `chrome` to be defined; it has various
    // unstandardized, non-extension-process stuff (like chrome.csi). We just
    // require the special APIs to be undefined.
    return content::EvalJs(frame, "!chrome || !chrome.tabs;").ExtractBool();
  }

  // Iterates over every context type and checks if it could be hosted given the
  // pairing of `extension` and `process_id`, expecting it to be allowed if and
  // only if the context type is in `allowed_contexts`. `debug_string` is used
  // in a scoped trace to make test failures more meaningful.
  void RunCanProcessHostContextTypeChecks(
      const Extension* extension,
      int process_id,
      const std::vector<Feature::Context>& allowed_contexts,
      base::StringPiece debug_string) {
    std::vector<Feature::Context> all_types = {
        Feature::UNSPECIFIED_CONTEXT,
        Feature::BLESSED_EXTENSION_CONTEXT,
        Feature::UNBLESSED_EXTENSION_CONTEXT,
        Feature::CONTENT_SCRIPT_CONTEXT,
        Feature::WEB_PAGE_CONTEXT,
        Feature::BLESSED_WEB_PAGE_CONTEXT,
        Feature::WEBUI_CONTEXT,
        Feature::WEBUI_UNTRUSTED_CONTEXT,
        Feature::LOCK_SCREEN_EXTENSION_CONTEXT,
        Feature::OFFSCREEN_EXTENSION_CONTEXT,
        Feature::USER_SCRIPT_CONTEXT,
    };

    for (auto context_type : all_types) {
      SCOPED_TRACE(testing::Message()
                   << "Testing Context Type: " << context_type
                   << ", Extension: "
                   << (extension ? extension->name() : "<no extension>")
                   << ", Debug String: " << debug_string);
      bool expected_to_be_allowed =
          base::Contains(allowed_contexts, context_type);
      EXPECT_EQ(expected_to_be_allowed,
                process_map()->CanProcessHostContextType(extension, process_id,
                                                         context_type));
    }
  }

  ProcessMap* process_map() { return ProcessMap::Get(profile()); }

 private:
  // Dirs for our test extensions; these have to stay in-scope for the duration
  // of the test.
  std::vector<std::unique_ptr<TestExtensionDir>> extension_dirs_;
};

// Tests that web pages are not considered privileged extension processes.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       IsPrivilegedExtensionProcess_WebPages) {
  // For fun, make sure an extension with access to the given web page is
  // loaded (just to validate we're not doing anything related to
  // extension permissions in our calculations).
  const Extension* extension =
      AddExtensionWithHostPermission("test", "*://example.com/*");
  ASSERT_TRUE(extension);

  OpenDomain("example.com");

  EXPECT_FALSE(process_map()->IsPrivilegedExtensionProcess(
      *extension, GetActiveMainFrameProcessID()));
}

// Tests the type of contexts that can be hosted in web page processes.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest, CanHostContextType_WebPages) {
  // For fun, make sure an extension with access to the given web page is
  // loaded (just to validate we're not doing anything related to
  // extension permissions in our calculations).
  const Extension* extension =
      AddExtensionWithHostPermission("test", "*://example.com/*");
  ASSERT_TRUE(extension);

  OpenDomain("example.com");
  int web_page_process_id = GetActiveMainFrameProcessID();

  RunCanProcessHostContextTypeChecks(
      extension, web_page_process_id,
      {Feature::CONTENT_SCRIPT_CONTEXT, Feature::USER_SCRIPT_CONTEXT},
      "web page with extension passed");
  RunCanProcessHostContextTypeChecks(
      nullptr, web_page_process_id,
      {Feature::WEB_PAGE_CONTEXT, Feature::WEBUI_UNTRUSTED_CONTEXT},
      "web page without extension passed");
}

// Tests that web ui pages are not considered privileged extension processes.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       IsPrivilegedExtensionProcess_WebUiPages) {
  const Extension* extension =
      AddExtensionWithHostPermission("test", "*://example.com/*");
  ASSERT_TRUE(extension);

  OpenWebUi();

  EXPECT_FALSE(process_map()->IsPrivilegedExtensionProcess(
      *extension, GetActiveMainFrameProcessID()));
}

// Tests the type of processes that can be hosted in web ui processes.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest, CanHostContextType_WebUiPages) {
  const Extension* extension =
      AddExtensionWithHostPermission("test", "*://example.com/*");
  ASSERT_TRUE(extension);

  OpenWebUi();
  int webui_process_id = GetActiveMainFrameProcessID();

  RunCanProcessHostContextTypeChecks(
      extension, webui_process_id,
      {Feature::CONTENT_SCRIPT_CONTEXT, Feature::USER_SCRIPT_CONTEXT},
      "webui page with extension passed");
  RunCanProcessHostContextTypeChecks(nullptr, webui_process_id,
                                     {Feature::WEBUI_CONTEXT},
                                     "webui page without extension passed");
}

// Tests that normal extension pages are considered privileged extension
// processes.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       IsPrivilegedExtensionProcess_ExtensionPages) {
  // Load up two extensions, each with the same permissions.
  const Extension* extension1 =
      AddExtensionWithHostPermission("test1", "*://example.com/*");
  const Extension* extension2 =
      AddExtensionWithHostPermission("test2", "*://example.com/*");
  ASSERT_TRUE(extension1);
  ASSERT_TRUE(extension2);

  // Navigate to a page within the first extension. It should be a privileged
  // page for that extension, but not the other.
  OpenExtensionPage(*extension1);
  EXPECT_TRUE(process_map()->IsPrivilegedExtensionProcess(
      *extension1, GetActiveMainFrameProcessID()));
  EXPECT_FALSE(process_map()->IsPrivilegedExtensionProcess(
      *extension2, GetActiveMainFrameProcessID()));

  // Inversion: Navigate to the page of the second extension. It should be a
  // privileged page in the second, but not the first.
  OpenExtensionPage(*extension2);
  EXPECT_FALSE(process_map()->IsPrivilegedExtensionProcess(
      *extension1, GetActiveMainFrameProcessID()));
  EXPECT_TRUE(process_map()->IsPrivilegedExtensionProcess(
      *extension2, GetActiveMainFrameProcessID()));
}

// Tests the type of contexts that can be hosted in regular extension processes.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       CanHostContextType_ExtensionPages) {
  // Load up two extensions, each with the same permissions.
  const Extension* extension1 =
      AddExtensionWithHostPermission("test1", "*://example.com/*");
  const Extension* extension2 =
      AddExtensionWithHostPermission("test2", "*://example.com/*");
  ASSERT_TRUE(extension1);
  ASSERT_TRUE(extension2);

  // Navigate to a page within the first extension. It should be a privileged
  // page for that extension, but not the other.
  OpenExtensionPage(*extension1);

  int extension1_process_id =
      GetActiveTab()->GetPrimaryMainFrame()->GetProcess()->GetID();

  RunCanProcessHostContextTypeChecks(
      extension1, extension1_process_id,
      {Feature::CONTENT_SCRIPT_CONTEXT, Feature::USER_SCRIPT_CONTEXT,
       Feature::BLESSED_EXTENSION_CONTEXT,
       Feature::OFFSCREEN_EXTENSION_CONTEXT},
      "extension1 page with extension1 passed");
  RunCanProcessHostContextTypeChecks(
      extension2, extension1_process_id,
      {Feature::CONTENT_SCRIPT_CONTEXT, Feature::USER_SCRIPT_CONTEXT},
      "extension1 page with extension2 passed");
  RunCanProcessHostContextTypeChecks(
      nullptr, extension1_process_id, {},
      "extension1 page without extension passed");

  // Inversion: Navigate to the page of the second extension. It should be a
  // privileged page in the second, but not the first.
  OpenExtensionPage(*extension2);

  int extension2_process_id =
      GetActiveTab()->GetPrimaryMainFrame()->GetProcess()->GetID();

  RunCanProcessHostContextTypeChecks(
      extension2, extension2_process_id,
      {Feature::CONTENT_SCRIPT_CONTEXT, Feature::USER_SCRIPT_CONTEXT,
       Feature::BLESSED_EXTENSION_CONTEXT,
       Feature::OFFSCREEN_EXTENSION_CONTEXT},
      "extension2 page with extension2 passed");
  RunCanProcessHostContextTypeChecks(
      extension1, extension2_process_id,
      {Feature::CONTENT_SCRIPT_CONTEXT, Feature::USER_SCRIPT_CONTEXT},
      "extension2 page with extension1 passed");
  RunCanProcessHostContextTypeChecks(
      nullptr, extension2_process_id, {},
      "extension2 page without extension passed");
}

// Tests that a web page with injected content scripts is not considered a
// privileged extension process.
IN_PROC_BROWSER_TEST_F(
    ProcessMapBrowserTest,
    IsPrivilegedExtensionProcess_WebPagesWithContentScripts) {
  const Extension* extension =
      AddExtensionWithContentScript("test", "*://example.com/*");
  ASSERT_TRUE(extension);

  // Navigate to a web page and wait for the content script to inject.
  OpenDomainAndWaitForContentScript("example.com");

  EXPECT_FALSE(process_map()->IsPrivilegedExtensionProcess(
      *extension, GetActiveMainFrameProcessID()));
}

// Tests the type of contexts that can be hosted in a web page process that has
// had a content script injected in it.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       CanHostContextType_WebPagesWithContentScripts) {
  const Extension* extension =
      AddExtensionWithContentScript("test", "*://example.com/*");
  ASSERT_TRUE(extension);

  // Navigate to a web page and wait for the content script to inject.
  OpenDomainAndWaitForContentScript("example.com");
  int page_process_id = GetActiveMainFrameProcessID();

  RunCanProcessHostContextTypeChecks(
      extension, page_process_id,
      {Feature::CONTENT_SCRIPT_CONTEXT, Feature::USER_SCRIPT_CONTEXT},
      "web page with extension passed");
  RunCanProcessHostContextTypeChecks(
      nullptr, page_process_id,
      {Feature::WEB_PAGE_CONTEXT, Feature::WEBUI_UNTRUSTED_CONTEXT},
      "web page without extension passed");
}

// Tests that sandboxed extension frames are considered privileged
// extension processes, since they execute within the same process (even
// though they don't have direct API access). This isn't a security bug
// since any compromised renderer could just access an un-sandboxed context.
// TODO(https://crbug.com/510122): This could change with out-of-process-
// sandboxed-iframes.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       IsPrivilegedExtensionProcess_SandboxedExtensionFrame) {
  const Extension* extension = AddExtensionWithSandboxedFrame();
  ASSERT_TRUE(extension);

  OpenExtensionPageWithSandboxedFrame(*extension);

  content::WebContents* web_contents = GetActiveTab();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* sandboxed_frame =
      content::ChildFrameAt(main_frame, 0);

  EXPECT_FALSE(ExtensionFrameIsSandboxed(main_frame));
  EXPECT_TRUE(ExtensionFrameIsSandboxed(sandboxed_frame));

  int main_frame_process_id = main_frame->GetProcess()->GetID();
  int sandboxed_frame_process_id = sandboxed_frame->GetProcess()->GetID();

  EXPECT_EQ(main_frame_process_id, sandboxed_frame_process_id);
  EXPECT_TRUE(process_map()->IsPrivilegedExtensionProcess(
      *extension, main_frame_process_id));
  EXPECT_TRUE(process_map()->IsPrivilegedExtensionProcess(
      *extension, sandboxed_frame_process_id));
}

// Tests the type of contexts that can be hosted in extension processes with
// a sandboxed process frame.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       CanHostContextType_SandboxedExtensionFrame) {
  const Extension* extension = AddExtensionWithSandboxedFrame();
  ASSERT_TRUE(extension);

  OpenExtensionPageWithSandboxedFrame(*extension);

  content::WebContents* web_contents = GetActiveTab();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* sandboxed_frame =
      content::ChildFrameAt(main_frame, 0);

  EXPECT_FALSE(ExtensionFrameIsSandboxed(main_frame));
  EXPECT_TRUE(ExtensionFrameIsSandboxed(sandboxed_frame));

  int main_frame_process_id = main_frame->GetProcess()->GetID();
  int sandboxed_frame_process_id = sandboxed_frame->GetProcess()->GetID();

  EXPECT_EQ(main_frame_process_id, sandboxed_frame_process_id);

  RunCanProcessHostContextTypeChecks(
      extension, main_frame_process_id,
      {Feature::CONTENT_SCRIPT_CONTEXT, Feature::USER_SCRIPT_CONTEXT,
       Feature::BLESSED_EXTENSION_CONTEXT,
       Feature::OFFSCREEN_EXTENSION_CONTEXT},
      "main frame process with extension passed");
  RunCanProcessHostContextTypeChecks(
      nullptr, main_frame_process_id, {},
      "main frame process without extension passed");

  RunCanProcessHostContextTypeChecks(
      extension, sandboxed_frame_process_id,
      {Feature::CONTENT_SCRIPT_CONTEXT, Feature::USER_SCRIPT_CONTEXT,
       Feature::BLESSED_EXTENSION_CONTEXT,
       Feature::OFFSCREEN_EXTENSION_CONTEXT},
      "sandboxed frame process with extension passed");
  RunCanProcessHostContextTypeChecks(
      nullptr, sandboxed_frame_process_id, {},
      "sandboxed frame process without extension passed");
}

IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       IsPrivilegedExtensionProcess_WebViews) {
  const Extension* extension = AddExtensionWithWebViewAndOpen();
  ASSERT_TRUE(extension);

  content::WebContents* embedder = GetAppWindowContents();
  ASSERT_TRUE(embedder);

  content::WebContents* webview = GetWebViewFromEmbedder(embedder);
  ASSERT_TRUE(webview);

  // The embedder (the app window) should be a privileged extension process,
  // but the webview should not.
  EXPECT_TRUE(process_map()->IsPrivilegedExtensionProcess(
      *extension, embedder->GetPrimaryMainFrame()->GetProcess()->GetID()));
  EXPECT_FALSE(process_map()->IsPrivilegedExtensionProcess(
      *extension, webview->GetPrimaryMainFrame()->GetProcess()->GetID()));
}

IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest, CanHostContextType_WebViews) {
  const Extension* extension = AddExtensionWithWebViewAndOpen();
  ASSERT_TRUE(extension);

  content::WebContents* embedder = GetAppWindowContents();
  ASSERT_TRUE(embedder);

  content::WebContents* webview = GetWebViewFromEmbedder(embedder);
  ASSERT_TRUE(webview);

  // The embedder (the app window) can host any kind of extension context
  // except an unblessed extension context (which is only available to
  // webviews).
  RunCanProcessHostContextTypeChecks(
      extension, embedder->GetPrimaryMainFrame()->GetProcess()->GetID(),
      {Feature::CONTENT_SCRIPT_CONTEXT, Feature::USER_SCRIPT_CONTEXT,
       Feature::BLESSED_EXTENSION_CONTEXT,
       Feature::OFFSCREEN_EXTENSION_CONTEXT},
      "embedder process");

  // The webview can only host content scripts, user scripts, and
  // unblessed extension contexts (accessible resources).
  RunCanProcessHostContextTypeChecks(
      extension, webview->GetPrimaryMainFrame()->GetProcess()->GetID(),
      {Feature::CONTENT_SCRIPT_CONTEXT, Feature::USER_SCRIPT_CONTEXT,
       Feature::UNBLESSED_EXTENSION_CONTEXT},
      "webview process with extension passed");

  // If the extension isn't associated with the call, the webview could only
  // possibly contain web pages and untrusted web ui.
  RunCanProcessHostContextTypeChecks(
      nullptr, webview->GetPrimaryMainFrame()->GetProcess()->GetID(),
      {Feature::WEB_PAGE_CONTEXT, Feature::WEBUI_UNTRUSTED_CONTEXT},
      "webview process without extension passed");
}

}  // namespace extensions

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

    bool is_sandboxed = false;
    // Note: it's okay for `chrome` to be defined; it has various
    // unstandardized, non-extension-process stuff (like chrome.csi). We just
    // require the special APIs to be undefined.
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        frame, "domAutomationController.send(!chrome || !chrome.tabs);",
        &is_sandboxed))
        << "Failed to execute script";
    return is_sandboxed;
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

  EXPECT_EQ(main_frame->GetProcess()->GetID(),
            sandboxed_frame->GetProcess()->GetID());
  EXPECT_TRUE(process_map()->IsPrivilegedExtensionProcess(
      *extension, main_frame->GetProcess()->GetID()));
  EXPECT_TRUE(process_map()->IsPrivilegedExtensionProcess(
      *extension, sandboxed_frame->GetProcess()->GetID()));
}

}  // namespace extensions

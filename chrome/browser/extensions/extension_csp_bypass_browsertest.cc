// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

namespace {

// Returns true if |window.scriptExecuted| is true for the given frame.
bool WasFrameWithScriptLoaded(content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host) {
    return false;
  }
  return content::EvalJs(render_frame_host, "!!window.scriptExecuted")
      .ExtractBool();
}

class ExtensionCSPBypassTest : public ExtensionBrowserTest {
 public:
  ExtensionCSPBypassTest() {}

  ExtensionCSPBypassTest(const ExtensionCSPBypassTest&) = delete;
  ExtensionCSPBypassTest& operator=(const ExtensionCSPBypassTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("same-origin.com", "127.0.0.1");
    host_resolver()->AddRule("cross-origin.com", "127.0.0.1");
    ExtensionBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  const Extension* AddExtension(bool is_component, bool all_urls_permission) {
    TestExtensionDir dir;

    std::string unique_name = base::StringPrintf(
        "component=%d, all_urls=%d", is_component, all_urls_permission);
    auto manifest =
        base::Value::Dict()
            .Set("name", unique_name)
            .Set("version", "1")
            .Set("manifest_version", 2)
            .Set("web_accessible_resources", base::Value::List().Append("*"));

    if (all_urls_permission) {
      manifest.Set("permissions", base::Value::List().Append("<all_urls>"));
    }
    if (is_component) {
      // LoadExtensionAsComponent requires the manifest to contain a key.
      std::string key;
      EXPECT_TRUE(Extension::ProducePEM(unique_name, &key));
      manifest.Set("key", key);
    }

    dir.WriteFile(FILE_PATH_LITERAL("script.js"), "");
    dir.WriteManifest(manifest);

    const Extension* extension = nullptr;
    if (is_component) {
      extension = LoadExtensionAsComponent(dir.UnpackedPath());
    } else {
      extension = LoadExtension(dir.UnpackedPath());
    }
    CHECK(extension);
    temp_dirs_.push_back(std::move(dir));
    return extension;
  }

  bool CanLoadScript(const Extension* extension) {
    content::RenderFrameHost* render_frame_host =
        web_contents()->GetPrimaryMainFrame();
    std::string code = base::StringPrintf(
        R"(
        function canLoadScript() {
          const s = document.createElement('script');
          try {
            s.src = '%s';
            document.body.appendChild(s);
          } catch(e) {
            // Blocked by TrustedTypes CSP.
            return false;
          }

          // Not blocked by CSP.
          return true;
        }
        canLoadScript();
        )",
        extension->GetResourceURL("script.js").spec().c_str());
    return EvalJs(render_frame_host, code).ExtractBool();
  }

  content::RenderFrameHost* GetFrameByName(const std::string& name) {
    return content::FrameMatchingPredicate(
        web_contents()->GetPrimaryPage(),
        base::BindRepeating(&content::FrameMatchesName, name));
  }

 private:
  std::vector<TestExtensionDir> temp_dirs_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionCSPBypassTest, LoadWebAccessibleScript) {
  const Extension* component_ext_with_permission = AddExtension(true, true);
  const Extension* component_ext_without_permission = AddExtension(true, false);
  const Extension* ext_with_permission = AddExtension(false, true);
  const Extension* ext_without_permission = AddExtension(false, false);

  // chrome-extension:-URLs can always bypass CSP in normal pages.
  GURL non_webui_url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_webui_url));

  EXPECT_TRUE(CanLoadScript(component_ext_with_permission));
  EXPECT_TRUE(CanLoadScript(component_ext_without_permission));
  EXPECT_TRUE(CanLoadScript(ext_with_permission));
  EXPECT_TRUE(CanLoadScript(ext_without_permission));

  // chrome-extension:-URLs can never bypass CSP in WebUI.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIExtensionsURL)));

  EXPECT_FALSE(CanLoadScript(component_ext_with_permission));
  EXPECT_FALSE(CanLoadScript(component_ext_without_permission));
  EXPECT_FALSE(CanLoadScript(ext_with_permission));
  EXPECT_FALSE(CanLoadScript(ext_without_permission));
}

// Tests that an extension can add a cross-origin iframe to a page
// whose CSP disallows iframes. Regression test for https://crbug.com/408932.
IN_PROC_BROWSER_TEST_F(ExtensionCSPBypassTest, InjectIframe) {
  // Install an extension that can add a cross-origin iframe to a document.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("csp/add_iframe_extension"));
  ASSERT_TRUE(extension);

  // Navigate to a page that has CSP with 'frame-src: none' to block any
  // iframes. Use the "same-origin.com" hostname as the test will add iframes to
  // "cross-origin.com" to make clear they are cross-origin.
  GURL test_url = embedded_test_server()->GetURL(
      "same-origin.com", "/extensions/csp/page_with_frame_csp.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  // First, verify that adding an iframe to the page from the main world will
  // fail. Add the frame. Its onload event fires even if it's blocked
  // (see https://crbug.com/365457), and reports back.
  EXPECT_EQ(true, content::EvalJs(web_contents(), "addIframe();"));

  // Use WasFrameWithScriptLoaded() to check whether the target frame really
  // loaded.
  content::RenderFrameHost* frame = GetFrameByName("added-by-page");
  ASSERT_TRUE(frame);
  EXPECT_FALSE(WasFrameWithScriptLoaded(frame));

  // Second, verify that adding an iframe to the page from the extension will
  // succeed. Click a button whose event handler runs in the extension's world
  // which bypasses CSP, and adds the iframe.
  content::DOMMessageQueue message_queue;
  EXPECT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.querySelector('#addIframeButton').click();"));
  std::string ack;
  EXPECT_TRUE(message_queue.WaitForMessage(&ack));
  EXPECT_EQ("true", ack);
  frame = GetFrameByName("added-by-extension");
  ASSERT_TRUE(frame);
  EXPECT_TRUE(WasFrameWithScriptLoaded(frame));
}

// CSP:frame-ancestor is not bypassed by extensions.
IN_PROC_BROWSER_TEST_F(ExtensionCSPBypassTest, FrameAncestors) {
  std::string manifest = R"(
    {
      "name": "CSP frame-ancestors",
      "manifest_version": 2,
      "version": "0.1",
      "browser_action": {
       "default_popup": "popup.html"
      }
    }
  )";

  std::string popup = R"(
    <!doctype html>
    <html>
      <iframe src = "$1"></iframe>
    </html>
  )";

  GURL iframe_url = embedded_test_server()->GetURL(
      "/extensions/csp/frame-ancestors-none.html");
  popup = base::ReplaceStringPlaceholders(popup, {iframe_url.spec()}, nullptr);

  TestExtensionDir test_dir;
  test_dir.WriteManifest(manifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"), popup);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(
      "Refused to frame * because an ancestor violates *");

  GURL popup_url = extension->GetResourceURL("popup.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), popup_url));

  // The iframe must be blocked because of CSP.
  ASSERT_TRUE(console_observer.Wait());
  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* child_frame = ChildFrameAt(main_frame, 0);
  EXPECT_EQ(popup_url, main_frame->GetLastCommittedURL());
  EXPECT_EQ(iframe_url, child_frame->GetLastCommittedURL());
  EXPECT_TRUE(child_frame->GetLastCommittedOrigin().opaque());
}

}  // namespace extensions

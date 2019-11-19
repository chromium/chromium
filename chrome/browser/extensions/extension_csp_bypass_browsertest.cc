// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

namespace {

// Returns true if |window.scriptExecuted| is true for the given frame.
bool WasFrameWithScriptLoaded(content::RenderFrameHost* rfh) {
  if (!rfh)
    return false;
  bool loaded = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      rfh, "domAutomationController.send(!!window.scriptExecuted)", &loaded));
  return loaded;
}

class ExtensionCSPBypassTest : public ExtensionBrowserTest {
 public:
  ExtensionCSPBypassTest() {}

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
    auto dir = std::make_unique<TestExtensionDir>();

    std::string unique_name = base::StringPrintf(
        "component=%d, all_urls=%d", is_component, all_urls_permission);
    DictionaryBuilder manifest;
    manifest.Set("name", unique_name)
        .Set("version", "1")
        .Set("manifest_version", 2)
        .Set("web_accessible_resources", ListBuilder().Append("*").Build());

    if (all_urls_permission) {
      manifest.Set("permissions", ListBuilder().Append("<all_urls>").Build());
    }
    if (is_component) {
      // LoadExtensionAsComponent requires the manifest to contain a key.
      std::string key;
      EXPECT_TRUE(Extension::ProducePEM(unique_name, &key));
      manifest.Set("key", key);
    }

    dir->WriteFile(FILE_PATH_LITERAL("script.js"), "");
    dir->WriteManifest(manifest.ToJSON());

    const Extension* extension = nullptr;
    if (is_component) {
      extension = LoadExtensionAsComponent(dir->UnpackedPath());
    } else {
      extension = LoadExtension(dir->UnpackedPath());
    }
    CHECK(extension);
    temp_dirs_.push_back(std::move(dir));
    return extension;
  }

  bool CanLoadScript(const Extension* extension) {
    content::RenderFrameHost* rfh = web_contents()->GetMainFrame();
    std::string code = base::StringPrintf(
        R"(
        var s = document.createElement('script');
        s.src = '%s';
        s.onload = function() {
          // Not blocked by CSP.
          window.domAutomationController.send(true);
        };
        s.onerror = function() {
          // Blocked by CSP.
          window.domAutomationController.send(false);
        };
        document.body.appendChild(s);)",
        extension->GetResourceURL("script.js").spec().c_str());
    bool script_loaded = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(rfh, code, &script_loaded));
    return script_loaded;
  }

  content::RenderFrameHost* GetFrameByName(const std::string& name) {
    return content::FrameMatchingPredicate(
        web_contents(), base::BindRepeating(&content::FrameMatchesName, name));
  }

 private:
  std::vector<std::unique_ptr<TestExtensionDir>> temp_dirs_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionCSPBypassTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionCSPBypassTest, LoadWebAccessibleScript) {
  const Extension* component_ext_with_permission = AddExtension(true, true);
  const Extension* component_ext_without_permission = AddExtension(true, false);
  const Extension* ext_with_permission = AddExtension(false, true);
  const Extension* ext_without_permission = AddExtension(false, false);

  // chrome-extension:-URLs can always bypass CSP in normal pages.
  GURL non_webui_url(embedded_test_server()->GetURL("/empty.html"));
  ui_test_utils::NavigateToURL(browser(), non_webui_url);

  EXPECT_TRUE(CanLoadScript(component_ext_with_permission));
  EXPECT_TRUE(CanLoadScript(component_ext_without_permission));
  EXPECT_TRUE(CanLoadScript(ext_with_permission));
  EXPECT_TRUE(CanLoadScript(ext_without_permission));

  // chrome-extension:-URLs can never bypass CSP in WebUI.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIVersionURL));

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
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(web_contents(),
                                                   "addIframe();", &result));
  EXPECT_TRUE(result);

  // Use WasFrameWithScriptLoaded() to check whether the target frame really
  // loaded.
  content::RenderFrameHost* frame = GetFrameByName("added-by-page");
  ASSERT_TRUE(frame);
  EXPECT_FALSE(WasFrameWithScriptLoaded(frame));

  // Second, verify that adding an iframe to the page from the extension will
  // succeed. Click a button whose event handler runs in the extension's world
  // which bypasses CSP, and adds the iframe.
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(), "document.querySelector('#addIframeButton').click();",
      &result));
  frame = GetFrameByName("added-by-extension");
  ASSERT_TRUE(frame);
  EXPECT_TRUE(WasFrameWithScriptLoaded(frame));
}

}  // namespace extensions

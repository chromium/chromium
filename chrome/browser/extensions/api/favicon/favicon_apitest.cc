// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

class FaviconApiTest : public ExtensionApiTest {
 public:
  FaviconApiTest() = default;

 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      extensions_features::kRestrictGeneratedPathsInWebAccessibleResources};
};

// TODO(crbug.com/40836446): Test is flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Extension DISABLED_Extension
#else
#define MAYBE_Extension Extension
#endif

// Fetch favicon from an extension with the correct permission.
IN_PROC_BROWSER_TEST_F(FaviconApiTest, MAYBE_Extension) {
  // Cache the favicon by loading a test page, then fetch the favicon.
  GURL page_url = embedded_test_server()->GetURL(
      "www.example.com", "/extensions/favicon/test_file.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), page_url));

  ASSERT_TRUE(
      RunExtensionTest("favicon/extension", {.extension_url = "test.html"}))
      << message_;
}

// Fetch favicon when an extension doesn't have the necessary permission.
IN_PROC_BROWSER_TEST_F(FaviconApiTest, Permission) {
  ASSERT_TRUE(RunExtensionTest("favicon/permission_missing",
                               {.extension_url = "test.html"}))
      << message_;
}

// The generated _favicon path is only available to the extension's own origin
// even when web_accessible_resources would otherwise match it.
IN_PROC_BROWSER_TEST_F(FaviconApiTest, NotAccessibleFromWebOrigin) {
  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(R"({
    "name": "Test",
    "version": "1.0",
    "manifest_version": 3,
    "permissions": ["favicon"],
    "sandbox": {
      "pages": ["sandboxed.html"]
    },
    "web_accessible_resources": [{
      "resources": ["*"],
      "matches": ["<all_urls>"]
    }]
  })");
  extension_dir.WriteFile(FILE_PATH_LITERAL("page.html"), "<!doctype html>");
  extension_dir.WriteFile(FILE_PATH_LITERAL("sandboxed.html"),
                          "<!doctype html>");
  const Extension* extension = LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  static constexpr char kLoadImageScript[] = R"(
    new Promise(resolve => {
      const img = document.createElement('img');
      img.onload = () => resolve('load');
      img.onerror = () => resolve('error');
      img.src = $1;
    });
  )";
  GURL page_url = embedded_test_server()->GetURL("example.com", "/simple.html");
  GURL favicon_url =
      extension->ResolveExtensionURL("_favicon/?pageUrl=" + page_url.spec());

  // Loading from the extension's own origin succeeds.
  ASSERT_TRUE(content::NavigateToURL(
      GetActiveWebContents(), extension->ResolveExtensionURL("page.html")));
  EXPECT_EQ("load",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace(kLoadImageScript, favicon_url)));

  // Loading from a sandboxed extension page (opaque origin with extension
  // precursor) succeeds.
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(),
                             extension->ResolveExtensionURL("sandboxed.html")));
  EXPECT_EQ("load",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace(kLoadImageScript, favicon_url)));

  // Loading from a web origin fails even though the path matches a
  // web_accessible_resources entry.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), page_url));
  EXPECT_EQ("error",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace(kLoadImageScript, favicon_url)));
}

// When explicitly declared in web_accessible_resources, _favicon is accessible
// to matching web origins (e.g. for content scripts).
IN_PROC_BROWSER_TEST_F(FaviconApiTest,
                       AccessibleFromWebOriginWhenExplicitlyListed) {
  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(R"({
    "name": "Test",
    "version": "1.0",
    "manifest_version": 3,
    "permissions": ["favicon"],
    "web_accessible_resources": [{
      "resources": ["_favicon/*"],
      "matches": ["<all_urls>"]
    }]
  })");
  const Extension* extension = LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  static constexpr char kLoadImageScript[] = R"(
    new Promise(resolve => {
      const img = document.createElement('img');
      img.onload = () => resolve('load');
      img.onerror = () => resolve('error');
      img.src = $1;
    });
  )";
  GURL page_url = embedded_test_server()->GetURL("example.com", "/simple.html");
  GURL favicon_url =
      extension->ResolveExtensionURL("_favicon/?pageUrl=" + page_url.spec());

  // Loading from a web origin succeeds because _favicon/* was explicitly
  // listed.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), page_url));
  EXPECT_EQ("load",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace(kLoadImageScript, favicon_url)));
}

// Content scripts can fetch favicons when _favicon/* is explicitly listed in
// web_accessible_resources.
IN_PROC_BROWSER_TEST_F(FaviconApiTest, ContentScriptExplicitlyListed) {
  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(R"({
    "name": "Content Script Explicit Favicon",
    "version": "1.0",
    "manifest_version": 3,
    "permissions": ["favicon"],
    "content_scripts": [{
      "matches": ["*://example.com/*"],
      "js": ["content_script.js"],
      "run_at": "document_end"
    }],
    "web_accessible_resources": [{
      "resources": ["_favicon/*"],
      "matches": ["*://example.com/*"]
    }]
  })");
  extension_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    const img = document.createElement('img');
    img.onload = () => chrome.test.sendMessage('load_success');
    img.onerror = () => chrome.test.sendMessage('load_error');
    img.src = chrome.runtime.getURL(
        '_favicon/?pageUrl=' + encodeURIComponent(location.href));
    document.body.appendChild(img);
  )");
  const Extension* extension = LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ExtensionTestMessageListener listener;
  GURL page_url = embedded_test_server()->GetURL("example.com", "/simple.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), page_url));
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("load_success", listener.message());
}

// Content scripts cannot fetch favicons when only a blanket wildcard is
// declared in web_accessible_resources.
IN_PROC_BROWSER_TEST_F(FaviconApiTest, ContentScriptBlockedWithWildcard) {
  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(R"({
    "name": "Content Script Wildcard Favicon",
    "version": "1.0",
    "manifest_version": 3,
    "permissions": ["favicon"],
    "content_scripts": [{
      "matches": ["*://example.com/*"],
      "js": ["content_script.js"],
      "run_at": "document_end"
    }],
    "web_accessible_resources": [{
      "resources": ["*"],
      "matches": ["*://example.com/*"]
    }]
  })");
  extension_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    const img = document.createElement('img');
    img.onload = () => chrome.test.sendMessage('load_success');
    img.onerror = () => chrome.test.sendMessage('load_error');
    img.src = chrome.runtime.getURL(
        '_favicon/?pageUrl=' + encodeURIComponent(location.href));
    document.body.appendChild(img);
  )");
  const Extension* extension = LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ExtensionTestMessageListener listener;
  GURL page_url = embedded_test_server()->GetURL("example.com", "/simple.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), page_url));
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("load_error", listener.message());
}

}  // namespace extensions

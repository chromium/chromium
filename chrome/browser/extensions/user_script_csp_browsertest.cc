// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/manifest.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

// Exercises CSP properties in "user-script" type extensions.
class UserScriptCspBrowserTest : public ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

// Tests that the background service worker has its eval blocked by CSP.
IN_PROC_BROWSER_TEST_F(UserScriptCspBrowserTest, ServiceWorkerEvalBlocked) {
  TestExtensionDir dir;
  dir.WriteManifest(R"({
    "name": "User Script CSP SW Test",
    "manifest_version": 3,
    "version": "1.0",
    "converted_from_user_script": true,
    "background": {
      "service_worker": "sw.js"
    }
  })");
  dir.WriteFile(FILE_PATH_LITERAL("sw.js"), R"(
    try {
      eval('self.foo = 3;');
      chrome.test.fail('Unexpected val success');
    } catch(e) {
      if (e.message.includes('Content Security Policy')) {
        chrome.test.succeed();
      } else {
        chrome.test.fail(`Unexpected eval failure: ${e.message}`);
      }
    }
  )");

  ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_EQ(Manifest::Type::kUserScript, extension->GetType());
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// Tests that an extension page has its eval blocked by CSP.
IN_PROC_BROWSER_TEST_F(UserScriptCspBrowserTest, ExtensionPageEvalBlocked) {
  TestExtensionDir dir;
  dir.WriteManifest(R"({
    "name": "User Script CSP Page Test",
    "manifest_version": 3,
    "version": "1.0",
    "converted_from_user_script": true
  })");
  dir.WriteFile(FILE_PATH_LITERAL("page.html"), R"(
    <html>
    <head>
    <script src="page.js"></script>
    </head>
    <body>Page</body>
    </html>
  )");
  dir.WriteFile(FILE_PATH_LITERAL("page.js"), R"(
    window.onload = function() {
      try {
        eval('window.foo = 3;');
        chrome.test.fail('Unexpected eval success');
      } catch(e) {
        if (e.message.includes('Content Security Policy')) {
          chrome.test.succeed();
        } else {
          chrome.test.fail(`Unexpected eval failure: ${e.message}`);
        }
      }
    };
  )");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_EQ(Manifest::Type::kUserScript, extension->GetType());

  ResultCatcher result_catcher;
  content::WebContents* web_contents = GetActiveWebContents();
  GURL page_url = extension->GetResourceURL("page.html");
  ASSERT_TRUE(NavigateToURL(web_contents, page_url));
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// Tests that a content script has its eval blocked by isolated world CSP.
IN_PROC_BROWSER_TEST_F(UserScriptCspBrowserTest, ContentScriptEvalBlocked) {
  TestExtensionDir dir;
  dir.WriteManifest(R"({
    "name": "User Script CSP CS Test",
    "manifest_version": 3,
    "version": "1.0",
    "converted_from_user_script": true,
    "content_scripts": [{
      "matches": ["*://*/*"],
      "js": ["content_script.js"]
    }]
  })");
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    try {
      eval('window.foo = 3;');
      chrome.test.fail('Unexpected eval success');
    } catch(e) {
      if (e.message.includes('Content Security Policy')) {
        chrome.test.succeed();
      } else {
        chrome.test.fail(`Unexpected eval failure: ${e.message}`);
      }
    }
  )");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_EQ(Manifest::Type::kUserScript, extension->GetType());

  GURL test_url = embedded_test_server()->GetURL("example.com", "/empty.html");
  content::WebContents* web_contents = GetActiveWebContents();

  ResultCatcher result_catcher;
  ASSERT_TRUE(NavigateToURL(web_contents, test_url));
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

}  // namespace extensions

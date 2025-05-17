// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#endif

namespace extensions {
namespace {

class WebAccessibleResourcesApiTest : public ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

// Fetch web accessible resources directly from a file:// page.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesApiTest,
                       FileSchemeInitiators_MainWorld) {
  // Load extension.
  TestExtensionDir extension_dir;
  const char* kManifestStub = R"({
    "name": "Test",
    "version": "0.1",
    "manifest_version": 3,
    "web_accessible_resources": [
      {
        "resources": [ "ok_0.html" ],
        "matches": [ "file://*/*" ]
      },
      {
        "resources": [ "ok_1.html" ],
        "matches": [ "<all_urls>" ]
      },
      {
        "resources": [ "no_0.html" ],
        "matches": [ "http://*.example.com/*" ]
      },
      {
        "resources": [ "no_1.html" ],
        "matches": [ "*://*/*" ]
      }
    ]
  })";
  extension_dir.WriteManifest(kManifestStub);
  extension_dir.WriteFile(FILE_PATH_LITERAL("ok_0.html"), "ok_0.html");
  extension_dir.WriteFile(FILE_PATH_LITERAL("ok_1.html"), "ok_1.html");
  extension_dir.WriteFile(FILE_PATH_LITERAL("no_0.html"), "no_0.html");
  extension_dir.WriteFile(FILE_PATH_LITERAL("no_1.html"), "no_1.html");
  const Extension* extension =
      LoadExtension(extension_dir.UnpackedPath(), {.allow_file_access = true});

  // Navigate to extension's index.html via file:// and test.
  base::FilePath test_page;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_page));
  test_page = test_page.AppendASCII("simple.html");
  GURL gurl = net::FilePathToFileURL(test_page);
  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, gurl));
  static constexpr char kScriptTemplate[] = R"(
    // Verify that web accessible resource can be fetched.
    async function run(isAllowed, filename) {
      return new Promise(async (resolve, reject) => {
        const url = `chrome-extension://%s/${filename}`;

        // Fetch and verify the contents of fetched web accessible resources.
        const verifyFetch = (actual) => {
          if (isAllowed == (filename == actual)) {
            resolve();
          } else {
            reject(`Unexpected result. File: ${filename}. Found: ${actual}`);
          }
        };
        fetch(url)
            .then(result => result.text())
            .catch(error => verifyFetch(error))
            .then(text => verifyFetch(text));
      });
    }

    // Run tests.
    const testCases = [
      [true, 'ok_0.html'],
      [true, 'ok_1.html'],
      [false, 'no_0.html'],
      [false, 'no_1.html']
    ];
    const tests = testCases.map(testCase => run(...testCase));
    Promise.all(tests).then(response => true);
  )";
  std::string script =
      base::StringPrintf(kScriptTemplate, extension->id().c_str());
  ASSERT_TRUE(content::EvalJs(web_contents, script).ExtractBool());
}

#if !BUILDFLAG(IS_ANDROID)
// Test loading of subresources using an initiator coming from a file:// scheme,
// and, notably, from within a content script context.
// TODO(crbug.com/391921606): Port to desktop Android when the chrome.scripting
// API is ported.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesApiTest,
                       FileSchemeInitiators_ContentScript) {
  // Load extension.
  TestExtensionDir test_dir;
  const char* kManifestStub = R"({
    "name": "Test",
    "version": "0.1",
    "manifest_version": 3,
    "background": {"service_worker": "service_worker.js"},
    "host_permissions": ["file:///*"],
    "permissions": ["scripting"],
    "web_accessible_resources": [
      {
        "resources": [ "ok_0.html" ],
        "matches": [ "file://*/*" ]
      },
      {
        "resources": [ "ok_1.html" ],
        "matches": [ "<all_urls>" ]
      },
      {
        "resources": [ "no_0.html" ],
        "matches": [ "http://*.example.com/*" ]
      },
      {
        "resources": [ "no_1.html" ],
        "matches": [ "*://*/*" ]
      }
    ]
  })";
  test_dir.WriteManifest(kManifestStub);
  test_dir.WriteFile(FILE_PATH_LITERAL("ok_0.html"), "ok_0.html");
  test_dir.WriteFile(FILE_PATH_LITERAL("ok_1.html"), "ok_1.html");
  test_dir.WriteFile(FILE_PATH_LITERAL("no_0.html"), "no_0.html");
  test_dir.WriteFile(FILE_PATH_LITERAL("no_1.html"), "no_1.html");
  test_dir.WriteFile(FILE_PATH_LITERAL("service_worker.js"), "");
  const char* kTestJs = R"(
    // Verify that web accessible resource can be fetched.
    async function run(isAllowed, filename) {
      return new Promise(async resolve => {
        const url = chrome.runtime.getURL(filename);

        // Fetch and verify the contents of fetched web accessible resources.
        const verifyFetch = (actual) => {
          chrome.test.assertEq(isAllowed, filename == actual);
          resolve();
        };
        fetch(url)
            .then(result => result.text())
            .catch(error => verifyFetch(error))
            .then(text => verifyFetch(text));
      });
    }

    // Run tests.
    const testCases = [
      [true, 'ok_0.html'],
      [true, 'ok_1.html'],
      [false, 'no_0.html'],
      [false, 'no_1.html']
    ];
    const tests = testCases.map(testCase => run(...testCase));
    Promise.all(tests).then(() => chrome.test.succeed());
  )";
  test_dir.WriteFile(FILE_PATH_LITERAL("test.js"), kTestJs);
  const Extension* extension =
      LoadExtension(test_dir.UnpackedPath(), {.allow_file_access = true});

  // Navigate to extension's index.html via file:// and test.
  ResultCatcher catcher;
  base::FilePath test_page;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_page);
  test_page = test_page.AppendASCII("simple.html");
  GURL gurl = net::FilePathToFileURL(test_page);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  const int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  static constexpr char kScript[] =
      R"((async () => {
        await chrome.scripting.executeScript(
          {target: {tabId: %d}, files: ['test.js']})
      })();)";
  BackgroundScriptExecutor::ExecuteScriptAsync(
      profile(), extension->id(), base::StringPrintf(kScript, tab_id));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Useful for testing web accessible resources loaded from a content script.
class WebAccessibleResourcesDynamicUrlScriptingApiTest
    : public ExtensionApiTest {
 public:
  WebAccessibleResourcesDynamicUrlScriptingApiTest() = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

 protected:
  const Extension* GetExtension(const char* manifest_piece) {
    // manifest.json.
    static constexpr char kManifestStub[] = R"({
      "name": "Test",
      "version": "1.0",
      "manifest_version": 3,
      "host_permissions": ["<all_urls>"],
      "web_accessible_resources": [
        {
          "resources": ["dynamic_web_accessible_resource.html"],
          "matches": ["<all_urls>"],
          "use_dynamic_url": true
        },
        {
          "resources": ["web_accessible_resource.html"],
          "matches": ["<all_urls>"]
        }
      ],
      %s
    })";
    auto kManifest = base::StringPrintf(kManifestStub, manifest_piece);
    test_dir_.WriteManifest(kManifest);

    // content.js
    static constexpr char kTestScript[] = R"(
      // Verify that web accessible resource can be fetched.
      async function run(isAllowed, filename, identifier, query = '') {
        return new Promise(async resolve => {
          // Verify URL.
          let expected = chrome.runtime.getURL(`${filename}${query}`);
          let url = `chrome-extension://${identifier}/${filename}${query}`;
          chrome.test.assertEq(isAllowed, expected == url);

          // Verify contents of fetched web accessible resource.
          const verify = (actual) => {
            chrome.test.assertEq(isAllowed, filename == actual);
            resolve();
          };

          // Fetch web accessible resource.
          fetch(url)
              .then(result => {
                // With `use_dynamic_url` set to `true`, we redirect from the
                // first URL using the GUID to the static extension origin.
                // Ensure query parameters are not lost in the redirect.
                chrome.test.assertEq(new URL(result.url).search, query);
                return result.text();
              })
              .catch(error => verify(error))
              .then(text => verify(text));
        });
      }

      // Verify that identifiers don't match.
      const id = chrome.runtime.id;
      const dynamicId = chrome.runtime.dynamicId;
      chrome.test.assertTrue(id != dynamicId);

      // Run tests with arguments [[isAllowed, filename, identifier]].
      const testCases = [
        [true, 'dynamic_web_accessible_resource.html', dynamicId],
        [true, 'dynamic_web_accessible_resource.html', dynamicId, '?foo=bar'],
        [true, 'web_accessible_resource.html', id],
        [false, 'web_accessible_resource.html', 'error'],
        [false, 'dynamic_web_accessible_resource.html', 'error'],
      ];
      const tests = testCases.map(testCase => run(...testCase));
      Promise.all(tests).then(() => chrome.test.succeed());
    )";

    // Write files and load extension.
    WriteFile(FILE_PATH_LITERAL("content.js"), kTestScript);
    WriteFile(FILE_PATH_LITERAL("dynamic_web_accessible_resource.html"),
              "dynamic_web_accessible_resource.html");
    WriteFile(FILE_PATH_LITERAL("web_accessible_resource.html"),
              "web_accessible_resource.html");
    const Extension* extension = LoadExtension(test_dir_.UnpackedPath());
    return extension;
  }

  // Write file to extension directory.
  void WriteFile(const base::FilePath::CharType* filename,
                 const char* contents) {
    test_dir_.WriteFile(filename, contents);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ScopedCurrentChannel current_channel_{version_info::Channel::CANARY};
  TestExtensionDir test_dir_;
};

// Load dynamic web accessible resource from a content script.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesDynamicUrlScriptingApiTest,
                       ContentScript) {
  static constexpr char kManifest[] = R"(
    "content_scripts": [
      {
        "matches": ["<all_urls>"],
        "js": ["content.js"],
        "run_at": "document_start"
      }
    ]
  )";
  const Extension* extension = GetExtension(kManifest);
  ASSERT_TRUE(extension);

  ResultCatcher catcher;
  GURL gurl = embedded_test_server()->GetURL("example.com", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), gurl));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

#if !BUILDFLAG(IS_ANDROID)
// Load dynamic web accessible resources via chrome.scripting.executeScript().
// TODO(crbug.com/391921606): Port to desktop Android when the chrome.scripting
// API is ported.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesDynamicUrlScriptingApiTest,
                       ExecuteScript) {
  // Load extension.
  WriteFile(FILE_PATH_LITERAL("worker.js"), "// Intentionally blank.");
  static constexpr char kManifest[] = R"(
    "permissions": ["scripting"],
    "background": {"service_worker": "worker.js"}
  )";
  const Extension* extension = GetExtension(kManifest);
  ASSERT_TRUE(extension);

  // Navigate to a non extension page and test.
  ResultCatcher catcher;
  GURL gurl = embedded_test_server()->GetURL("example.com", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  const int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  static constexpr char kScript[] =
      R"((async () => {
        await chrome.scripting.executeScript(
          {target: {tabId: %d}, files: ['content.js']})
      })();)";
  BackgroundScriptExecutor::ExecuteScriptAsync(
      profile(), extension->id(), base::StringPrintf(kScript, tab_id));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace extensions

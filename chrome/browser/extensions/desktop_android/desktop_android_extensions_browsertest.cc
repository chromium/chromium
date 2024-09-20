// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/desktop_android/desktop_android_extension_system.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/embedder_user_script_loader.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_content_script_load_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

class DesktopAndroidExtensionsBrowserTest : public AndroidBrowserTest {
 public:
  DesktopAndroidExtensionsBrowserTest() = default;
  ~DesktopAndroidExtensionsBrowserTest() override = default;

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Returns the main `BrowserContext`.
  content::BrowserContext* GetBrowserContext() {
    return GetActiveWebContents()->GetBrowserContext();
  }

  // Attempts to parse and load an extension from the given `file_path` and add
  // it to the extensions system (which will also activate the extension).
  // Returns the extension on success; on failure, returns null and adds a test
  // failure.
  const Extension* LoadExtensionFromDirectory(const base::FilePath& file_path) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    std::string load_error;
    scoped_refptr<Extension> extension = file_util::LoadExtension(
        file_path, mojom::ManifestLocation::kUnpacked, 0, &load_error);
    if (!extension) {
      ADD_FAILURE() << "Failed to parse extension: " << load_error;
      return nullptr;
    }

    content::BrowserContext* browser_context =
        GetActiveWebContents()->GetBrowserContext();

    auto* android_system = static_cast<DesktopAndroidExtensionSystem*>(
        ExtensionSystem::Get(browser_context));
    std::string error;
    if (!android_system->AddExtension(extension, error)) {
      ADD_FAILURE() << "Failed to add extension: " << error;
    }

    ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);
    if (!registry->enabled_extensions().Contains(extension->id())) {
      ADD_FAILURE() << "Extension is not properly enabled.";
      return nullptr;
    }

    return extension.get();
  }
};

// The following is a simple test exercising a basic navigation and script
// injection. This doesn't exercise any extensions logic, but ensures Chrome
// successfully starts and can navigate the web.
IN_PROC_BROWSER_TEST_F(DesktopAndroidExtensionsBrowserTest, SanityCheck) {
  ASSERT_EQ(TabModelList::models().size(), 1u);

  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      embedded_test_server()->GetURL("example.com", "/title1.html")));

  EXPECT_EQ("This page has no title.",
            content::EvalJs(GetActiveWebContents(), "document.body.innerText"));
}

// Tests the ability to parse and validate a simple extension.
IN_PROC_BROWSER_TEST_F(DesktopAndroidExtensionsBrowserTest,
                       ParseAndValidateASimpleExtension) {
  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 3
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);

  scoped_refptr<const Extension> extension =
      LoadExtensionFromDirectory(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Validate the fields in the extension.
  EXPECT_EQ("Test Extension", extension->name());
  EXPECT_EQ("0.1", extension->version().GetString());
  EXPECT_EQ(3, extension->manifest_version());
}

// Tests the adding an extension to the registry and navigating to a
// corresponding page in the extension, verifying the expected content, and
// leveraging the chrome.test API to pass a result. The latter verifies the
// core extension bindings system and API handling works, including
// exercising custom bindings.
IN_PROC_BROWSER_TEST_F(DesktopAndroidExtensionsBrowserTest,
                       NavigateToExtensionPage) {
  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 3
         })";
  static constexpr char kPageHtml[] =
      R"(<html>
           Hello, world
           <script src="page.js"></script>
         </html>)";
  static constexpr char kPageJs[] =
      R"(chrome.test.runTests([
           function sanityCheck() {
             chrome.test.assertEq(2, 1 + 1);
             chrome.test.succeed();
           }
         ]);)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), kPageJs);

  scoped_refptr<const Extension> extension =
      LoadExtensionFromDirectory(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL extension_page = extension->GetResourceURL("page.html");

  ResultCatcher result_catcher;
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(), extension_page));
  EXPECT_EQ(extension_page, GetActiveWebContents()->GetLastCommittedURL());
  EXPECT_EQ("Hello, world",
            content::EvalJs(GetActiveWebContents(), "document.body.innerText"));
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// Test service worker-based extensions properly load and have the service
// worker initialize and run.
IN_PROC_BROWSER_TEST_F(DesktopAndroidExtensionsBrowserTest,
                       ServiceWorkerBasedExtension) {
  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 3,
           "background": {"service_worker": "background.js"}
         })";
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function sanityCheck() {
             chrome.test.assertEq(2, 1 + 1);
             chrome.test.succeed();
           }
         ]);)";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);

  ResultCatcher result_catcher;
  scoped_refptr<const Extension> extension =
      LoadExtensionFromDirectory(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// Tests the declarative net request API in extension service workers.
IN_PROC_BROWSER_TEST_F(DesktopAndroidExtensionsBrowserTest,
                       DeclarativeNetRequestSupport) {
  // Load a simple extension that redirects from example.com -> google.com.
  static constexpr char kManifest[] =
      R"({
           "name": "My Test Extension",
           "manifest_version": 3,
           "version": "0.1",
           "declarative_net_request": {
             "rule_resources": [{
               "id": "ruleset",
               "enabled": true,
               "path": "rules.json"
             }]
           },
           "permissions": ["declarativeNetRequest"],
           "host_permissions": ["*://example.com/*"]
         })";

  // One rule, which is a redirect from example.com to
  // http://google.com:<portid>/title2.html.
  static constexpr char kRulesJson[] =
      R"([{
           "id": 1,
           "priority": 1,
           "action": {
             "type": "redirect",
             "redirect": { "url": "http://google.com:%d/title2.html" }
           },
           "condition": {
             "urlFilter": "example.com",
             "resourceTypes": ["main_frame"]
           }
         }])";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(
      FILE_PATH_LITERAL("rules.json"),
      base::StringPrintf(kRulesJson, embedded_test_server()->port()));

  declarative_net_request::RulesetManagerObserver ruleset_manager_observer(
      declarative_net_request::RulesMonitorService::Get(GetBrowserContext())
          ->ruleset_manager());

  const Extension* extension =
      LoadExtensionFromDirectory(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ruleset_manager_observer.WaitForExtensionsWithRulesetsCount(1);

  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  GURL expected_url =
      embedded_test_server()->GetURL("google.com", "/title2.html");

  EXPECT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), url, expected_url));
  EXPECT_EQ(expected_url, GetActiveWebContents()->GetLastCommittedURL());
  EXPECT_EQ("This page has a title.",
            content::EvalJs(GetActiveWebContents(), "document.body.innerText"));
}

// Verifies content scripts are properly injected.
IN_PROC_BROWSER_TEST_F(DesktopAndroidExtensionsBrowserTest,
                       ContentScriptInjection) {
  // An extension that injects a simple script on match.test sites.
  static constexpr char kManifest[] =
      R"({
           "name": "Content Script Extension",
           "manifest_version": 3,
           "version": "0.1",
           "content_scripts": [{
             "matches": ["*://match.test/*"],
             "js": ["content_script.js"],
             "run_at": "document_idle"
           }]
         })";
  // The script just appends a span to indicate it injected.
  static constexpr char kContentScriptJs[] =
      R"(let span = document.createElement('span');
         span.textContent = 'content script injected';
         document.body.appendChild(span);)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScriptJs);

  const Extension* extension =
      LoadExtensionFromDirectory(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Verify scripts were properly parsed.
  const UserScriptList& content_scripts =
      ContentScriptsInfo::GetContentScripts(extension);
  ASSERT_EQ(1u, content_scripts.size());

  // Wait for scripts to load (if they haven't already).
  UserScriptManager* user_script_manager =
      ExtensionSystem::Get(GetBrowserContext())->user_script_manager();
  ExtensionUserScriptLoader* user_script_loader =
      user_script_manager->GetUserScriptLoaderForExtension(extension->id());
  if (!user_script_loader->HasLoadedScripts()) {
    ContentScriptLoadWaiter waiter(user_script_loader);
    waiter.Wait();
  }

  // First, navigate to a site that *doesn't* match the extension. The script
  // should not inject.
  const GURL no_match_test =
      embedded_test_server()->GetURL("no-match.test", "/title1.html");
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(), no_match_test));
  EXPECT_EQ(no_match_test, GetActiveWebContents()->GetLastCommittedURL());
  EXPECT_EQ("This page has no title.",
            content::EvalJs(GetActiveWebContents(), "document.body.innerText"));

  // Next, navigate to a site that *does* match. The script should inject.
  const GURL match_test =
      embedded_test_server()->GetURL("match.test", "/title1.html");
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(), match_test));
  EXPECT_EQ(match_test, GetActiveWebContents()->GetLastCommittedURL());
  EXPECT_EQ("This page has no title. content script injected",
            content::EvalJs(GetActiveWebContents(), "document.body.innerText"));
}

}  // namespace extensions

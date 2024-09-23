// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions/permissions_updater.h"

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/permissions/permissions_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/script_injection_tracker.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {

class PermissionsUpdaterBrowserTest : public ExtensionBrowserTest {
 public:
  PermissionsUpdaterBrowserTest() = default;
  PermissionsUpdaterBrowserTest(const PermissionsUpdaterBrowserTest&) = delete;
  const PermissionsUpdaterBrowserTest& operator=(
      const PermissionsUpdaterBrowserTest&) = delete;
  ~PermissionsUpdaterBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
  }

  // Returns the current active web contents.
  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

// Tests that updating the permissions of a disabled extension doesn't update
// the renderer.
IN_PROC_BROWSER_TEST_F(PermissionsUpdaterBrowserTest,
                       UpdatePermissions_DisabledExtension) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Step 1: Install extension with <all_urls> optional host permission and
  // dynamic content script with a.com matches.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
        {
          "name": "ScriptingAPI - Update host permissions",
          "version": "1.0",
          "manifest_version": 3,
          "permissions": [ "scripting" ],
          "optional_host_permissions": ["<all_urls>"],
          "background": { "service_worker": "worker.js" }
        } )";
  const char kWorkerScript[] = R"(
        var scripts = [{
          id: 'script1',
          matches: ['*://a.com/*'],
          js: ['content_script.js'],
          runAt: 'document_end'
        }];

        chrome.runtime.onInstalled.addListener(function(details) {
          chrome.scripting.registerContentScripts(scripts, () => {
            chrome.test.sendMessage('SCRIPT_LOADED');
          });
        }); )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorkerScript);
  const char kContentScript[] = R"(
        window.didInjectContentScript = true;
    )";
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScript);

  ExtensionTestMessageListener script_loaded_listener("SCRIPT_LOADED");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(script_loaded_listener.WaitUntilSatisfied());

  // Step 2: Navigate to a.com.
  GURL optional_url = embedded_test_server()->GetURL("a.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), optional_url));

  // Verify extension's active permissions don't include a.com and renderer
  // hasn't injected the extension's script.
  EXPECT_FALSE(extension->permissions_data()
                   ->active_permissions()
                   .HasEffectiveAccessToURL(optional_url));
  content::WebContents* web_contents = GetActiveWebContents();
  EXPECT_EQ(base::Value(),
            content::EvalJs(web_contents, "window.didInjectContentScript"));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  // Step 3: Disable extension.
  DisableExtension(extension->id());

  // Step 4: Grant optional permissions.
  permissions_test_util::GrantOptionalPermissionsAndWaitForCompletion(
      profile(), *extension,
      PermissionsParser::GetOptionalPermissions(extension));

  // Verify extension's active permissions include a.com. However, since
  // extension is disabled, renderer doesn't inject the extensions's script.
  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  .HasEffectiveAccessToURL(optional_url));
  EXPECT_EQ(base::Value(),
            content::EvalJs(web_contents, "window.didInjectContentScript"));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));
}

}  // namespace extensions

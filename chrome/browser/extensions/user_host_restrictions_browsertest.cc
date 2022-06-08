// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

// A parameterized test suite exercising user host restrictions. The param
// controls if the feature is enabled; user host restrictions should not be
// taken into account if the feature is disabled.
class UserHostRestrictionsBrowserTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<bool> {
 public:
  UserHostRestrictionsBrowserTest() {
    const base::Feature& feature =
        extensions_features::kExtensionsMenuAccessControl;
    if (GetParam())
      feature_list_.InitAndEnableFeature(feature);
    else
      feature_list_.InitAndDisableFeature(feature);
  }
  ~UserHostRestrictionsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
  }

  content::WebContents* GetActiveTab() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  int GetActiveTabId() {
    return sessions::SessionTabHelper::IdForTab(GetActiveTab()).id();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, UserHostRestrictionsBrowserTest, testing::Bool());

// Tests that extensions cannot run on user-restricted sites. This specifically
// checks browser-side permissions restrictions (with the
// chrome.scripting.executeScript() method).
IN_PROC_BROWSER_TEST_P(UserHostRestrictionsBrowserTest,
                       ExtensionsCannotRunOnUserRestrictedSites_BrowserCheck) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 3,
           "permissions": ["scripting"],
           "host_permissions": ["<all_urls>"],
           "background": {"service_worker": "background.js"}
         })";

  static constexpr char kBackground[] =
      R"(// Attempts to execute a script on the given `tabId` passing either the
         // result of the execution or the error encountered back as the script
         // result.
         async function tryExecuteScript(tabId) {
           let result;
           try {
             let injectionResult =
                 await chrome.scripting.executeScript(
                     {
                       target: {tabId},
                       func: () => { return location.href; }
                     });
             result = injectionResult[0].result;
           } catch (e) {
             result = e.toString();
           }
           chrome.test.sendScriptResult(result);
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  auto try_execute_script = [this, extension](int tab_id) {
    static constexpr char kScript[] = "tryExecuteScript(%d)";
    base::Value result = BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(), base::StringPrintf(kScript, tab_id),
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    return result.is_string() ? result.GetString() : "<invalid result>";
  };

  const GURL allowed_url =
      embedded_test_server()->GetURL("allowed.example", "/title1.html");
  const GURL restricted_url =
      embedded_test_server()->GetURL("restricted.example", "/title2.html");

  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  permissions_manager->AddUserRestrictedSite(
      url::Origin::Create(restricted_url));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), allowed_url));
  EXPECT_EQ(allowed_url.spec(), try_execute_script(GetActiveTabId()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), restricted_url));

  // The extension should not be able to run on the user-restricted site iff
  // the feature is enabled.
  if (GetParam()) {
    EXPECT_EQ("Error: Blocked", try_execute_script(GetActiveTabId()));
  } else {
    EXPECT_EQ(restricted_url.spec(), try_execute_script(GetActiveTabId()));
  }
}

// Tests that extensions cannot run on user-restricted sites. This specifically
// checks renderer-side permissions restrictions (with content scripts).
IN_PROC_BROWSER_TEST_P(UserHostRestrictionsBrowserTest,
                       ExtensionsCannotRunOnUserRestrictedSites_RendererCheck) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 3,
           "content_scripts": [{
             "matches": ["<all_urls>"],
             "js": ["content_script.js"],
             "run_at": "document_end"
           }]
         })";

  // Change the page title if the script is injected. Since the script is
  // injected at document_end (which happens before the page completes loading),
  // there shouldn't be a race condition in our checks.
  static constexpr char kContentScript[] = "document.title = 'Injected';";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScript);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const GURL allowed_url =
      embedded_test_server()->GetURL("allowed.example", "/title1.html");
  const GURL restricted_url =
      embedded_test_server()->GetURL("restricted.example", "/title2.html");

  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  permissions_manager->AddUserRestrictedSite(
      url::Origin::Create(restricted_url));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), allowed_url));
  static constexpr char16_t kInjectedTitle[] = u"Injected";
  EXPECT_EQ(kInjectedTitle, GetActiveTab()->GetTitle());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), restricted_url));

  // The extension should not be able to run on the user-restricted site iff
  // the feature is enabled.
  if (GetParam()) {
    EXPECT_EQ(u"Title Of Awesomeness", GetActiveTab()->GetTitle());
  } else {
    EXPECT_EQ(kInjectedTitle, GetActiveTab()->GetTitle());
  }
}

}  // namespace extensions

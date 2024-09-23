// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/api_permission_id.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// A parameterized test suite exercising user host restrictions. The param
// controls if the feature is enabled; user host restrictions should not be
// taken into account if the feature is disabled.
class UserHostRestrictionsBrowserTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<bool> {
 public:
  UserHostRestrictionsBrowserTest() {
    feature_list_.InitWithFeatureState(
        extensions_features::kExtensionsMenuAccessControl, GetParam());
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

  // Withholds host permissions from `extension` and waits for the withholding
  // to take effect.
  void WithholdExtensionPermissions(const Extension& extension) {
    // Withhold extension host permissions. Wait for the notification to be
    // fired to ensure all renderers and services have been properly updated.
    PermissionsManagerWaiter waiter(PermissionsManager::Get(profile()));
    ScriptingPermissionsModifier(profile(), &extension)
        .SetWithholdHostPermissions(true);
    waiter.WaitForExtensionPermissionsUpdate();
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

// Ensures user host restrictions are properly propagated to the network
// service. Since fetch() permissions are controlled here, a cross-origin
// fetch() is a suitable exercise.
IN_PROC_BROWSER_TEST_P(
    UserHostRestrictionsBrowserTest,
    ExtensionsCannotRunOnUserRestrictedSites_NetworkService) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 3,
           "background": {"service_worker": "background.js"},
           "host_permissions": ["<all_urls>"]
         })";

  static constexpr char kBackground[] =
      R"(// Attempts to execute a script on the given `tabId` passing either the
         // result of the execution or the error encountered back as the script
         // result.
         async function tryFetchUrl(url) {
           let result;
           try {
             let fetchResult = await fetch(url);
             result = await fetchResult.text();
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

  auto try_fetch_url = [this, extension](const GURL& url) {
    base::Value result = BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(), content::JsReplace("tryFetchUrl($1)", url),
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    return result.is_string() ? result.GetString() : "<invalid result>";
  };

  const GURL allowed_url = embedded_test_server()->GetURL(
      "allowed.example", "/extensions/fetch1.html");
  const GURL restricted_url = embedded_test_server()->GetURL(
      "restricted.example", "/extensions/fetch2.html");

  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  {
    PermissionsManagerWaiter waiter(permissions_manager);
    permissions_manager->AddUserRestrictedSite(
        url::Origin::Create(restricted_url));
    waiter.WaitForUserPermissionsSettingsChange();
  }

  EXPECT_EQ("fetch1 - cat\n", try_fetch_url(allowed_url));

  // The extension should not be able to fetch the user-restricted site iff
  // the feature is enabled.
  if (GetParam()) {
    EXPECT_EQ("TypeError: Failed to fetch", try_fetch_url(restricted_url));
  } else {
    EXPECT_EQ("fetch2 - dog\n", try_fetch_url(restricted_url));
  }
}

class UserHostRestrictionsWithPermittedSitesBrowserTest
    : public UserHostRestrictionsBrowserTest {
 public:
  UserHostRestrictionsWithPermittedSitesBrowserTest();
  UserHostRestrictionsWithPermittedSitesBrowserTest(
      const UserHostRestrictionsWithPermittedSitesBrowserTest&) = delete;
  const UserHostRestrictionsWithPermittedSitesBrowserTest& operator=(
      const UserHostRestrictionsWithPermittedSitesBrowserTest&) = delete;
  ~UserHostRestrictionsWithPermittedSitesBrowserTest() override = default;

  // Adds `url` as a new user-permitted site and waits for the change to take
  // effect.
  void AddUserPermittedSite(const GURL& url) {
    PermissionsManager* permissions_manager =
        PermissionsManager::Get(profile());
    PermissionsManagerWaiter waiter(permissions_manager);
    permissions_manager->AddUserPermittedSite(url::Origin::Create(url));
    waiter.WaitForUserPermissionsSettingsChange();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

UserHostRestrictionsWithPermittedSitesBrowserTest::
    UserHostRestrictionsWithPermittedSitesBrowserTest() {
  feature_list_.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControlWithPermittedSites);
}

INSTANTIATE_TEST_SUITE_P(All,
                         UserHostRestrictionsWithPermittedSitesBrowserTest,
                         testing::Bool());

// Tests that extensions with withheld host permissions are automatically
// allowed to run on sites the user allows all extensions to run on.
IN_PROC_BROWSER_TEST_P(UserHostRestrictionsWithPermittedSitesBrowserTest,
                       UserPermittedSites) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 3,
           "content_scripts": [{
             "matches": ["http://allowed.example/*",
                         "http://restricted.example/*"],
             "js": ["content_script.js"],
             "run_at": "document_end"
           }],
           "permissions": ["storage"]
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
  const GURL unrequested_url =
      embedded_test_server()->GetURL("unrequested.example", "/title3.html");

  WithholdExtensionPermissions(*extension);

  const int kTabId = extension_misc::kUnknownTabId;

  // Check the initial state of (withheld) permissions - the extension should
  // have all requested host permissions withheld, and be denied on sites it
  // didn't request.
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            extension->permissions_data()->GetContentScriptAccess(
                allowed_url, kTabId, nullptr));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            extension->permissions_data()->GetContentScriptAccess(
                restricted_url, kTabId, nullptr));
  EXPECT_EQ(PermissionsData::PageAccess::kDenied,
            extension->permissions_data()->GetContentScriptAccess(
                unrequested_url, kTabId, nullptr));
  // And sanity check API permissions.
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      mojom::APIPermissionID::kStorage));

  // Next, simulate the user granting all extensions access to `allowed_url` and
  // `unrequested_url`.
  AddUserPermittedSite(allowed_url);
  AddUserPermittedSite(unrequested_url);

  // Now, the extension should be allowed to run on the `allowed_url`, but
  // `restricted_url` should remain withheld.
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            extension->permissions_data()->GetContentScriptAccess(
                allowed_url, kTabId, nullptr));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            extension->permissions_data()->GetContentScriptAccess(
                restricted_url, kTabId, nullptr));
  // Even though `unrequested_url` is a user-permitted site, the extension is
  // denied access because it didn't request permission.
  EXPECT_EQ(PermissionsData::PageAccess::kDenied,
            extension->permissions_data()->GetContentScriptAccess(
                unrequested_url, kTabId, nullptr));
  // Sanity check API permissions are unaffected.
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      mojom::APIPermissionID::kStorage));

  // Verify permissions access in the renderer. `allowed_url`'s title should be
  // changed, while `restricted_url` and `unrequested_url` should remain at
  // their original (awesome) titles.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), allowed_url));
  static constexpr char16_t kInjectedTitle[] = u"Injected";
  EXPECT_EQ(kInjectedTitle, GetActiveTab()->GetTitle());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), restricted_url));
  EXPECT_EQ(u"Title Of Awesomeness", GetActiveTab()->GetTitle());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), unrequested_url));
  EXPECT_EQ(u"Title Of More Awesomeness", GetActiveTab()->GetTitle());

  // Finally, remove the user-permitted `allowed_url`. Since the extension
  // only had access to this URL via it being a user-permitted URL (and not
  // via an explicit grant), the extension should lose access to the URL.
  {
    PermissionsManager* permissions_manager =
        PermissionsManager::Get(profile());
    PermissionsManagerWaiter waiter(permissions_manager);
    permissions_manager->RemoveUserPermittedSite(
        url::Origin::Create(allowed_url));
    waiter.WaitForUserPermissionsSettingsChange();
  }

  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            extension->permissions_data()->GetContentScriptAccess(
                allowed_url, kTabId, nullptr));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), allowed_url));
  // Note that title1.html has no title, so it defaults to the URL - but it's
  // sanitized for display (e.g. stripping HTTPS) so to avoid tying this too
  // closely with the UI, we just check that it's not equal to the injected
  // title.
  EXPECT_NE(kInjectedTitle, GetActiveTab()->GetTitle());

  // TODO(crbug.com/40803363): We could add more checks here to
  // exercise the network service path, as we do for user restricted sites
  // above. Since the user-permitted sites just grants the permissions to the
  // extension, we don't *really* need to, but additional coverage never hurt
  // (in case the implementation changes).
}

// Tests that user permitted sites are persisted and granted on extension load.
IN_PROC_BROWSER_TEST_P(UserHostRestrictionsWithPermittedSitesBrowserTest,
                       PRE_UserPermittedSitesArePersisted) {
  // Note: We need a "real" extension here (instead of just a TestExtensionDir)
  // because it needs to persist for the next test.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_all_urls"));
  ASSERT_TRUE(extension);

  WithholdExtensionPermissions(*extension);

  // Note: We don't use `embedded_test_server` to grab a URL here because the
  // port would (potentially) change between the PRE_ test and the second test.
  // Instead, just use a constructed URL. Since all we check is the permissions
  // data, we don't need the URL to actually load in the browsertest.
  const GURL allowed_url("https://example.com");

  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            extension->permissions_data()->GetPageAccess(
                allowed_url, extension_misc::kUnknownTabId, nullptr));

  AddUserPermittedSite(allowed_url);
  // Technically, this should only happen if the feature is enabled. However,
  // we only add user-permitted sites when the feature is enabled. We can't
  // DCHECK that (because then the version of these tests without the feature
  // don't work), so we somewhat awkwardly just allow it to take effect
  // (knowing that it shouldn't happen outside of tests).
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            extension->permissions_data()->GetPageAccess(
                allowed_url, extension_misc::kUnknownTabId, nullptr));
}

IN_PROC_BROWSER_TEST_P(UserHostRestrictionsWithPermittedSitesBrowserTest,
                       UserPermittedSitesArePersisted) {
  const Extension* found_extension = nullptr;
  for (const auto& extension :
       ExtensionRegistry::Get(profile())->enabled_extensions()) {
    if (extension->name() == "All Urls Extension") {
      found_extension = extension.get();
      break;
    }
  }
  ASSERT_TRUE(found_extension);

  const GURL example_com("https://example.com");
  // The user-permitted site should be allowed iff the
  // kExtensionsMenuAccessControl feature is enabled (unlike the test above, our
  // load-time granting *is* guarded behind the feature flag).
  if (GetParam()) {
    EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
              found_extension->permissions_data()->GetPageAccess(
                  example_com, extension_misc::kUnknownTabId, nullptr));
  } else {
    EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
              found_extension->permissions_data()->GetPageAccess(
                  example_com, extension_misc::kUnknownTabId, nullptr));
  }
}

// Tests that sites the user indicated all extensions may run on are still
// available to extensions after a permissions withholding change.
IN_PROC_BROWSER_TEST_P(UserHostRestrictionsWithPermittedSitesBrowserTest,
                       UserPermittedSitesAreAppliedOnWithholdingChange) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 3,
           "host_permissions": ["<all_urls>"]
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const GURL user_permitted_site("https://allowed.example");
  const GURL non_user_permitted_site("https://not-allowed.example");

  AddUserPermittedSite(user_permitted_site);

  // Without withholding permissions, the extension may run on both sites.
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            extension->permissions_data()->GetPageAccess(
                user_permitted_site, extension_misc::kUnknownTabId, nullptr));
  EXPECT_EQ(
      PermissionsData::PageAccess::kAllowed,
      extension->permissions_data()->GetPageAccess(
          non_user_permitted_site, extension_misc::kUnknownTabId, nullptr));

  WithholdExtensionPermissions(*extension);

  // Once permissions are withheld, with the kExtensionsMenuAccessControl
  // feature enabled, the extension may still run on the user-permitted site
  // (without the feature enabled, the site is withheld).
  if (GetParam()) {
    EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
              extension->permissions_data()->GetPageAccess(
                  user_permitted_site, extension_misc::kUnknownTabId, nullptr));
  } else {
    EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
              extension->permissions_data()->GetPageAccess(
                  user_permitted_site, extension_misc::kUnknownTabId, nullptr));
  }

  // Non-permitted sites remain withheld with and without the feature enabled.
  EXPECT_EQ(
      PermissionsData::PageAccess::kWithheld,
      extension->permissions_data()->GetPageAccess(
          non_user_permitted_site, extension_misc::kUnknownTabId, nullptr));
}

IN_PROC_BROWSER_TEST_P(UserHostRestrictionsWithPermittedSitesBrowserTest,
                       UserPermittedSitesAndChromeFavicon) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Note: MV2 extension because chrome://favicon is removed in MV3 (yay!).
  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 2,
           "permissions": ["<all_urls>"]
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const GURL favicon_url("chrome://favicon/http://example.com");
  EXPECT_TRUE(extension->permissions_data()->HasHostPermission(favicon_url));

  WithholdExtensionPermissions(*extension);
  EXPECT_TRUE(extension->permissions_data()->HasHostPermission(favicon_url));

  AddUserPermittedSite(GURL("https://allowed.example"));
  EXPECT_TRUE(extension->permissions_data()->HasHostPermission(favicon_url));
}

}  // namespace extensions

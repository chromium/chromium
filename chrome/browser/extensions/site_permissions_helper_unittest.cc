// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/site_permissions_helper.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/permissions_manager_waiter.h"

namespace extensions {

namespace {

base::Value::List ToValueList(const std::vector<std::string>& permissions) {
  base::Value::List list;
  for (const std::string& permission : permissions)
    list.Append(permission);
  return list;
}

}  // namespace

using UserSiteAccess = PermissionsManager::UserSiteAccess;
using SiteInteraction = SitePermissionsHelper::SiteInteraction;

class SitePermissionsHelperUnitTest : public ExtensionServiceTestWithInstall {
 public:
  scoped_refptr<const extensions::Extension> InstallExtension(
      const std::string& name);

  scoped_refptr<const extensions::Extension> InstallExtensionWithPermissions(
      const std::string& name,
      const std::vector<std::string>& host_permissions,
      const std::vector<std::string>& permissions = {});

  // Adds a new tab with `url` to the tab strip, and returns the WebContents
  // associated with it.
  content::WebContents* AddTab(const GURL& url);

  Browser* browser();

  SitePermissionsHelper* permissions_helper() {
    return permissions_helper_.get();
  }
  PermissionsManager* permissions_manager() { return permissions_manager_; }

  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

 private:
  // The browser and accompaying window.
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestBrowserWindow> browser_window_;

  // Site permissions helper being tested.
  std::unique_ptr<SitePermissionsHelper> permissions_helper_;

  raw_ptr<PermissionsManager> permissions_manager_;
};

scoped_refptr<const extensions::Extension>
SitePermissionsHelperUnitTest::InstallExtension(const std::string& name) {
  return InstallExtensionWithPermissions(name, {}, {});
}

scoped_refptr<const extensions::Extension>
SitePermissionsHelperUnitTest::InstallExtensionWithPermissions(
    const std::string& name,
    const std::vector<std::string>& host_permissions,
    const std::vector<std::string>& permissions) {
  auto extension =
      extensions::ExtensionBuilder(name)
          .SetManifestVersion(3)
          .SetManifestKey("host_permissions", ToValueList(host_permissions))
          .AddPermissions(permissions)
          .SetID(crx_file::id_util::GenerateId(name))
          .Build();
  service()->AddExtension(extension.get());

  return extension;
}

content::WebContents* SitePermissionsHelperUnitTest::AddTab(const GURL& url) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  content::WebContents* raw_contents = web_contents.get();

  browser()->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                  true);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(), raw_contents);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(raw_contents, url);
  EXPECT_EQ(url, raw_contents->GetLastCommittedURL());

  return raw_contents;
}

Browser* SitePermissionsHelperUnitTest::browser() {
  if (!browser_) {
    Browser::CreateParams params(profile(), true);
    browser_window_ = std::make_unique<TestBrowserWindow>();
    params.window = browser_window_.get();
    browser_.reset(Browser::Create(params));
  }
  return browser_.get();
}

void SitePermissionsHelperUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  permissions_helper_ = std::make_unique<SitePermissionsHelper>(profile());
  permissions_manager_ = PermissionsManager::Get(profile());
}

void SitePermissionsHelperUnitTest::TearDown() {
  // Remove any tabs in the tab strip; else the test crashes.
  if (browser_) {
    while (!browser_->tab_strip_model()->empty())
      browser_->tab_strip_model()->DetachAndDeleteWebContentsAt(0);
  }

  ExtensionServiceTestBase::TearDown();
}

// TODO(crbug.com/1289441): Move test that verify SiteAccess and SiteInteraction
// behavior after moving both enums to PermissionsManager.
TEST_F(SitePermissionsHelperUnitTest, SiteAccessAndInteraction_AllUrls) {
  auto extension =
      InstallExtensionWithPermissions("AllUrls Extension", {"<all_urls>"});

  {
    // Verify a non-restricted url has "on all sites" site access and "granted"
    // site interaction when the extension has all urls permission.
    const GURL non_restricted_url("http://www.non-restricted-url.com");
    auto* web_contents = AddTab(non_restricted_url);
    EXPECT_EQ(permissions_manager()->GetUserSiteAccess(*extension,
                                                       non_restricted_url),
              UserSiteAccess::kOnAllSites);
    EXPECT_EQ(
        permissions_helper()->GetSiteInteraction(*extension, web_contents),
        SiteInteraction::kGranted);
  }

  {
    // Verify a restricted url has "none" site interaction even when the
    // extension has all urls permission
    const GURL restricted_url("chrome://extensions");
    auto* web_contents = AddTab(restricted_url);
    EXPECT_EQ(
        permissions_helper()->GetSiteInteraction(*extension, web_contents),
        SiteInteraction::kNone);
  }
}

TEST_F(SitePermissionsHelperUnitTest, SiteAccessAndInteraction_RequestedUrl) {
  const GURL requested_url("http://www.requested.com");
  auto extension = InstallExtensionWithPermissions("Requested Extension",
                                                   {requested_url.spec()});

  {
    // Verify a non-restricted url has "on site" site access and "granted" site
    // interaction by default when the extension requests it.
    auto* web_contents = AddTab(requested_url);
    EXPECT_EQ(
        permissions_manager()->GetUserSiteAccess(*extension, requested_url),
        UserSiteAccess::kOnSite);
    EXPECT_EQ(
        permissions_helper()->GetSiteInteraction(*extension, web_contents),
        SiteInteraction::kGranted);
  }

  {
    // Verify a non-restricted url has "none" site interaction when the
    // extension does not request it.
    const GURL non_requested_url("http://www.non-requested.com");
    auto* web_contents = AddTab(non_requested_url);
    EXPECT_EQ(
        permissions_helper()->GetSiteInteraction(*extension, web_contents),
        SiteInteraction::kNone);
  }
}

TEST_F(SitePermissionsHelperUnitTest, SiteAccessAndInteraction_ActiveTab) {
  auto extension = InstallExtensionWithPermissions(
      "ActiveTab Extension",
      /*host_permissions=*/{}, /*permissions=*/{"activeTab"});

  {
    // Verify a non-restricted url has "on click" site access and "active tab"
    // site interaction when the extension only has active tab permission.
    const GURL non_restricted_url("http://www.non-restricted.com");
    auto* web_contents = AddTab(non_restricted_url);
    EXPECT_EQ(permissions_manager()->GetUserSiteAccess(*extension,
                                                       non_restricted_url),
              UserSiteAccess::kOnClick);
    EXPECT_EQ(
        permissions_helper()->GetSiteInteraction(*extension, web_contents),
        SiteInteraction::kActiveTab);
  }

  {
    // Verify a restricted url has "none" site interaction even if the extension
    // has active tab permission.
    const GURL restricted_url("chrome://extensions");
    auto* web_contents = AddTab(restricted_url);
    EXPECT_EQ(
        permissions_helper()->GetSiteInteraction(*extension, web_contents),
        SiteInteraction::kNone);
  }
}

TEST_F(SitePermissionsHelperUnitTest,
       SiteAccessAndInteraction_ActiveTabAndRequestedUrl) {
  const GURL requested_url("http://www.requested.com");
  auto extension = InstallExtensionWithPermissions(
      "ActiveTab Extension",
      /*host_permissions=*/{requested_url.spec()},
      /*permissions=*/{"activeTab"});

  {
    // Verify a url has "on click" site access and  "active tab"
    // site interaction when the extension does not request it but has active
    // tab permission.
    const GURL non_requested_url("http://www.non-requested.com");
    auto* web_contents = AddTab(non_requested_url);
    EXPECT_EQ(
        permissions_manager()->GetUserSiteAccess(*extension, non_requested_url),
        UserSiteAccess::kOnClick);
    EXPECT_EQ(
        permissions_helper()->GetSiteInteraction(*extension, web_contents),
        SiteInteraction::kActiveTab);
  }

  {
    // Verify a url has "on site" site access and "granted" site
    // interaction when the extension requests it and has access (default
    // behavior). "granted" takes priority over "activeTab" since the extension
    // has access to the site.
    auto* web_contents = AddTab(requested_url);
    EXPECT_EQ(
        permissions_manager()->GetUserSiteAccess(*extension, requested_url),
        UserSiteAccess::kOnSite);
    EXPECT_EQ(
        permissions_helper()->GetSiteInteraction(*extension, web_contents),
        SiteInteraction::kGranted);
  }

  ScriptingPermissionsModifier(profile(), extension.get())
      .RemoveAllGrantedHostPermissions();

  {
    // Verify a url has "on site" site access and "granted" site
    // interaction when the extension requests it and its access is withheld.
    // "withheld" takes priority over "activeTab" since the extension is
    // explicitly requesting access to the site.
    auto* web_contents = AddTab(requested_url);
    EXPECT_EQ(
        permissions_manager()->GetUserSiteAccess(*extension, requested_url),
        UserSiteAccess::kOnClick);
    EXPECT_EQ(
        permissions_helper()->GetSiteInteraction(*extension, web_contents),
        SiteInteraction::kWithheld);
  }
}

TEST_F(SitePermissionsHelperUnitTest,
       SiteAccessAndInteraction_NoHostPermissions) {
  auto extension = InstallExtension("Requested Extension");

  // Verify any url has "none" site interaction when the extension has no host
  // permissions.
  const GURL url("http://www.example.com");
  auto* web_contents = AddTab(url);
  EXPECT_EQ(permissions_helper()->GetSiteInteraction(*extension, web_contents),
            SiteInteraction::kNone);
}

// Tests that updating permission only applies the permission to the upgraded
// site and not others.
TEST_F(SitePermissionsHelperUnitTest, UpdateSiteAccess_OnlySiteSelected) {
  const GURL site("https://allowed.example");
  auto extension = InstallExtensionWithPermissions(
      "extension", /*host_permissions=*/{site.spec()});
  const GURL site_without_permission("https://disallowed.com");
  auto* site_contents = AddTab(site);

  // Extension should only have on-click access to both sites.
  ASSERT_EQ(PermissionsManager::UserSiteAccess::kOnSite,
            permissions_manager()->GetUserSiteAccess(*extension, site));
  EXPECT_EQ(PermissionsManager::UserSiteAccess::kOnClick,
            permissions_manager()->GetUserSiteAccess(*extension,
                                                     site_without_permission));

  // Switch the extension from on-click to always on site.
  ExtensionActionRunner* action_runner =
      ExtensionActionRunner::GetForWebContents(site_contents);
  ASSERT_TRUE(action_runner);
  action_runner->accept_bubble_for_testing(false);
  permissions_helper()->UpdateSiteAccess(
      *extension, site_contents, PermissionsManager::UserSiteAccess::kOnClick);

  // Confirm always on site permission applied.
  EXPECT_EQ(PermissionsManager::UserSiteAccess::kOnClick,
            permissions_manager()->GetUserSiteAccess(*extension, site));

  // Site without permission should remain without access.
  EXPECT_EQ(PermissionsManager::UserSiteAccess::kOnClick,
            permissions_manager()->GetUserSiteAccess(*extension,
                                                     site_without_permission));
}

class SitePermissionsHelperWithUserHostControlsUnitTest
    : public SitePermissionsHelperUnitTest {
 public:
  SitePermissionsHelperWithUserHostControlsUnitTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        extensions_features::kExtensionsMenuAccessControl,
        extensions_features::kExtensionsMenuAccessControlWithPermittedSites};
    std::vector<base::test::FeatureRef> disabled_features;
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  ~SitePermissionsHelperWithUserHostControlsUnitTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that setting an extension to on-click retains its access to
// sites the user explicitly marked as ones that all extensions may run on.
TEST_F(SitePermissionsHelperWithUserHostControlsUnitTest,
       DowngradingFromAllSitesToOnClickAppliesUserPermittedSites) {
  auto extension = InstallExtensionWithPermissions(
      "extension", /*host_permissions=*/{"<all_urls>"}, /*permissions=*/{});

  const GURL user_permitted_site("https://allowed.example");
  const GURL non_user_permitted_site("https://not-allowed.example");

  {
    // Add a user-permitted site.
    PermissionsManagerWaiter waiter(permissions_manager());
    permissions_manager()->AddUserPermittedSite(
        url::Origin::Create(user_permitted_site));
    waiter.WaitForUserPermissionsSettingsChange();
  }

  auto* user_permitted_contents = AddTab(user_permitted_site);
  auto* non_user_permitted_contents = AddTab(non_user_permitted_site);

  // Right now, the extension should be allowed to run everywhere (on both
  // `user_permitted_site` and `non_user_permitted_site`).
  EXPECT_EQ(UserSiteAccess::kOnAllSites,
            permissions_manager()->GetUserSiteAccess(*extension,
                                                     user_permitted_site));
  EXPECT_EQ(SitePermissionsHelper::SiteInteraction::kGranted,
            permissions_helper()->GetSiteInteraction(*extension,
                                                     user_permitted_contents));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            extension->permissions_data()->GetPageAccess(
                user_permitted_site, extension_misc::kUnknownTabId, nullptr));
  EXPECT_EQ(UserSiteAccess::kOnAllSites,
            permissions_manager()->GetUserSiteAccess(*extension,
                                                     non_user_permitted_site));
  EXPECT_EQ(SitePermissionsHelper::SiteInteraction::kGranted,
            permissions_helper()->GetSiteInteraction(
                *extension, non_user_permitted_contents));
  EXPECT_EQ(
      PermissionsData::PageAccess::kAllowed,
      extension->permissions_data()->GetPageAccess(
          non_user_permitted_site, extension_misc::kUnknownTabId, nullptr));

  {
    // Switch the extension from on all sites to on-click.
    ExtensionActionRunner* action_runner =
        ExtensionActionRunner::GetForWebContents(non_user_permitted_contents);
    ASSERT_TRUE(action_runner);
    // Permissions for the site are still updated even if the tab is not
    // reloaded.
    action_runner->accept_bubble_for_testing(false);
    PermissionsManagerWaiter waiter(permissions_manager());
    permissions_helper()->UpdateSiteAccess(
        *extension, non_user_permitted_contents, UserSiteAccess::kOnClick);
    waiter.WaitForExtensionPermissionsUpdate();
  }

  // The extension should now be able to run on `user_permitted` site
  // automatically, since it's a user-permitted site.

  // TODO(https://crbug.com/1268198): The following check should be in place:
  // EXPECT_EQ(UserSiteAccess::kOnSite,
  //           permissions_manager->GetUserSiteAccess(
  //               *extension, user_permitted_site));
  // However, currently PermissionsManager::GetSiteAccess() (which is used by
  // SitePermissionsHelper::GetSiteAccess()) doesn't take user-permitted sites
  // into account.
  EXPECT_EQ(UserSiteAccess::kOnClick, permissions_manager()->GetUserSiteAccess(
                                          *extension, user_permitted_site));
  EXPECT_EQ(SitePermissionsHelper::SiteInteraction::kGranted,
            permissions_helper()->GetSiteInteraction(*extension,
                                                     user_permitted_contents));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            extension->permissions_data()->GetPageAccess(
                user_permitted_site, extension_misc::kUnknownTabId, nullptr));

  // Non-user-permitted sites should remain withheld.
  EXPECT_EQ(UserSiteAccess::kOnClick, permissions_manager()->GetUserSiteAccess(
                                          *extension, non_user_permitted_site));
  EXPECT_EQ(SitePermissionsHelper::SiteInteraction::kWithheld,
            permissions_helper()->GetSiteInteraction(
                *extension, non_user_permitted_contents));
  EXPECT_EQ(
      PermissionsData::PageAccess::kWithheld,
      extension->permissions_data()->GetPageAccess(
          non_user_permitted_site, extension_misc::kUnknownTabId, nullptr));
}

}  // namespace extensions

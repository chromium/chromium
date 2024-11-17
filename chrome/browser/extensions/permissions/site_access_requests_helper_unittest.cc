// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/site_access_requests_helper.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/permissions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/url_pattern.h"
#include "extensions/test/permissions_manager_waiter.h"

namespace extensions {

class SiteAccessRequestsHelperUnittest : public ExtensionServiceTestBase {
 public:
  SiteAccessRequestsHelperUnittest() = default;
  ~SiteAccessRequestsHelperUnittest() override = default;

  SiteAccessRequestsHelperUnittest(const SiteAccessRequestsHelperUnittest&) =
      delete;
  SiteAccessRequestsHelperUnittest& operator=(
      const SiteAccessRequestsHelperUnittest&) = delete;

  // Installs an extension with `host_permission` and withhelds them.
  scoped_refptr<const Extension> InstallExtensionAndWithholdHostPermissions(
      const std::string& name,
      const std::string& host_permission);

  // Installs an extension with activeTab permission.
  scoped_refptr<const Extension> InstallExtensionWithActiveTab(
      const std::string& name);

  // Adds a new tab with `url` to the tab strip, and returns the WebContents
  // associated with it.
  content::WebContents* AddTab(const GURL& url);

  // Returns the browser. Creates a new one if it doesn't exist.
  Browser* browser();

  PermissionsManager* permissions_manager() { return permissions_manager_; }

  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

 private:
  // The browser and accompaying window.
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestBrowserWindow> browser_window_;

  raw_ptr<PermissionsManager> permissions_manager_;
};

scoped_refptr<const Extension>
SiteAccessRequestsHelperUnittest::InstallExtensionAndWithholdHostPermissions(
    const std::string& name,
    const std::string& host_permission) {
  auto extension = ExtensionBuilder(name)
                       .SetManifestVersion(3)
                       .AddHostPermission(host_permission)
                       .SetID(crx_file::id_util::GenerateId(name))
                       .Build();
  service()->AddExtension(extension.get());

  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  return extension;
}

scoped_refptr<const Extension>
SiteAccessRequestsHelperUnittest::InstallExtensionWithActiveTab(
    const std::string& name) {
  auto extension = ExtensionBuilder(name)
                       .SetManifestVersion(3)
                       .SetID(crx_file::id_util::GenerateId(name))
                       .AddAPIPermission("activeTab")
                       .Build();
  service()->AddExtension(extension.get());

  return extension;
}

content::WebContents* SiteAccessRequestsHelperUnittest::AddTab(
    const GURL& url) {
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

Browser* SiteAccessRequestsHelperUnittest::browser() {
  if (!browser_) {
    Browser::CreateParams params(profile(), true);
    browser_window_ = std::make_unique<TestBrowserWindow>();
    params.window = browser_window_.get();
    browser_.reset(Browser::Create(params));
  }
  return browser_.get();
}

void SiteAccessRequestsHelperUnittest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  permissions_manager_ = PermissionsManager::Get(profile());
}

void SiteAccessRequestsHelperUnittest::TearDown() {
  // Remove any tabs in the tab strip; else the test crashes.
  if (browser_) {
    while (!browser_->tab_strip_model()->empty()) {
      browser_->tab_strip_model()->DetachAndDeleteWebContentsAt(0);
    }
  }
  permissions_manager_ = nullptr;

  ExtensionServiceTestBase::TearDown();
}

// Tests site access requests are properly added and removed.
TEST_F(SiteAccessRequestsHelperUnittest, AddAndRemoveRequests) {
  auto extension_A =
      InstallExtensionAndWithholdHostPermissions("Extension A", "<all_urls>");
  auto extension_B =
      InstallExtensionAndWithholdHostPermissions("Extension B", "<all_urls>");

  content::WebContents* web_contents = AddTab(GURL("http://www.example.com/"));
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Try to remove a non-existent site access request. Verify nothing happens.
  EXPECT_FALSE(permissions_manager()->RemoveSiteAccessRequest(
      tab_id, extension_A->id()));
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension_A->id()));
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension_B->id()));

  // Add site access request for extension A. Verify only extension A has an
  // active request.
  permissions_manager()->AddSiteAccessRequest(web_contents, tab_id,
                                              *extension_A);
  EXPECT_TRUE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension_A->id()));
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension_B->id()));

  // Add site access request for extension B. Verify both extensions have active
  // requests.
  permissions_manager()->AddSiteAccessRequest(web_contents, tab_id,
                                              *extension_B);
  EXPECT_TRUE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension_A->id()));
  EXPECT_TRUE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension_B->id()));

  // Remove site access request for extension A. Verify only extension B has an
  // active request.
  EXPECT_TRUE(permissions_manager()->RemoveSiteAccessRequest(
      tab_id, extension_A->id()));
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension_A->id()));
  EXPECT_TRUE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension_B->id()));
}

// Tests that site access requests which include a filter are properly added
// and removed.
TEST_F(SiteAccessRequestsHelperUnittest,
       AddAndRemoveRequestsWithPatternFilter) {
  auto extension =
      InstallExtensionAndWithholdHostPermissions("Extension", "<all_urls>");
  URLPattern matching_filter(Extension::kValidHostPermissionSchemes,
                             "http://www.matching.com/");
  URLPattern non_matching_filter(Extension::kValidHostPermissionSchemes,
                                 "http://www.non-matching.com/");

  content::WebContents* web_contents = AddTab(GURL("http://www.matching.com/"));
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Add a site access request with filter that does not match the current web
  // contents. Verify request is not active.
  permissions_manager()->AddSiteAccessRequest(web_contents, tab_id, *extension,
                                              non_matching_filter);
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));

  // Add a site access request with filter that matches the current web
  // contents. Verify request is active.
  permissions_manager()->AddSiteAccessRequest(web_contents, tab_id, *extension,
                                              matching_filter);
  EXPECT_TRUE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));

  // Remove a site access request with filter that doesn't match the current web
  // contents. Verify request is active.
  permissions_manager()->RemoveSiteAccessRequest(tab_id, extension->id(),
                                                 non_matching_filter);
  EXPECT_TRUE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));

  // Remove a site access request with filter that matches the current web
  // contents. Verify request is not active.
  permissions_manager()->RemoveSiteAccessRequest(tab_id, extension->id(),
                                                 matching_filter);
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));

  // Add back a site access request with filter that matches the current web
  // contents (so we can test removal without filter). Verify request is active.
  permissions_manager()->AddSiteAccessRequest(web_contents, tab_id, *extension,
                                              matching_filter);
  EXPECT_TRUE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));

  // Remove a site access request for extension without specifying a filter.
  // Verify request is no longer active, since a request without filter matches
  // all patterns.
  EXPECT_TRUE(
      permissions_manager()->RemoveSiteAccessRequest(tab_id, extension->id()));
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));
}

// Test request is not added when extension only has activeTab permission.
TEST_F(SiteAccessRequestsHelperUnittest,
       InvalidRequest_ExtensionOnlyHasActiveTabPermission) {
  auto extension = InstallExtensionWithActiveTab("Extension");

  content::WebContents* web_contents = AddTab(GURL("http://www.example.com"));
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Add site access request for extension. Verify request is not active.
  permissions_manager()->AddSiteAccessRequest(web_contents, tab_id, *extension);
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));
}

// Tests that site access requests dismissed by the user are not active
// requests.
TEST_F(SiteAccessRequestsHelperUnittest, UserDismissedRequest) {
  auto extension =
      InstallExtensionAndWithholdHostPermissions("Extension", "<all_urls>");

  content::WebContents* web_contents = AddTab(GURL("http://www.example.com/"));
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Add site access request for extension A. Verify extension has an active
  // request.
  permissions_manager()->AddSiteAccessRequest(web_contents, tab_id, *extension);
  EXPECT_TRUE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));

  // Dismiss site access request for extension. Verify request is not an active
  // request.
  permissions_manager()->UserDismissedSiteAccessRequest(web_contents, tab_id,
                                                        extension->id());
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));
}

// Tests request is removed on cross-origin navigations.
TEST_F(SiteAccessRequestsHelperUnittest,
       RequestRemovedOnCrossOriginNavigation) {
  auto extension =
      InstallExtensionAndWithholdHostPermissions("Extension", "<all_urls>");

  content::WebContents* web_contents =
      AddTab(GURL("http://www.same-origin.com/a"));
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Add site access request for extension.
  permissions_manager()->AddSiteAccessRequest(web_contents, tab_id, *extension);
  EXPECT_TRUE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));

  // Same-origin navigation should retain request.
  web_contents_tester->NavigateAndCommit(GURL("http://www.same-origin.com/b"));
  EXPECT_TRUE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));

  // Cross-origin navigation should remove request.
  web_contents_tester->NavigateAndCommit(GURL("http://www.cross-origin.com/c"));
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));
}

// Tests that a request with matching filter pattern are active on same origin
// navigations.
TEST_F(SiteAccessRequestsHelperUnittest, RequestUpdatedOnPageNavigations) {
  auto extension =
      InstallExtensionAndWithholdHostPermissions("Extension", "<all_urls>");

  content::WebContents* web_contents = AddTab(GURL("http://www.example.com/"));
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Add site access request for extension. Verify request is active.
  std::optional<URLPattern> filter;
  permissions_manager()->AddSiteAccessRequest(web_contents, tab_id, *extension,
                                              filter);
  EXPECT_TRUE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));

  // Add site access request for extension with a filter that doesn't match the
  // current web contents but has the same origin. Verify request is not
  // active, since new request should override the previous one.
  filter = URLPattern(Extension::kValidHostPermissionSchemes, "*://*/path");
  permissions_manager()->AddSiteAccessRequest(web_contents, tab_id, *extension,
                                              filter);
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));

  // Navigate to a site that matches the filter. Verify request is active.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents, GURL("http://www.example.com/path"));
  EXPECT_TRUE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));

  // Add site access request for extension with a filter that doesn't have the
  // same origin as the current web contents. Verify request is not active.
  filter = URLPattern(Extension::kValidHostPermissionSchemes,
                      "http://www.other.com/path");
  permissions_manager()->AddSiteAccessRequest(web_contents, tab_id, *extension,
                                              filter);
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));
}

// Test request is removed when extension is granted "always on this site" site
// access.
TEST_F(SiteAccessRequestsHelperUnittest,
       RequestRemovedWhenExtensionHasSiteAccess) {
  auto extension =
      InstallExtensionAndWithholdHostPermissions("Extension", "<all_urls>");

  content::WebContents* web_contents =
      AddTab(GURL("http://www.same-origin.com/a"));
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Add site access request for extension.
  permissions_manager()->AddSiteAccessRequest(web_contents, tab_id, *extension);
  EXPECT_TRUE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));

  // Grant "always on this site" access to the extension.
  PermissionsManagerWaiter waiter(PermissionsManager::Get(profile()));
  SitePermissionsHelper permissions(profile());
  permissions.UpdateSiteAccess(*extension, web_contents,
                               PermissionsManager::UserSiteAccess::kOnSite);
  waiter.WaitForExtensionPermissionsUpdate();

  // Request should be removed since extension has granted site access.
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));
}

// Test request is removed when extension is granted one-time site access.
TEST_F(SiteAccessRequestsHelperUnittest,
       RequestRemovedWhenExtensionHasGrantedActiveTab) {
  // Add extension with host permissions and activeTab, and withhold the host
  // permissions.
  const std::string extension_name = "Extension";
  auto extension = ExtensionBuilder(extension_name)
                       .SetManifestVersion(3)
                       .AddHostPermission("http://www.example.com/")
                       .AddAPIPermission("activeTab")
                       .SetID(crx_file::id_util::GenerateId(extension_name))
                       .Build();
  service()->AddExtension(extension.get());
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  content::WebContents* web_contents = AddTab(GURL("http://www.example.com"));
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Add site access request for extension.
  permissions_manager()->AddSiteAccessRequest(web_contents, tab_id, *extension);
  EXPECT_TRUE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));

  // Grant tab permission to extension.
  ActiveTabPermissionGranter* active_tab_permission_granter =
      TabHelper::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents())
          ->active_tab_permission_granter();
  ASSERT_TRUE(active_tab_permission_granter);
  active_tab_permission_granter->GrantIfRequested(extension.get());

  // Request should be removed since extension has granted site access.
  // Even though the extension was only granted activeTab (and not persistent
  // site access, as would be the case if the user accepted the request), we no
  // longer show the site access request. This is to avoid a user seeing a
  // request after having granted one-time access to an extension.
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension->id()));
}

// Test request is removed when extension is unloaded.
TEST_F(SiteAccessRequestsHelperUnittest,
       RequestRemovedWhenExtensionIsUnloaded) {
  auto extension_A =
      InstallExtensionAndWithholdHostPermissions("Extension A", "<all_urls>");
  auto extension_B =
      InstallExtensionAndWithholdHostPermissions("Extension B", "<all_urls>");

  content::WebContents* web_contents =
      AddTab(GURL("http://www.same-origin.com/a"));
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Add site access request for both extensions.
  permissions_manager()->AddSiteAccessRequest(web_contents, tab_id,
                                              *extension_A);
  permissions_manager()->AddSiteAccessRequest(web_contents, tab_id,
                                              *extension_B);
  ASSERT_TRUE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension_A->id()));
  ASSERT_TRUE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension_B->id()));

  // Uninstall extension A. Verify only extension B should have a site access
  // request.
  service()->UninstallExtension(
      extension_A->id(), extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension_A->id()));
  EXPECT_TRUE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension_B->id()));

  // Disable extension B. Verify no extension should have a site access request.
  service()->DisableExtension(extension_B->id(),
                              extensions::disable_reason::DISABLE_USER_ACTION);
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension_A->id()));
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension_B->id()));

  // Enable extension B. Verify no extension has a site access request. Request
  // is not persisted when extension is re-enabled, the extension needs to add
  // the request again.
  service()->EnableExtension(extension_B->id());
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension_A->id()));
  EXPECT_FALSE(permissions_manager()->HasActiveSiteAccessRequest(
      tab_id, extension_B->id()));
}

}  // namespace extensions

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/site_permissions_helper.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

namespace {

std::unique_ptr<base::ListValue> ToListValue(
    const std::vector<std::string>& permissions) {
  extensions::ListBuilder builder;
  for (const std::string& permission : permissions)
    builder.Append(permission);
  return builder.Build();
}

}  // namespace

using SiteAccess = SitePermissionsHelper::SiteAccess;
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

  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

 private:
  // The browser and accompaying window.
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestBrowserWindow> browser_window_;

  // Site permissions helper being tested.
  std::unique_ptr<SitePermissionsHelper> permissions_helper_;
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
          .SetManifestKey("host_permissions", ToListValue(host_permissions))
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
}

void SitePermissionsHelperUnitTest::TearDown() {
  // Remove any tabs in the tab strip; else the test crashes.
  if (browser_) {
    while (!browser_->tab_strip_model()->empty())
      browser_->tab_strip_model()->DetachAndDeleteWebContentsAt(0);
  }

  ExtensionServiceTestBase::TearDown();
}

TEST_F(SitePermissionsHelperUnitTest, SiteAccessAndInteraction_AllUrls) {
  auto extension =
      InstallExtensionWithPermissions("AllUrls Extension", {"<all_urls>"});

  {
    // Verify a non-restricted url has "on all sites" site access and "active"
    // site interaction when the extension has all urls permission.
    const GURL non_restricted_url("http://www.non-restricted-url.com");
    auto* web_contents = AddTab(non_restricted_url);
    EXPECT_EQ(
        permissions_helper()->GetSiteAccess(*extension, non_restricted_url),
        SiteAccess::kOnAllSites);
    EXPECT_EQ(
        permissions_helper()->GetSiteInteraction(*extension, web_contents),
        SiteInteraction::kActive);
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
    // Verify a non-restricted url has "on site" site access and "active" site
    // interaction by default when the extension requests it.
    auto* web_contents = AddTab(requested_url);
    EXPECT_EQ(permissions_helper()->GetSiteAccess(*extension, requested_url),
              SiteAccess::kOnSite);
    EXPECT_EQ(
        permissions_helper()->GetSiteInteraction(*extension, web_contents),
        SiteInteraction::kActive);
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
    EXPECT_EQ(
        permissions_helper()->GetSiteAccess(*extension, non_restricted_url),
        SiteAccess::kOnClick);
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
    // Verify a non-restricted url has "on site" site access and "active" site
    // interaction by default when the extension requests it, regardless if the
    // extension also has active tab permission.
    auto* web_contents = AddTab(requested_url);
    EXPECT_EQ(permissions_helper()->GetSiteAccess(*extension, requested_url),
              SiteAccess::kOnSite);
    EXPECT_EQ(
        permissions_helper()->GetSiteInteraction(*extension, web_contents),
        SiteInteraction::kActive);
  }

  {
    // Verify a non-restricted url has "on click" site access and  "active tab"
    // site interaction when the extension does not request it but has active
    // tab permission.
    const GURL non_requested_url("http://www.non-requested.com");
    auto* web_contents = AddTab(non_requested_url);
    EXPECT_EQ(
        permissions_helper()->GetSiteAccess(*extension, non_requested_url),
        SiteAccess::kOnClick);
    EXPECT_EQ(
        permissions_helper()->GetSiteInteraction(*extension, web_contents),
        SiteInteraction::kActiveTab);
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

TEST_F(SitePermissionsHelperUnitTest, CanSelectSiteAccess_AllUrls) {
  auto extension =
      InstallExtensionWithPermissions("AllUrls Extension", {"<all_urls>"});

  // Verify "on click", "on site" and "on all sites" site access can be selected
  // for a non-restricted url.
  const GURL url("http://www.example.com");
  EXPECT_TRUE(permissions_helper()->CanSelectSiteAccess(*extension, url,
                                                        SiteAccess::kOnClick));
  EXPECT_TRUE(permissions_helper()->CanSelectSiteAccess(*extension, url,
                                                        SiteAccess::kOnSite));
  EXPECT_TRUE(permissions_helper()->CanSelectSiteAccess(
      *extension, url, SiteAccess::kOnAllSites));

  // Verify "on click", "on site" and "on all sites" cannot be selected for a
  // restricted url.
  const GURL chrome_url("chrome://settings");
  EXPECT_FALSE(permissions_helper()->CanSelectSiteAccess(*extension, chrome_url,
                                                         SiteAccess::kOnClick));
  EXPECT_FALSE(permissions_helper()->CanSelectSiteAccess(*extension, chrome_url,
                                                         SiteAccess::kOnSite));
  EXPECT_FALSE(permissions_helper()->CanSelectSiteAccess(
      *extension, chrome_url, SiteAccess::kOnAllSites));
}

TEST_F(SitePermissionsHelperUnitTest, CanSelectSiteAccess_SpecificUrl) {
  const GURL url_a("http://www.a.com");
  auto extension =
      InstallExtensionWithPermissions("A Extension", {url_a.spec()});

  // Verify "on click" and "on site" can be selected for the specific url, but
  // "on all sites" cannot be selected.
  EXPECT_TRUE(permissions_helper()->CanSelectSiteAccess(*extension, url_a,
                                                        SiteAccess::kOnClick));
  EXPECT_TRUE(permissions_helper()->CanSelectSiteAccess(*extension, url_a,
                                                        SiteAccess::kOnSite));
  EXPECT_FALSE(permissions_helper()->CanSelectSiteAccess(
      *extension, url_a, SiteAccess::kOnAllSites));

  // Verify "on click", "on site" and "on all sites" cannot be selected for any
  // other url.
  const GURL url_b("http://www.b.com");
  EXPECT_FALSE(permissions_helper()->CanSelectSiteAccess(*extension, url_b,
                                                         SiteAccess::kOnClick));
  EXPECT_FALSE(permissions_helper()->CanSelectSiteAccess(*extension, url_b,
                                                         SiteAccess::kOnSite));
  EXPECT_FALSE(permissions_helper()->CanSelectSiteAccess(
      *extension, url_b, SiteAccess::kOnAllSites));
}

TEST_F(SitePermissionsHelperUnitTest, CanSelectSiteAccess_NoHostPermissions) {
  auto extension = InstallExtension("Extension");

  // Verify "on click", "on site" and "on all sites" cannot be selected for any
  // url.
  const GURL url("http://www.example.com");
  EXPECT_FALSE(permissions_helper()->CanSelectSiteAccess(*extension, url,
                                                         SiteAccess::kOnClick));
  EXPECT_FALSE(permissions_helper()->CanSelectSiteAccess(*extension, url,
                                                         SiteAccess::kOnSite));
  EXPECT_FALSE(permissions_helper()->CanSelectSiteAccess(
      *extension, url, SiteAccess::kOnAllSites));
}

TEST_F(SitePermissionsHelperUnitTest, CanSelectSiteAccess_ActiveTab) {
  auto extension = InstallExtensionWithPermissions(
      "ActiveTab Extension",
      /*host_permissions=*/{}, /*permissions=*/{"activeTab"});

  // Verify "on click" can be selected for the specific url, but "on site" and
  // "on all sites" cannot be selected.
  const GURL url("http://www.example.com");
  EXPECT_TRUE(permissions_helper()->CanSelectSiteAccess(*extension, url,
                                                        SiteAccess::kOnClick));
  EXPECT_FALSE(permissions_helper()->CanSelectSiteAccess(*extension, url,
                                                         SiteAccess::kOnSite));
  EXPECT_FALSE(permissions_helper()->CanSelectSiteAccess(
      *extension, url, SiteAccess::kOnAllSites));
}

}  // namespace extensions

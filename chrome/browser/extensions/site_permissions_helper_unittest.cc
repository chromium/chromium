// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/site_permissions_helper.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "components/crx_file/id_util.h"
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

class SitePermissionsHelperUnitTest : public ExtensionServiceTestWithInstall {
 public:
  void SetUp() override;

  scoped_refptr<const extensions::Extension> InstallExtension(
      const std::string& name);

  scoped_refptr<const extensions::Extension> InstallExtensionWithPermissions(
      const std::string& name,
      const std::vector<std::string>& host_permissions,
      const std::vector<std::string>& permissions = {});

  SitePermissionsHelper* permissions_helper() {
    return permissions_helper_.get();
  }

 private:
  // Site permissions helper being tested.
  std::unique_ptr<SitePermissionsHelper> permissions_helper_;
};

void SitePermissionsHelperUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  permissions_helper_ = std::make_unique<SitePermissionsHelper>(profile());
}

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

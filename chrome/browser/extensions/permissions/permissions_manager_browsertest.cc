// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using PermissionsManagerBrowserTest = ExtensionBrowserTest;

IN_PROC_BROWSER_TEST_F(PermissionsManagerBrowserTest,
                       PRE_RestrictedSitesArePersisted) {
  auto* manager = PermissionsManager::Get(profile());

  // Verify the restricted sites list is empty.
  EXPECT_EQ(manager->GetUserPermissionsSettings().restricted_sites,
            std::set<url::Origin>());

  // Add a url to restricted sites. Verify the site is stored as a restricted
  // site.
  const url::Origin url =
      url::Origin::Create(GURL("http://restricted.example.com"));
  std::set<url::Origin> set_with_url;
  set_with_url.insert(url);
  manager->AddUserRestrictedSite(url);
  EXPECT_EQ(manager->GetUserPermissionsSettings().restricted_sites,
            set_with_url);
}

// Tests that user-level permissions are properly persisted across sessions.
IN_PROC_BROWSER_TEST_F(PermissionsManagerBrowserTest,
                       RestrictedSitesArePersisted) {
  auto* manager = PermissionsManager::Get(profile());

  // Verify the restricted site stored in previous session is persisted.
  std::set<url::Origin> set_with_url;
  set_with_url.insert(
      url::Origin::Create(GURL("http://restricted.example.com")));
  EXPECT_EQ(manager->GetUserPermissionsSettings().restricted_sites,
            set_with_url);
}

class PermissionsManagerWithPermittedSitesBrowserTest
    : public ExtensionBrowserTest {
 public:
  PermissionsManagerWithPermittedSitesBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsMenuAccessControlWithPermittedSites);
  }
  ~PermissionsManagerWithPermittedSitesBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PermissionsManagerWithPermittedSitesBrowserTest,
                       PRE_PermittedSitesArePersisted) {
  auto* manager = PermissionsManager::Get(profile());

  // Add a url to permitted sites. Verify the site is stored as a permitted
  // site.
  const url::Origin url =
      url::Origin::Create(GURL("http://permitted.example.com"));
  std::set<url::Origin> set_with_url;
  set_with_url.insert(url);
  manager->AddUserPermittedSite(url);
  EXPECT_EQ(manager->GetUserPermissionsSettings().permitted_sites,
            set_with_url);
}

// Tests that user-level permissions are properly persisted across sessions.
IN_PROC_BROWSER_TEST_F(PermissionsManagerWithPermittedSitesBrowserTest,
                       PermittedSitesArePersisted) {
  auto* manager = PermissionsManager::Get(profile());

  // Verify the permitted site stored in previous session is persisted.
  std::set<url::Origin> set_with_url;
  set_with_url.insert(
      url::Origin::Create(GURL("http://permitted.example.com")));
  EXPECT_EQ(manager->GetUserPermissionsSettings().permitted_sites,
            set_with_url);
}

}  // namespace extensions

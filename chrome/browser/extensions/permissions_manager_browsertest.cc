// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/state_store.h"

namespace extensions {

using PermissionsManagerBrowserTest = ExtensionBrowserTest;

IN_PROC_BROWSER_TEST_F(PermissionsManagerBrowserTest,
                       PRE_UserPermissionsArePersisted) {
  auto* manager = PermissionsManager::Get(profile());

  // Verify the restricted sites list is empty.
  EXPECT_EQ(manager->GetUserPermissionsSettings().restricted_sites,
            std::set<url::Origin>());

  {
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

  {
    // Add a different url to permitted sites. Verify the site is stored as a
    // permitted site.
    const url::Origin url =
        url::Origin::Create(GURL("http://permitted.example.com"));
    std::set<url::Origin> set_with_url;
    set_with_url.insert(url);
    manager->AddUserPermittedSite(url);
    EXPECT_EQ(manager->GetUserPermissionsSettings().permitted_sites,
              set_with_url);
  }
}

// Tests that user-level permissions are properly persisted across sessions.
IN_PROC_BROWSER_TEST_F(PermissionsManagerBrowserTest,
                       UserPermissionsArePersisted) {
  auto* manager = PermissionsManager::Get(profile());

  {
    // Verify the restricted site stored in previous session is persisted.
    std::set<url::Origin> set_with_url;
    set_with_url.insert(
        url::Origin::Create(GURL("http://restricted.example.com")));
    EXPECT_EQ(manager->GetUserPermissionsSettings().restricted_sites,
              set_with_url);
  }

  {
    // Verify the permitted site stored in previous session is persisted.
    std::set<url::Origin> set_with_url;
    set_with_url.insert(
        url::Origin::Create(GURL("http://permitted.example.com")));
    EXPECT_EQ(manager->GetUserPermissionsSettings().permitted_sites,
              set_with_url);
  }
}

}  // namespace extensions

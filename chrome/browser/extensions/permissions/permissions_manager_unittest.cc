// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/permissions_manager.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

using PermissionsManagerUnitTest = ExtensionServiceTestWithInstall;

TEST_F(PermissionsManagerUnitTest, FaviconPermissionsAreNotWithheld) {
  constexpr char kManifest[] =
      R"({
           "name": "<all urls> extension",
           "manifest_version": 2,
           "version": "0.1",
           "permissions": ["<all_urls>"]
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);

  InitializeEmptyExtensionService();
  const Extension* extension =
      PackAndInstallCRX(test_dir.UnpackedPath(), INSTALL_NEW);
  ASSERT_TRUE(extension);

  URLPattern chrome_favicon_pattern(Extension::kValidHostPermissionSchemes,
                                    "chrome://favicon/");
  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  .explicit_hosts()
                  .ContainsPattern(chrome_favicon_pattern));

  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);
  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  .explicit_hosts()
                  .ContainsPattern(chrome_favicon_pattern));

  {
    const ExtensionId id = extension->id();
    service()->ReloadExtensionsForTest();
    extension = registry()->enabled_extensions().GetByID(id);
    ASSERT_TRUE(extension);
  }
  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  .explicit_hosts()
                  .ContainsPattern(chrome_favicon_pattern));
}

}  // namespace extensions

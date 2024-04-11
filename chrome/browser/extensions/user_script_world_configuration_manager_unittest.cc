// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/user_script_world_configuration_manager.h"

#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

using UserScriptWorldConfigurationManagerTest = ExtensionServiceTestWithInstall;

// Tests that extension-specified world configurations are cleared on
// extension update. This matches the behavior of the registered content and
// user scripts.
TEST_F(UserScriptWorldConfigurationManagerTest,
       ConfigurationsAreClearedOnExtensionUpdate) {
  InitializeEmptyExtensionService();

  UserScriptWorldConfigurationManager* manager =
      UserScriptWorldConfigurationManager::Get(browser_context());

  static constexpr char kManifest[] =
      R"({
           "name": "World Configuration",
           "version": "%s",
           "manifest_version": 3,
           "permissions": ["userScripts"]
         })";
  auto get_manifest = [](const char* version) {
    return base::StringPrintf(kManifest, version);
  };
  TestExtensionDir extension_dir;

  extension_dir.WriteManifest(get_manifest("0.1"));
  base::FilePath crx_v1 = extension_dir.Pack("v1.crx");

  extension_dir.WriteManifest(get_manifest("0.2"));
  base::FilePath crx_v2 = extension_dir.Pack("v2.crx");

  const Extension* extension = InstallCRX(crx_v1, INSTALL_NEW);
  ASSERT_TRUE(extension);

  // Register two different configurations for user script worlds, one for the
  // default world and another for "world 1".
  manager->SetUserScriptWorldInfo(
      *extension, std::nullopt, "script-src: self", /*enable_messaging=*/false);
  manager->SetUserScriptWorldInfo(
      *extension, "world 1", "script-src: none", /*enable_messaging=*/false);
  EXPECT_EQ(2u, manager->GetAllUserScriptWorlds(extension->id()).size());

  extension = InstallCRX(crx_v2, INSTALL_UPDATED);
  ASSERT_TRUE(extension);

  // Since the extension updated to a new version, the world configurations
  // should have been removed.
  EXPECT_EQ(0u, manager->GetAllUserScriptWorlds(extension->id()).size());
}

}  // namespace extensions

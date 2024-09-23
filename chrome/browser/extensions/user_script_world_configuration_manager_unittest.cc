// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/user_script_world_configuration_manager.h"

#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

class UserScriptWorldConfigurationManagerTest
    : public ExtensionServiceTestWithInstall {
 public:
  UserScriptWorldConfigurationManagerTest() = default;
  ~UserScriptWorldConfigurationManagerTest() override = default;

  void SetUp() override {
    ExtensionServiceTestWithInstall::SetUp();
    InitializeEmptyExtensionService();

    configuration_manager_ =
        UserScriptWorldConfigurationManager::Get(browser_context());
  }

  void TearDown() override {
    configuration_manager_ = nullptr;
    ExtensionServiceTestWithInstall::TearDown();
  }

  UserScriptWorldConfigurationManager* configuration_manager() {
    return configuration_manager_.get();
  }

 private:
  raw_ptr<UserScriptWorldConfigurationManager> configuration_manager_;
};

// Returns a matcher for a mojom::UserScriptWorldInfoPtr.
auto GetWorldMatcher(const ExtensionId& extension_id,
                     const std::optional<std::string>& world_id,
                     const std::optional<std::string>& csp,
                     bool enable_messaging = false) {
  return testing::Pointer(testing::AllOf(
      testing::Field("extension id", &mojom::UserScriptWorldInfo::extension_id,
                     testing::Eq(extension_id)),
      testing::Field("world id", &mojom::UserScriptWorldInfo::world_id,
                     testing::Eq(world_id)),
      testing::Field("csp", &mojom::UserScriptWorldInfo::csp, testing::Eq(csp)),
      testing::Field("enable messaging",
                     &mojom::UserScriptWorldInfo::enable_messaging,
                     testing::Eq(enable_messaging))));
}

// Tests that extension-specified world configurations are cleared on
// extension update. This matches the behavior of the registered content and
// user scripts.
TEST_F(UserScriptWorldConfigurationManagerTest,
       ConfigurationsAreClearedOnExtensionUpdate) {
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
  configuration_manager()->SetUserScriptWorldInfo(
      *extension, std::nullopt, "script-src: self", /*enable_messaging=*/false);
  configuration_manager()->SetUserScriptWorldInfo(
      *extension, "world 1", "script-src: none", /*enable_messaging=*/false);
  EXPECT_EQ(
      2u,
      configuration_manager()->GetAllUserScriptWorlds(extension->id()).size());

  extension = InstallCRX(crx_v2, INSTALL_UPDATED);
  ASSERT_TRUE(extension);

  // Since the extension updated to a new version, the world configurations
  // should have been removed.
  EXPECT_EQ(
      0u,
      configuration_manager()->GetAllUserScriptWorlds(extension->id()).size());
}

// Tests clearing configurations for particular user script worlds.
TEST_F(UserScriptWorldConfigurationManagerTest, ClearingConfigurations) {
  // Create two extensions.
  scoped_refptr<const Extension> extension1 = ExtensionBuilder("ext1").Build();
  scoped_refptr<const Extension> extension2 = ExtensionBuilder("ext2").Build();

  const std::string other_world = "other world";
  const std::string csp1 = "csp1";
  const std::string csp2 = "csp2";
  static constexpr bool kEnableMessaging = false;
  static constexpr std::optional<std::string> default_world;

  // Set configurations for a specified world and the default world for each
  // extension.
  configuration_manager()->SetUserScriptWorldInfo(*extension1, default_world,
                                                  csp1, kEnableMessaging);
  configuration_manager()->SetUserScriptWorldInfo(*extension1, other_world,
                                                  csp2, kEnableMessaging);

  configuration_manager()->SetUserScriptWorldInfo(*extension2, default_world,
                                                  csp1, kEnableMessaging);
  configuration_manager()->SetUserScriptWorldInfo(*extension2, other_world,
                                                  csp2, kEnableMessaging);

  // Verify initial state. Each extension should have those two worlds
  // configured.
  auto ext1_default_world =
      GetWorldMatcher(extension1->id(), default_world, csp1);
  auto ext1_other_world = GetWorldMatcher(extension1->id(), other_world, csp2);
  auto ext2_default_world =
      GetWorldMatcher(extension2->id(), default_world, csp1);
  auto ext2_other_world = GetWorldMatcher(extension2->id(), other_world, csp2);

  EXPECT_THAT(
      configuration_manager()->GetAllUserScriptWorlds(extension1->id()),
      testing::UnorderedElementsAre(ext1_default_world, ext1_other_world));
  EXPECT_THAT(
      configuration_manager()->GetAllUserScriptWorlds(extension2->id()),
      testing::UnorderedElementsAre(ext2_default_world, ext2_other_world));

  // Next, clear "other world" for the first extension.
  configuration_manager()->ClearUserScriptWorldInfo(*extension1, other_world);

  // The first extension should now only have a configuration for the default
  // world, while the configurations for the second extension are unchanged.
  EXPECT_THAT(configuration_manager()->GetAllUserScriptWorlds(extension1->id()),
              testing::UnorderedElementsAre(ext1_default_world));
  EXPECT_THAT(
      configuration_manager()->GetAllUserScriptWorlds(extension2->id()),
      testing::UnorderedElementsAre(ext2_default_world, ext2_other_world));

  // Remove the configuration for the default world for the second extension.
  configuration_manager()->ClearUserScriptWorldInfo(*extension2, default_world);

  // Now, the first extension should only have the default configuration, while
  // the second extension should only have the other world's configuration.
  EXPECT_THAT(configuration_manager()->GetAllUserScriptWorlds(extension1->id()),
              testing::UnorderedElementsAre(ext1_default_world));
  EXPECT_THAT(configuration_manager()->GetAllUserScriptWorlds(extension2->id()),
              testing::UnorderedElementsAre(ext2_other_world));
}

// Verifies that any user script world configurations found in prefs that
// don't correctly parse are (gracefully) ignored.
TEST_F(UserScriptWorldConfigurationManagerTest,
       InvalidUserScriptWorldConfigurationsAreIgnored) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("ext").Build();

  // Manually write some worlds to the preferences.
  static constexpr char kWorldsDictJson[] =
      R"({
           // Default world. Valid.
           "_default": {
             "messaging": true,
             "csp": "default"
           },
           // Some other world. Valid.
           "other": {
             "messaging": true,
             "csp": "other"
           },
           // No CSP. Valid.
           "noCspSpecified": {
             "messaging": true
           },
           // No messaging key specified. Valid.
           "noMessagingSpecified": {
             "csp": "no messaging"
           },
           // Nothing specified. Also valid (returns the default settings).
           "emptyWorld": {},
           // Another world with an unknown key. We should allow it and simply
           // ignore the unknown key.
           "unknownKey": {
             "messaging": true,
             "csp": "unknown key",
             "unknown_key": "some value"
           },
           // Non-dictionary world. Should be ignored.
           "nonDict": "some value",
           // World ID beginning with '_'. Should be ignored.
           "_reservedWorld": {
             "messaging": true,
             "csp": "reserved"
           }
         })";
  base::Value worlds_json = base::test::ParseJson(kWorldsDictJson);
  ASSERT_TRUE(worlds_json.is_dict());

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
  extension_prefs->UpdateExtensionPref(extension->id(),
                                       kUserScriptsWorldsConfiguration.name,
                                       std::move(worlds_json));

  auto all_worlds =
      configuration_manager()->GetAllUserScriptWorlds(extension->id());

  // Verify that only valid worlds were parsed.
  EXPECT_THAT(
      all_worlds,
      testing::UnorderedElementsAre(
          // Default.
          GetWorldMatcher(extension->id(), std::nullopt, "default", true),
          // Other world.
          GetWorldMatcher(extension->id(), "other", "other", true),
          // No CSP.
          GetWorldMatcher(extension->id(), "noCspSpecified", std::nullopt,
                          true),
          // No messaging.
          GetWorldMatcher(extension->id(), "noMessagingSpecified",
                          "no messaging", false),
          // Unknown key.
          GetWorldMatcher(extension->id(), "unknownKey", "unknown key", true),
          // Empty world.
          GetWorldMatcher(extension->id(), "emptyWorld", std::nullopt, false)));
}

}  // namespace extensions

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/permissions/permissions_test_util.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/user_script.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

using extensions::mojom::ManifestLocation;

namespace extensions {

namespace {

using permissions_test_util::GetPatternsAsStrings;

std::vector<std::string> GetEffectivePatternsAsStrings(
    const Extension& extension) {
  return GetPatternsAsStrings(
      extension.permissions_data()->active_permissions().effective_hosts());
}

std::vector<std::string> GetScriptablePatternsAsStrings(
    const Extension& extension) {
  return GetPatternsAsStrings(
      extension.permissions_data()->active_permissions().scriptable_hosts());
}

std::vector<std::string> GetExplicitPatternsAsStrings(
    const Extension& extension) {
  return GetPatternsAsStrings(
      extension.permissions_data()->active_permissions().explicit_hosts());
}

void InitializeExtensionPermissions(Profile* profile,
                                    const Extension& extension) {
  PermissionsUpdater updater(profile);
  updater.InitializePermissions(&extension);
  updater.GrantActivePermissions(&extension);
}

void CheckActiveHostPermissions(
    const Extension& extension,
    const std::vector<std::string>& explicit_hosts,
    const std::vector<std::string>& scriptable_hosts) {
  EXPECT_THAT(GetExplicitPatternsAsStrings(extension),
              testing::UnorderedElementsAreArray(explicit_hosts));
  EXPECT_THAT(GetScriptablePatternsAsStrings(extension),
              testing::UnorderedElementsAreArray(scriptable_hosts));
}

void CheckWithheldHostPermissions(
    const Extension& extension,
    const std::vector<std::string>& explicit_hosts,
    const std::vector<std::string>& scriptable_hosts) {
  const PermissionsData* permissions_data = extension.permissions_data();
  EXPECT_THAT(GetPatternsAsStrings(
                  permissions_data->withheld_permissions().explicit_hosts()),
              testing::UnorderedElementsAreArray(explicit_hosts));
  EXPECT_THAT(GetPatternsAsStrings(
                  permissions_data->withheld_permissions().scriptable_hosts()),
              testing::UnorderedElementsAreArray(scriptable_hosts));
}

using ScriptingPermissionsModifierUnitTest = ExtensionServiceTestWithInstall;

}  // namespace

TEST_F(ScriptingPermissionsModifierUnitTest, GrantAndWithholdHostPermissions) {
  InitializeEmptyExtensionService();

  std::vector<std::string> test_cases[] = {
      {"http://www.google.com/*"},
      {"http://*/*"},
      {"<all_urls>"},
      {"http://*.com/*"},
      {"http://google.com/*", "<all_urls>"},
  };

  for (const auto& test_case : test_cases) {
    std::string test_case_name = base::JoinString(test_case, ",");
    SCOPED_TRACE(test_case_name);
    scoped_refptr<const Extension> extension =
        ExtensionBuilder(test_case_name)
            .AddHostPermissions(test_case)
            .AddContentScript("foo.js", test_case)
            .SetLocation(ManifestLocation::kInternal)
            .Build();

    PermissionsUpdater(profile()).InitializePermissions(extension.get());
    ASSERT_TRUE(
        PermissionsManager::Get(profile())->CanAffectExtension(*extension));

    // By default, all permissions are granted.
    {
      SCOPED_TRACE("Initial state");
      CheckActiveHostPermissions(*extension, test_case, test_case);
      CheckWithheldHostPermissions(*extension, {}, {});
    }

    // Then, withhold host permissions.
    ScriptingPermissionsModifier modifier(profile(), extension);
    modifier.SetWithholdHostPermissions(true);
    {
      SCOPED_TRACE("After setting to withhold");
      CheckActiveHostPermissions(*extension, {}, {});
      CheckWithheldHostPermissions(*extension, test_case, test_case);
    }

    // Finally, re-grant the withheld permissions.
    modifier.SetWithholdHostPermissions(false);

    // We should be back to our initial state - all requested permissions are
    // granted.
    {
      SCOPED_TRACE("After setting to not withhold");
      CheckActiveHostPermissions(*extension, test_case, test_case);
      CheckWithheldHostPermissions(*extension, {}, {});
    }
  }
}

// Tests that with the creation flag present, requested host permissions are
// withheld on installation, but still allow for individual permissions to be
// granted, or all permissions be set back to not being withheld by default.
TEST_F(ScriptingPermissionsModifierUnitTest, WithholdHostPermissionsOnInstall) {
  InitializeEmptyExtensionService();

  constexpr char kHostGoogle[] = "https://google.com/*";
  constexpr char kHostChromium[] = "https://chromium.org/*";
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("a")
          .AddHostPermissions({kHostGoogle, kHostChromium})
          .AddContentScript("foo.js", {kHostGoogle})
          .SetLocation(ManifestLocation::kInternal)
          .AddFlags(Extension::WITHHOLD_PERMISSIONS)
          .Build();

  // Initialize the permissions and have the prefs built and stored.
  PermissionsUpdater(profile()).InitializePermissions(extension.get());
  ExtensionPrefs::Get(profile())->OnExtensionInstalled(
      extension.get(), Extension::State::ENABLED, syncer::StringOrdinal(), "");

  ASSERT_TRUE(
      PermissionsManager::Get(profile())->CanAffectExtension(*extension));

  // With the flag present, permissions should have been withheld.
  {
    SCOPED_TRACE("Initial state");
    CheckActiveHostPermissions(*extension, {}, {});
    CheckWithheldHostPermissions(*extension, {kHostGoogle, kHostChromium},
                                 {kHostGoogle});
  }

  // Grant one of the permissions manually.
  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.GrantHostPermission(GURL(kHostChromium));

  {
    SCOPED_TRACE("After granting single");
    CheckActiveHostPermissions(*extension, {kHostChromium}, {});
    CheckWithheldHostPermissions(*extension, {kHostGoogle}, {kHostGoogle});
  }

  // Finally, re-grant the withheld permissions.
  modifier.SetWithholdHostPermissions(false);

  // All requested permissions should now be granted granted.
  {
    SCOPED_TRACE("After setting to not withhold");
    CheckActiveHostPermissions(*extension, {kHostGoogle, kHostChromium},
                               {kHostGoogle});
    CheckWithheldHostPermissions(*extension, {}, {});
  }
}

// Tests that reloading an extension after withholding host permissions on
// installation retains the correct state and any changes that have been made
// since installation.
TEST_F(ScriptingPermissionsModifierUnitTest,
       WithholdOnInstallPreservedOnReload) {
  InitializeEmptyExtensionService();

  constexpr char kHostGoogle[] = "https://google.com/*";
  constexpr char kHostChromium[] = "https://chromium.org/*";
  TestExtensionDir test_extension_dir;
  test_extension_dir.WriteManifest(
      R"({
           "name": "foo",
           "manifest_version": 2,
           "version": "1",
           "permissions": ["https://google.com/*", "https://chromium.org/*"]
         })");
  ChromeTestExtensionLoader loader(profile());
  loader.add_creation_flag(Extension::WITHHOLD_PERMISSIONS);
  loader.set_pack_extension(true);
  scoped_refptr<const Extension> extension =
      loader.LoadExtension(test_extension_dir.UnpackedPath());
  // Cache the ID, since the extension will be invalidated across reloads.
  ExtensionId extension_id = extension->id();

  auto reload_extension = [this, &extension_id]() {
    TestExtensionRegistryObserver observer(ExtensionRegistry::Get(profile()));
    service()->ReloadExtension(extension_id);
    return observer.WaitForExtensionLoaded();
  };

  // Permissions start withheld due to creation flag and remain withheld after
  // reload.
  {
    SCOPED_TRACE("Initial state");
    CheckActiveHostPermissions(*extension, {}, {});
    CheckWithheldHostPermissions(*extension, {kHostGoogle, kHostChromium}, {});
  }

  {
    SCOPED_TRACE("Reload after initial state");
    extension = reload_extension();
    CheckActiveHostPermissions(*extension, {}, {});
    CheckWithheldHostPermissions(*extension, {kHostGoogle, kHostChromium}, {});
  }

  // Grant one of the permissions and check it persists after reload.
  ScriptingPermissionsModifier(profile(), extension)
      .GrantHostPermission(GURL(kHostGoogle));
  {
    SCOPED_TRACE("Granting single");
    CheckActiveHostPermissions(*extension, {kHostGoogle}, {});
    CheckWithheldHostPermissions(*extension, {kHostChromium}, {});
  }

  {
    SCOPED_TRACE("Reload after granting single");
    extension = reload_extension();
    CheckActiveHostPermissions(*extension, {kHostGoogle}, {});
    CheckWithheldHostPermissions(*extension, {kHostChromium}, {});
  }

  // Set permissions not to be withheld at all and check it persists after
  // reload.
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(false);
  {
    SCOPED_TRACE("Setting to not withhold");
    CheckActiveHostPermissions(*extension, {kHostGoogle, kHostChromium}, {});
    CheckWithheldHostPermissions(*extension, {}, {});
  }

  {
    SCOPED_TRACE("Reload after setting to not withhold");
    extension = reload_extension();
    CheckActiveHostPermissions(*extension, {kHostGoogle, kHostChromium}, {});
    CheckWithheldHostPermissions(*extension, {}, {});
  }

  // Finally, set permissions to be withheld again and check it persists after
  // reload.
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);
  {
    SCOPED_TRACE("Setting back to withhold");
    CheckActiveHostPermissions(*extension, {}, {});
    CheckWithheldHostPermissions(*extension, {kHostGoogle, kHostChromium}, {});
  }

  {
    SCOPED_TRACE("Reload after setting back to withhold");
    extension = reload_extension();
    CheckActiveHostPermissions(*extension, {}, {});
    CheckWithheldHostPermissions(*extension, {kHostGoogle, kHostChromium}, {});
  }
}

// Tests that updating an extension after withholding host permissions on
// installation retains the correct state and any changes that have been made
// since installation.
TEST_F(ScriptingPermissionsModifierUnitTest,
       WithholdOnInstallPreservedOnUpdate) {
  InitializeEmptyExtensionService();

  constexpr char kHostGoogle[] = "https://google.com/*";
  constexpr char kHostChromium[] = "https://chromium.org/*";
  TestExtensionDir test_extension_dir;
  constexpr char kManifestTemplate[] =
      R"({
           "name": "foo",
           "manifest_version": 2,
           "version": "%s",
           "permissions": ["https://google.com/*", "https://chromium.org/*"]
         })";

  test_extension_dir.WriteManifest(base::StringPrintf(kManifestTemplate, "1"));
  // We need to use a pem file here for consistent update IDs.
  const base::FilePath pem_path =
      data_dir().AppendASCII("permissions/update.pem");
  scoped_refptr<const Extension> extension = PackAndInstallCRX(
      test_extension_dir.UnpackedPath(), pem_path, INSTALL_NEW,
      Extension::WITHHOLD_PERMISSIONS, mojom::ManifestLocation::kInternal);
  // Cache the ID, since the extension will be invalidated across updates.
  ExtensionId extension_id = extension->id();
  // Hold onto references for the extension dirs so they don't get deleted
  // outside the lambda.
  std::vector<TestExtensionDir> extension_dirs;

  auto update_extension = [this, &extension_id, &pem_path, &kManifestTemplate,
                           &extension_dirs](const char* version) {
    TestExtensionDir update_version;
    update_version.WriteManifest(
        base::StringPrintf(kManifestTemplate, version));
    PackCRXAndUpdateExtension(extension_id, update_version.UnpackedPath(),
                              pem_path, ENABLED);
    scoped_refptr<const Extension> updated_extension =
        registry()->GetInstalledExtension(extension_id);

    EXPECT_EQ(version, updated_extension->version().GetString());
    extension_dirs.push_back(std::move(update_version));
    return updated_extension;
  };

  // Permissions start withheld due to creation flag and remain withheld after
  // update.
  {
    SCOPED_TRACE("Initial state");
    CheckActiveHostPermissions(*extension, {}, {});
    CheckWithheldHostPermissions(*extension, {kHostGoogle, kHostChromium}, {});
  }

  {
    SCOPED_TRACE("Update after initial state");
    extension = update_extension("2");
    CheckActiveHostPermissions(*extension, {}, {});
    CheckWithheldHostPermissions(*extension, {kHostGoogle, kHostChromium}, {});
  }

  // Grant one of the permissions and check it persists after update.
  ScriptingPermissionsModifier(profile(), extension)
      .GrantHostPermission(GURL(kHostGoogle));
  {
    SCOPED_TRACE("Granting single");
    CheckActiveHostPermissions(*extension, {kHostGoogle}, {});
    CheckWithheldHostPermissions(*extension, {kHostChromium}, {});
  }

  {
    SCOPED_TRACE("Update after granting single");
    extension = update_extension("3");
    CheckActiveHostPermissions(*extension, {kHostGoogle}, {});
    CheckWithheldHostPermissions(*extension, {kHostChromium}, {});
  }

  // Set permissions not to be withheld at all and check it persists after
  // update.
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(false);
  {
    SCOPED_TRACE("Setting to not withhold");
    CheckActiveHostPermissions(*extension, {kHostGoogle, kHostChromium}, {});
    CheckWithheldHostPermissions(*extension, {}, {});
  }

  {
    SCOPED_TRACE("Update after setting to not withhold");
    extension = update_extension("4");
    CheckActiveHostPermissions(*extension, {kHostGoogle, kHostChromium}, {});
    CheckWithheldHostPermissions(*extension, {}, {});
  }

  // Finally, set permissions to be withheld again and check it persists after
  // update.
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);
  {
    SCOPED_TRACE("Setting back to withhold");
    CheckActiveHostPermissions(*extension, {}, {});
    CheckWithheldHostPermissions(*extension, {kHostGoogle, kHostChromium}, {});
  }

  {
    SCOPED_TRACE("Update after setting back to withhold");
    extension = update_extension("5");
    CheckActiveHostPermissions(*extension, {}, {});
    CheckWithheldHostPermissions(*extension, {kHostGoogle, kHostChromium}, {});
  }
}

TEST_F(ScriptingPermissionsModifierUnitTest, SwitchBehavior) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("a")
          .AddHostPermission(URLPattern::kAllUrlsPattern)
          .AddContentScript("foo.js", {URLPattern::kAllUrlsPattern})
          .SetLocation(ManifestLocation::kInternal)
          .Build();
  PermissionsUpdater updater(profile());
  updater.InitializePermissions(extension.get());
  const PermissionsData* permissions_data = extension->permissions_data();

  // By default, the extension should have all its permissions.
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension),
              testing::UnorderedElementsAre(URLPattern::kAllUrlsPattern));
  EXPECT_TRUE(
      permissions_data->withheld_permissions().effective_hosts().is_empty());
  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  EXPECT_FALSE(permissions_manager->HasWithheldHostPermissions(*extension));

  // Revoke access.
  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);
  EXPECT_TRUE(permissions_manager->HasWithheldHostPermissions(*extension));
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());
  EXPECT_THAT(GetPatternsAsStrings(
                  permissions_data->withheld_permissions().effective_hosts()),
              testing::UnorderedElementsAre(URLPattern::kAllUrlsPattern));
}

TEST_F(ScriptingPermissionsModifierUnitTest, GrantHostPermission) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddHostPermission(URLPattern::kAllUrlsPattern)
          .AddContentScript("foo.js", {URLPattern::kAllUrlsPattern})
          .SetLocation(ManifestLocation::kInternal)
          .Build();
  PermissionsUpdater(profile()).InitializePermissions(extension.get());

  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  const GURL kUrl("https://www.google.com/");
  const GURL kUrl2("https://www.chromium.org/");

  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  EXPECT_FALSE(permissions_manager->HasGrantedHostPermission(*extension, kUrl));
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, kUrl2));

  const PermissionsData* permissions_data = extension->permissions_data();
  auto get_page_access = [&permissions_data](const GURL& url) {
    return permissions_data->GetPageAccess(url, 0, nullptr);
  };

  EXPECT_EQ(PermissionsData::PageAccess::kWithheld, get_page_access(kUrl));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld, get_page_access(kUrl2));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  {
    std::unique_ptr<const PermissionSet> permissions =
        prefs->GetRuntimeGrantedPermissions(extension->id());
    EXPECT_FALSE(permissions->effective_hosts().MatchesURL(kUrl));
    EXPECT_FALSE(permissions->effective_hosts().MatchesURL(kUrl2));
  }

  modifier.GrantHostPermission(kUrl);
  EXPECT_TRUE(permissions_manager->HasGrantedHostPermission(*extension, kUrl));
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, kUrl2));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed, get_page_access(kUrl));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld, get_page_access(kUrl2));
  {
    std::unique_ptr<const PermissionSet> permissions =
        prefs->GetRuntimeGrantedPermissions(extension->id());
    EXPECT_TRUE(permissions->effective_hosts().MatchesURL(kUrl));
    EXPECT_FALSE(permissions->effective_hosts().MatchesURL(kUrl2));
  }

  modifier.RemoveGrantedHostPermission(kUrl);
  EXPECT_FALSE(permissions_manager->HasGrantedHostPermission(*extension, kUrl));
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, kUrl2));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld, get_page_access(kUrl));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld, get_page_access(kUrl2));
  {
    std::unique_ptr<const PermissionSet> permissions =
        prefs->GetRuntimeGrantedPermissions(extension->id());
    EXPECT_FALSE(permissions->effective_hosts().MatchesURL(kUrl));
    EXPECT_FALSE(permissions->effective_hosts().MatchesURL(kUrl2));
  }
}

// Tests ScriptingPermissinsModifier::GrantHostPermission() grants optional host
// permissions which may have been withheld
TEST_F(ScriptingPermissionsModifierUnitTest, GrantedOptionalHostPermission) {
  InitializeEmptyExtensionService();

  // Add an extension with <all_urls> optional host permission.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddOptionalHostPermission(URLPattern::kAllUrlsPattern)
          .SetLocation(ManifestLocation::kInternal)
          .Build();
  PermissionsUpdater(profile()).InitializePermissions(extension.get());

  ScriptingPermissionsModifier modifier(profile(), extension);

  const GURL kUrl("https://www.chromium.org/");
  const GURL kOtherUrl("https://www.example.com/");

  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  EXPECT_FALSE(permissions_manager->HasGrantedHostPermission(*extension, kUrl));

  const PermissionsData* permissions_data = extension->permissions_data();
  auto get_page_access = [&permissions_data](const GURL& url) {
    return permissions_data->GetPageAccess(url, 0, nullptr);
  };

  // No host permissions should have been granted to the extension at this
  // point.
  ASSERT_EQ(PermissionsData::PageAccess::kDenied, get_page_access(kUrl));

  // Now grant the optional host permissions.
  PermissionsUpdater updater(profile());
  updater.GrantOptionalPermissions(
      *extension, PermissionsParser::GetOptionalPermissions(extension.get()),
      base::DoNothing());

  // The extension should now be allowed to run on all hosts. Verify this by
  // checking the page access on kUrl and kOtherUrl.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  {
    std::unique_ptr<const PermissionSet> permissions =
        prefs->GetRuntimeGrantedPermissions(extension->id());
    ASSERT_TRUE(permissions->effective_hosts().MatchesURL(kUrl));
    ASSERT_EQ(PermissionsData::PageAccess::kAllowed, get_page_access(kUrl));
    ASSERT_EQ(PermissionsData::PageAccess::kAllowed,
              get_page_access(kOtherUrl));
  }

  // Grant the extension access to kUrl only by first withholding all host
  // permissions and then granting access to kUrl only.
  modifier.SetWithholdHostPermissions(true);
  EXPECT_FALSE(permissions_manager->HasGrantedHostPermission(*extension, kUrl));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld, get_page_access(kUrl));
  modifier.GrantHostPermission(kUrl);

  // Verify that kUrl is re-granted as the extension had already been granted
  // access to <all_urls> which encompasses access to kUrl. Access to kOtherUrl
  // should now be withheld.
  EXPECT_TRUE(permissions_manager->HasGrantedHostPermission(*extension, kUrl));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed, get_page_access(kUrl));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld, get_page_access(kOtherUrl));
  {
    std::unique_ptr<const PermissionSet> permissions =
        prefs->GetRuntimeGrantedPermissions(extension->id());
    EXPECT_TRUE(permissions->effective_hosts().MatchesURL(kUrl));
    EXPECT_FALSE(permissions->effective_hosts().MatchesURL(kOtherUrl));
  }
}

TEST_F(ScriptingPermissionsModifierUnitTest,
       ExtensionsInitializedWithSavedRuntimeGrantedHostPermissionsAcrossLoad) {
  InitializeEmptyExtensionService();

  const GURL kExampleCom("https://example.com/");
  const GURL kChromiumOrg("https://chromium.org/");
  const URLPatternSet kExampleComPatternSet({URLPattern(
      Extension::kValidHostPermissionSchemes, "https://example.com/")});

  TestExtensionDir test_extension_dir;
  test_extension_dir.WriteManifest(
      R"({
           "name": "foo",
           "manifest_version": 2,
           "version": "1",
           "permissions": ["<all_urls>"]
         })");
  ChromeTestExtensionLoader loader(profile());
  loader.set_grant_permissions(true);
  scoped_refptr<const Extension> extension =
      loader.LoadExtension(test_extension_dir.UnpackedPath());

  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  .explicit_hosts()
                  .MatchesURL(kExampleCom));
  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  .explicit_hosts()
                  .MatchesURL(kChromiumOrg));

  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);
  EXPECT_FALSE(extension->permissions_data()
                   ->active_permissions()
                   .explicit_hosts()
                   .MatchesURL(kExampleCom));
  EXPECT_FALSE(extension->permissions_data()
                   ->active_permissions()
                   .explicit_hosts()
                   .MatchesURL(kChromiumOrg));

  ScriptingPermissionsModifier(profile(), extension)
      .GrantHostPermission(kExampleCom);
  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  .explicit_hosts()
                  .MatchesURL(kExampleCom));
  EXPECT_FALSE(extension->permissions_data()
                   ->active_permissions()
                   .explicit_hosts()
                   .MatchesURL(kChromiumOrg));

  {
    TestExtensionRegistryObserver observer(ExtensionRegistry::Get(profile()));
    service()->ReloadExtension(extension->id());
    extension = observer.WaitForExtensionLoaded();
  }
  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  .explicit_hosts()
                  .MatchesURL(kExampleCom));
  EXPECT_FALSE(extension->permissions_data()
                   ->active_permissions()
                   .explicit_hosts()
                   .MatchesURL(kChromiumOrg));
}

// Test ScriptingPermissionsModifier::RemoveAllGrantedHostPermissions() revokes
// hosts granted through the ScriptingPermissionsModifier.
TEST_F(ScriptingPermissionsModifierUnitTest,
       RemoveAllGrantedHostPermissions_GrantedHosts) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddHostPermission("<all_urls>").Build();
  ScriptingPermissionsModifier modifier(profile(), extension.get());

  modifier.SetWithholdHostPermissions(true);

  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());

  modifier.GrantHostPermission(GURL("https://example.com"));
  modifier.GrantHostPermission(GURL("https://chromium.org"));

  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension),
              testing::UnorderedElementsAre("https://example.com/*",
                                            "https://chromium.org/*"));

  modifier.RemoveAllGrantedHostPermissions();
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());
}

// Test ScriptingPermissionsModifier::RemoveAllGrantedHostPermissions() revokes
// hosts granted through the ScriptingPermissionsModifier for extensions that
// don't request <all_urls>.
TEST_F(ScriptingPermissionsModifierUnitTest,
       RemoveAllGrantedHostPermissions_GrantedHostsForNonAllUrlsExtension) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test")
          .AddHostPermissions(
              {"https://example.com/*", "https://chromium.org/*"})
          .Build();
  ScriptingPermissionsModifier modifier(profile(), extension.get());

  modifier.SetWithholdHostPermissions(true);

  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());

  modifier.GrantHostPermission(GURL("https://example.com"));
  modifier.GrantHostPermission(GURL("https://chromium.org"));

  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension),
              testing::UnorderedElementsAre("https://example.com/*",
                                            "https://chromium.org/*"));

  modifier.RemoveAllGrantedHostPermissions();
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());
}

// Test ScriptingPermissionsModifier::RemoveAllGrantedHostPermissions() revokes
// granted optional host permissions.
TEST_F(ScriptingPermissionsModifierUnitTest,
       RemoveAllGrantedHostPermissions_GrantedOptionalPermissions) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test")
          .AddOptionalHostPermission("https://example.com/*")
          .Build();

  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());

  {
    // Simulate adding an optional permission, which should also be revokable.
    URLPatternSet patterns;
    patterns.AddPattern(URLPattern(Extension::kValidHostPermissionSchemes,
                                   "https://example.com/*"));
    permissions_test_util::GrantOptionalPermissionsAndWaitForCompletion(
        profile(), *extension,
        PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                      std::move(patterns), URLPatternSet()));
  }

  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension),
              testing::UnorderedElementsAre("https://example.com/*"));

  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.RemoveAllGrantedHostPermissions();
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());
}

// Tests that HasBroadGrantedHostPermissions detects cases where there is a
// granted permission that is sufficiently broad enough to be counted as akin to
// <all_urls> type permissions.
TEST_F(ScriptingPermissionsModifierUnitTest, HasBroadGrantedHostPermissions) {
  InitializeEmptyExtensionService();

  struct {
    std::vector<std::string> hosts;
    bool expected_broad_permissions;
  } test_cases[] = {{{}, false},
                    {{"https://www.google.com/*"}, false},
                    {{"https://www.google.com/*", "*://chromium.org/*"}, false},
                    {{"*://*.google.*/*"}, false},
                    {{"<all_urls>"}, true},
                    {{"https://*/*"}, true},
                    {{"*://*/*"}, true},
                    {{"https://www.google.com/*", "<all_urls>"}, true}};

  for (const auto& test_case : test_cases) {
    std::string test_case_name = base::JoinString(test_case.hosts, ",");
    SCOPED_TRACE(test_case_name);
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("test: " + test_case_name)
            .AddHostPermission("<all_urls>")
            .Build();

    ScriptingPermissionsModifier modifier(profile(), extension.get());
    modifier.SetWithholdHostPermissions(true);

    PermissionsManager* permissions_manager =
        PermissionsManager::Get(profile());
    EXPECT_FALSE(
        permissions_manager->HasBroadGrantedHostPermissions(*extension));

    std::string error;
    bool allow_file_access = false;
    URLPatternSet patterns;
    patterns.Populate(test_case.hosts, Extension::kValidHostPermissionSchemes,
                      allow_file_access, &error);
    permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
        profile(), *extension,
        PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                      std::move(patterns), URLPatternSet()));

    EXPECT_EQ(test_case.expected_broad_permissions,
              permissions_manager->HasBroadGrantedHostPermissions(*extension));
  }
}

// Tests RemoveBroadGrantedHostPermissions only removes the broad permissions
// and leaves others intact.
TEST_F(ScriptingPermissionsModifierUnitTest,
       RemoveBroadGrantedHostPermissions) {
  InitializeEmptyExtensionService();

  const GURL google_com = GURL("https://google.com/*");
  const GURL example_com = GURL("https://example.com/*");

  // Define a list of broad patters that should give access to both URLs.
  std::string broad_patterns[] = {"https://*/*", "<all_urls>",
                                  "https://*.com/*"};

  for (const auto& broad_pattern : broad_patterns) {
    SCOPED_TRACE(broad_pattern);
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("test: " + broad_pattern)
            .AddHostPermission("<all_urls>")
            .Build();
    ScriptingPermissionsModifier modifier(profile(), extension.get());

    modifier.SetWithholdHostPermissions(true);

    // Explicitly grant google.com and the broad pattern.
    modifier.GrantHostPermission(google_com);
    const URLPattern pattern(Extension::kValidHostPermissionSchemes,
                             broad_pattern);
    permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
        profile(), *extension,
        PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                      URLPatternSet({pattern}), URLPatternSet()));

    PermissionsManager* permissions_manager =
        PermissionsManager::Get(profile());
    EXPECT_TRUE(
        permissions_manager->HasGrantedHostPermission(*extension, google_com));
    EXPECT_TRUE(
        permissions_manager->HasGrantedHostPermission(*extension, example_com));

    // Now removing the broad patterns should leave it only with the explicit
    // google permission.
    modifier.RemoveBroadGrantedHostPermissions();
    EXPECT_TRUE(
        permissions_manager->HasGrantedHostPermission(*extension, google_com));
    EXPECT_FALSE(
        permissions_manager->HasGrantedHostPermission(*extension, example_com));
    EXPECT_FALSE(
        permissions_manager->HasBroadGrantedHostPermissions(*extension));
  }
}

// Tests granting runtime permissions for a full host when the extension only
// wants to run on a subset of that host.
TEST_F(ScriptingPermissionsModifierUnitTest,
       GrantingHostPermissionsBeyondRequested) {
  InitializeEmptyExtensionService();

  static constexpr char kContentScripts[] = R"([
    {
      "matches": ["https://google.com/maps"],
      "js": ["foo.js"]
    }
  ])";
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test")
          .SetManifestKey("content_scripts",
                          base::test::ParseJsonList(kContentScripts))
          .Build();

  // At installation, all permissions granted.
  ScriptingPermissionsModifier modifier(profile(), extension);
  PermissionsManager* manager = PermissionsManager::Get(profile());
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension),
              testing::UnorderedElementsAre("https://google.com/maps"));

  // Withhold host permissions.
  modifier.SetWithholdHostPermissions(true);
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());

  // Grant the requested host permission. We grant origins (rather than just
  // paths), but we don't over-grant permissions to the actual extension object.
  // The active permissions on the extension should be restricted to the
  // permissions explicitly requested (google.com/maps), but the granted
  // permissions in preferences will be the full host (google.com).
  modifier.GrantHostPermission(GURL("https://google.com/maps"));
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension),
              testing::UnorderedElementsAre("https://google.com/maps"));
  EXPECT_THAT(
      GetPatternsAsStrings(
          manager->GetRevokablePermissions(*extension)->effective_hosts()),
      // Subtle: revokable permissions include permissions either in
      // the runtime granted permissions preference or active on the
      // extension object. In this case, that includes both google.com/*
      // and google.com/maps.
      testing::UnorderedElementsAre("https://google.com/maps",
                                    "https://google.com/*"));

  // Remove the granted permission. This should remove the permission from both
  // the active permissions on the extension object and the entry in the
  // preferences.
  modifier.RemoveAllGrantedHostPermissions();
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());
  EXPECT_THAT(
      GetPatternsAsStrings(
          manager->GetRevokablePermissions(*extension)->effective_hosts()),
      testing::IsEmpty());
}

// TODO(crbug.com/40817514): Move test to PermissionsManager once permissions
// can be withheld in the extensions directory since this test checks important
// part of the PermissionsManager logic.
TEST_F(ScriptingPermissionsModifierUnitTest, ChangeHostPermissions_AllHosts) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddHostPermission("<all_urls>").Build();
  InitializeExtensionPermissions(profile(), *extension);
  auto* manager = PermissionsManager::Get(profile());

  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.SetWithholdHostPermissions(true);

  // Verify a non-restricted site has wihthheld both site access and all sites
  // access.
  const GURL example_com("https://www.example.com");
  {
    const PermissionsManager::ExtensionSiteAccess site_access =
        manager->GetSiteAccess(*extension, example_com);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_TRUE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_TRUE(site_access.withheld_all_sites_access);
  }

  // Verify a restricted site does not have site access withheld, but it has all
  // sites withheld.
  const GURL chrome_extensions("chrome://extensions");
  {
    const PermissionsManager::ExtensionSiteAccess site_access =
        manager->GetSiteAccess(*extension, chrome_extensions);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_TRUE(site_access.withheld_all_sites_access);
  }

  modifier.GrantHostPermission(example_com);

  // Verify the granted url has site access but all sites are still withheld.
  {
    const PermissionsManager::ExtensionSiteAccess site_access =
        manager->GetSiteAccess(*extension, example_com);
    EXPECT_TRUE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_TRUE(site_access.withheld_all_sites_access);
  }

  // Verify the non-granted url has withheld both sites access and all sites
  // access.
  const GURL google_com("https://google.com");
  {
    const PermissionsManager::ExtensionSiteAccess site_access =
        manager->GetSiteAccess(*extension, google_com);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_TRUE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_TRUE(site_access.withheld_all_sites_access);
  }
}

// TODO(crbug.com/40817514): Move test to PermissionsManager once permissions
// can be withheld in the extensions directory since this test checks important
// part of the PermissionsManager logic.
TEST_F(ScriptingPermissionsModifierUnitTest,
       ChangeHostPermissions_AllHostsLike) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddHostPermission("*://*.com/*").Build();
  InitializeExtensionPermissions(profile(), *extension);

  ScriptingPermissionsModifier(profile(), extension.get())
      .SetWithholdHostPermissions(true);

  // Verify a non-restricted site has wihtheld both site access and all sites
  // access.
  const GURL example_com("https://www.example.com");
  {
    const PermissionsManager::ExtensionSiteAccess site_access =
        PermissionsManager::Get(profile())->GetSiteAccess(*extension,
                                                          example_com);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_TRUE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_TRUE(site_access.withheld_all_sites_access);
  }
}

// TODO(crbug.com/40817514): Move test to PermissionsManager once permissions
// can be withheld in the extensions directory since this test checks important
// part of the PermissionsManager logic
TEST_F(ScriptingPermissionsModifierUnitTest,
       ChangeHostPermissions_SpecificSite) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddHostPermission("*://*.example.com/*")
          .Build();
  InitializeExtensionPermissions(profile(), *extension);

  ScriptingPermissionsModifier(profile(), extension.get())
      .SetWithholdHostPermissions(true);

  // Verify a requested sited has wihtheld both site access and all sites
  // access.
  const GURL example_com("https://www.example.com");
  {
    const PermissionsManager::ExtensionSiteAccess site_access =
        PermissionsManager::Get(profile())->GetSiteAccess(*extension,
                                                          example_com);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_TRUE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }
}

// TODO(crbug.com/40817514): Move test to PermissionsManager once permissions
// can be withheld in the extensions directory since this test checks important
// part of the PermissionsManager logic
TEST_F(ScriptingPermissionsModifierUnitTest, AddRuntimeGrantedHostPermission) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddHostPermission("*://*.example.com/*")
          .Build();
  InitializeExtensionPermissions(profile(), *extension);
  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.SetWithholdHostPermissions(true);

  const URLPatternSet google_com_pattern({URLPattern(
      Extension::kValidHostPermissionSchemes, "https://google.com/*")});
  ExtensionPrefs::Get(profile())->AddRuntimeGrantedPermissions(
      extension->id(),
      PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                    google_com_pattern.Clone(), google_com_pattern.Clone()));

  const GURL google_com("https://google.com");
  {
    const PermissionsManager::ExtensionSiteAccess site_access =
        PermissionsManager(profile()).GetSiteAccess(*extension, google_com);
    // The has_access and withheld_access bits should be set appropriately, even
    // if the extension has access to a site it didn't request.
    EXPECT_TRUE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }
}

// Tests that for the purposes of displaying an extension's site access to the
// user (or granting/revoking permissions), we ignore paths in the URL.
// TODO(crbug.com/40817514): Move test to PermissionsManager once permissions
// can be withheld in the extensions directory since this test checks important
// part of the PermissionsManager logic
TEST_F(ScriptingPermissionsModifierUnitTest,
       ChangeHostPermissions_IgnorePaths) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddContentScript("foo.js", {"https://www.example.com/foo"})
          .SetLocation(ManifestLocation::kInternal)
          .Build();
  InitializeExtensionPermissions(profile(), *extension);

  auto* manager = PermissionsManager::Get(profile());

  const GURL example_com("https://www.example.com/bar");
  {
    const PermissionsManager::ExtensionSiteAccess site_access =
        manager->GetSiteAccess(*extension, example_com);
    // Even though the path doesn't exactly match one in the content scripts,
    // the domain is requested, and thus we treat it as if the site was
    // requested.
    EXPECT_TRUE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }

  ScriptingPermissionsModifier(profile(), extension.get())
      .SetWithholdHostPermissions(true);
  {
    const PermissionsManager::ExtensionSiteAccess site_access =
        manager->GetSiteAccess(*extension, example_com);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_TRUE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }
}

// Tests that removing access for a host removes all patterns that grant access
// to that host.
TEST_F(ScriptingPermissionsModifierUnitTest,
       RemoveHostAccess_RemovesOverlappingPatterns) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddHostPermission("*://*/*").Build();
  InitializeExtensionPermissions(profile(), *extension);
  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.SetWithholdHostPermissions(true);

  const URLPattern all_com_pattern(Extension::kValidHostPermissionSchemes,
                                   "https://*.com/*");
  permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
      profile(), *extension,
      PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                    URLPatternSet({all_com_pattern}), URLPatternSet()));

  // Removing example.com access should result in *.com access being revoked,
  // since that is the pattern that grants access to example.com.
  const GURL example_com("https://www.example.com");
  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, example_com));
  modifier.RemoveGrantedHostPermission(example_com);
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, example_com));
  EXPECT_TRUE(ExtensionPrefs::Get(profile())
                  ->GetRuntimeGrantedPermissions(extension->id())
                  ->explicit_hosts()
                  .is_empty());
}

// Test that granting <all_urls> as an optional permission, and then revoking
// it, behaves properly. Regression test for https://crbug.com/930062.
TEST_F(ScriptingPermissionsModifierUnitTest,
       RemoveAllURLsGrantedOptionalPermission) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddOptionalHostPermission("<all_urls>")
          .Build();
  InitializeExtensionPermissions(profile(), *extension);

  // Also verify the extension doesn't have file access, so that <all_urls>
  // shouldn't match file URLs either.
  EXPECT_FALSE(util::AllowFileAccess(extension->id(), profile()));

  {
    PermissionsUpdater updater(profile());
    updater.GrantOptionalPermissions(
        *extension, PermissionsParser::GetOptionalPermissions(extension.get()),
        base::DoNothing());
  }

  ScriptingPermissionsModifier(profile(), extension.get())
      .SetWithholdHostPermissions(true);

  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());
}

}  // namespace extensions

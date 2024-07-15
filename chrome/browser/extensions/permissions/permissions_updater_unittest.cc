// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions/permissions_updater.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/permissions/permissions_test_util.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

using extension_test_util::LoadManifest;
using extensions::mojom::APIPermissionID;

namespace extensions {

namespace {

scoped_refptr<const Extension> CreateExtensionWithOptionalPermissions(
    base::Value::List optional_permissions,
    base::Value::List permissions,
    const std::string& name) {
  return ExtensionBuilder()
      .SetLocation(mojom::ManifestLocation::kInternal)
      .SetManifest(
          base::Value::Dict()
              .Set("name", name)
              .Set("description", "foo")
              .Set("manifest_version", 2)
              .Set("version", "0.1.2.3")
              .Set("permissions", std::move(permissions))
              .Set("optional_permissions", std::move(optional_permissions)))
      .SetID(crx_file::id_util::GenerateId(name))
      .Build();
}

class PermissionsUpdaterTest : public ExtensionServiceTestBase {};

void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

}  // namespace

// Test that the PermissionUpdater can correctly add and remove active
// permissions. This tests all of PermissionsUpdater's public methods because
// GrantActivePermissions and SetPermissions are used by AddPermissions.
TEST_F(PermissionsUpdaterTest, GrantAndRevokeOptionalPermissions) {
  InitializeEmptyExtensionService();

  // Load the test extension.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("permissions")
          .AddAPIPermission("management")
          .AddHostPermission("http://a.com/*")
          .AddOptionalAPIPermission("notifications")
          .AddOptionalHostPermission("http://*.c.com/*")
          .Build();

  {
    PermissionsUpdater updater(profile());
    updater.InitializePermissions(extension.get());
    // Grant the active permissions, as if the extension had just been
    // installed.
    updater.GrantActivePermissions(extension.get());
  }

  APIPermissionSet default_apis;
  default_apis.insert(APIPermissionID::kManagement);

  URLPatternSet default_hosts;
  AddPattern(&default_hosts, "http://a.com/*");
  PermissionSet default_permissions(default_apis.Clone(),
                                    ManifestPermissionSet(),
                                    std::move(default_hosts), URLPatternSet());

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_.get());
  std::unique_ptr<const PermissionSet> active_permissions;
  std::unique_ptr<const PermissionSet> granted_permissions;

  // Make sure it loaded properly.
  ASSERT_EQ(default_permissions,
            extension->permissions_data()->active_permissions());
  EXPECT_EQ(default_permissions,
            *prefs->GetGrantedPermissions(extension->id()));

  // Add a few permissions.
  APIPermissionSet apis;
  apis.insert(APIPermissionID::kNotifications);
  URLPatternSet hosts;
  AddPattern(&hosts, "http://*.c.com/*");

  {
    PermissionSet delta(apis.Clone(), ManifestPermissionSet(), hosts.Clone(),
                        URLPatternSet());

    PermissionsManagerWaiter waiter(PermissionsManager::Get(profile_.get()));
    PermissionsUpdater(profile_.get())
        .GrantOptionalPermissions(*extension, delta, base::DoNothing());
    waiter.WaitForExtensionPermissionsUpdate(base::BindOnce(
        [](scoped_refptr<const Extension> extension, PermissionSet* delta,
           const Extension& actual_extension,
           const PermissionSet& actual_permissions,
           PermissionsManager::UpdateReason actual_reason) {
          ASSERT_EQ(extension.get()->id(), actual_extension.id());
          ASSERT_EQ(*delta, actual_permissions);
          ASSERT_EQ(PermissionsManager::UpdateReason::kAdded, actual_reason);
        },
        extension, &delta));

    // Make sure the extension's active permissions reflect the change.
    active_permissions = PermissionSet::CreateUnion(default_permissions, delta);
    ASSERT_EQ(*active_permissions,
              extension->permissions_data()->active_permissions());

    // Verify that the new granted and active permissions were also stored
    // in the extension preferences. In this case, the granted permissions
    // should be equal to the active permissions.
    ASSERT_EQ(*active_permissions,
              *prefs->GetDesiredActivePermissions(extension->id()));
    granted_permissions = active_permissions->Clone();
    ASSERT_EQ(*granted_permissions,
              *prefs->GetGrantedPermissions(extension->id()));
  }

  {
    // In the second part of the test, we'll remove the permissions that we
    // just added except for 'notifications'.
    apis.erase(APIPermissionID::kNotifications);
    PermissionSet delta(apis.Clone(), ManifestPermissionSet(), hosts.Clone(),
                        URLPatternSet());

    PermissionsManagerWaiter waiter(PermissionsManager::Get(profile_.get()));
    PermissionsUpdater(profile_.get())
        .RevokeOptionalPermissions(*extension, delta,
                                   PermissionsUpdater::REMOVE_SOFT,
                                   base::DoNothing());
    waiter.WaitForExtensionPermissionsUpdate(base::BindOnce(
        [](scoped_refptr<const Extension> extension, PermissionSet* delta,
           const Extension& actual_extension,
           const PermissionSet& actual_permissions,
           PermissionsManager::UpdateReason actual_reason) {
          ASSERT_EQ(extension.get()->id(), actual_extension.id());
          ASSERT_EQ(*delta, actual_permissions);
          ASSERT_EQ(PermissionsManager::UpdateReason::kRemoved, actual_reason);
        },
        extension, &delta));

    // Make sure the extension's active permissions reflect the change.
    active_permissions =
        PermissionSet::CreateDifference(*active_permissions, delta);
    ASSERT_EQ(*active_permissions,
              extension->permissions_data()->active_permissions());

    // Verify that the extension prefs hold the new active permissions and the
    // same granted permissions.
    ASSERT_EQ(*active_permissions,
              *prefs->GetDesiredActivePermissions(extension->id()));

    ASSERT_EQ(*granted_permissions,
              *prefs->GetGrantedPermissions(extension->id()));
  }
}

TEST_F(PermissionsUpdaterTest, RevokingPermissions) {
  InitializeEmptyExtensionService();

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  auto api_permission_set = [](APIPermissionID id) {
    APIPermissionSet apis;
    apis.insert(id);
    return std::make_unique<PermissionSet>(std::move(apis),
                                           ManifestPermissionSet(),
                                           URLPatternSet(), URLPatternSet());
  };

  auto can_access_page =
      [](scoped_refptr<const extensions::Extension> extension,
         const GURL& document_url) -> bool {
    PermissionsData::PageAccess access =
        extension->permissions_data()->GetPageAccess(document_url, -1, nullptr);
    return access == PermissionsData::PageAccess::kAllowed;
  };

  {
    // Test revoking optional permissions.
    auto optional_permissions =
        base::Value::List().Append("tabs").Append("cookies").Append(
            "management");
    base::Value::List required_permissions;
    required_permissions.Append("topSites");
    scoped_refptr<const Extension> extension =
        CreateExtensionWithOptionalPermissions(std::move(optional_permissions),
                                               std::move(required_permissions),
                                               "My Extension");

    PermissionsUpdater updater(profile());
    EXPECT_TRUE(updater.GetRevokablePermissions(extension.get())->IsEmpty());

    // Add the optional "cookies" permission.
    permissions_test_util::GrantOptionalPermissionsAndWaitForCompletion(
        profile(), *extension, *api_permission_set(APIPermissionID::kCookie));
    const PermissionsData* permissions = extension->permissions_data();
    // The extension should have the permission in its active permissions and
    // its granted permissions (stored in prefs). And, the permission should
    // be revokable.
    EXPECT_TRUE(permissions->HasAPIPermission(APIPermissionID::kCookie));
    std::unique_ptr<const PermissionSet> granted_permissions =
        prefs->GetGrantedPermissions(extension->id());
    EXPECT_TRUE(
        granted_permissions->HasAPIPermission(APIPermissionID::kCookie));
    EXPECT_TRUE(updater.GetRevokablePermissions(extension.get())
                    ->HasAPIPermission(APIPermissionID::kCookie));

    // Repeat with "tabs".
    permissions_test_util::GrantOptionalPermissionsAndWaitForCompletion(
        profile(), *extension, *api_permission_set(APIPermissionID::kTab));
    EXPECT_TRUE(permissions->HasAPIPermission(APIPermissionID::kTab));
    granted_permissions = prefs->GetGrantedPermissions(extension->id());
    EXPECT_TRUE(granted_permissions->HasAPIPermission(APIPermissionID::kTab));
    EXPECT_TRUE(updater.GetRevokablePermissions(extension.get())
                    ->HasAPIPermission(APIPermissionID::kTab));

    // Remove the "tabs" permission. The extension should no longer have it
    // in its active or granted permissions, and it shouldn't be revokable.
    // The extension should still have the "cookies" permission.
    permissions_test_util::RevokeOptionalPermissionsAndWaitForCompletion(
        profile(), *extension, *api_permission_set(APIPermissionID::kTab),
        PermissionsUpdater::REMOVE_HARD);
    EXPECT_FALSE(permissions->HasAPIPermission(APIPermissionID::kTab));
    granted_permissions = prefs->GetGrantedPermissions(extension->id());
    EXPECT_FALSE(granted_permissions->HasAPIPermission(APIPermissionID::kTab));
    EXPECT_FALSE(updater.GetRevokablePermissions(extension.get())
                     ->HasAPIPermission(APIPermissionID::kTab));
    EXPECT_TRUE(permissions->HasAPIPermission(APIPermissionID::kCookie));
    granted_permissions = prefs->GetGrantedPermissions(extension->id());
    EXPECT_TRUE(
        granted_permissions->HasAPIPermission(APIPermissionID::kCookie));
    EXPECT_TRUE(updater.GetRevokablePermissions(extension.get())
                    ->HasAPIPermission(APIPermissionID::kCookie));
  }

  {
    // Make sure policy restriction updates update permission data.
    URLPatternSet default_policy_blocked_hosts;
    URLPatternSet default_policy_allowed_hosts;
    URLPatternSet policy_blocked_hosts;
    URLPatternSet policy_allowed_hosts;
    base::Value::List optional_permissions;
    base::Value::List required_permissions =
        base::Value::List().Append("tabs").Append("http://*/*");
    scoped_refptr<const Extension> extension =
        CreateExtensionWithOptionalPermissions(std::move(optional_permissions),
                                               std::move(required_permissions),
                                               "ExtensionSettings");
    AddPattern(&default_policy_blocked_hosts, "http://*.google.com/*");
    PermissionsUpdater updater(profile());
    updater.InitializePermissions(extension.get());
    PermissionsData::SetDefaultPolicyHostRestrictions(
        util::GetBrowserContextId(profile()), default_policy_blocked_hosts,
        default_policy_allowed_hosts);

    // By default, all subdomains of google.com should be blocked.
    const GURL kOrigin("http://foo.com");
    const GURL kGoogle("http://www.google.com");
    const GURL kExampleGoogle("http://example.google.com");
    EXPECT_TRUE(
        extension->permissions_data()->UsesDefaultPolicyHostRestrictions());
    EXPECT_TRUE(can_access_page(extension, kOrigin));
    EXPECT_FALSE(can_access_page(extension, kGoogle));
    EXPECT_FALSE(can_access_page(extension, kExampleGoogle));

    AddPattern(&default_policy_allowed_hosts, "http://example.google.com/*");
    // Give the extension access to example.google.com. Now the
    // example.google.com should not be a runtime blocked host.
    updater.SetDefaultPolicyHostRestrictions(default_policy_blocked_hosts,
                                             default_policy_allowed_hosts);

    EXPECT_TRUE(
        extension->permissions_data()->UsesDefaultPolicyHostRestrictions());
    EXPECT_TRUE(can_access_page(extension, kOrigin));
    EXPECT_FALSE(can_access_page(extension, kGoogle));
    EXPECT_TRUE(can_access_page(extension, kExampleGoogle));

    // Revoke extension access to foo.com. Now, foo.com should be a runtime
    // blocked host.
    AddPattern(&default_policy_blocked_hosts, "*://*.foo.com/");
    updater.SetDefaultPolicyHostRestrictions(default_policy_blocked_hosts,
                                             default_policy_allowed_hosts);
    EXPECT_TRUE(
        extension->permissions_data()->UsesDefaultPolicyHostRestrictions());
    EXPECT_FALSE(can_access_page(extension, kOrigin));
    EXPECT_FALSE(can_access_page(extension, kGoogle));
    EXPECT_TRUE(can_access_page(extension, kExampleGoogle));

    // Remove foo.com from blocked hosts. The extension should no longer have
    // be a runtime blocked host.
    default_policy_blocked_hosts.ClearPatterns();
    AddPattern(&default_policy_blocked_hosts, "*://*.foo.com/");
    updater.SetDefaultPolicyHostRestrictions(default_policy_blocked_hosts,
                                             default_policy_allowed_hosts);
    EXPECT_TRUE(
        extension->permissions_data()->UsesDefaultPolicyHostRestrictions());
    EXPECT_FALSE(can_access_page(extension, kOrigin));
    EXPECT_TRUE(can_access_page(extension, kGoogle));
    EXPECT_TRUE(can_access_page(extension, kExampleGoogle));

    // Set an empty individual policy, should not affect default policy.
    updater.SetPolicyHostRestrictions(extension.get(), policy_blocked_hosts,
                                      policy_allowed_hosts);
    EXPECT_FALSE(
        extension->permissions_data()->UsesDefaultPolicyHostRestrictions());
    EXPECT_TRUE(can_access_page(extension, kOrigin));
    EXPECT_TRUE(can_access_page(extension, kGoogle));
    EXPECT_TRUE(can_access_page(extension, kExampleGoogle));

    // Block google.com for the Individual scope.
    // Allowlist example.google.com for the Indiviaul scope.
    // Leave google.com and example.google.com off both the allowlist and
    // blocklist for Default scope.
    AddPattern(&policy_blocked_hosts, "*://*.google.com/*");
    AddPattern(&policy_allowed_hosts, "*://example.google.com/*");
    updater.SetPolicyHostRestrictions(extension.get(), policy_blocked_hosts,
                                      policy_allowed_hosts);
    EXPECT_FALSE(
        extension->permissions_data()->UsesDefaultPolicyHostRestrictions());
    EXPECT_TRUE(can_access_page(extension, kOrigin));
    EXPECT_FALSE(can_access_page(extension, kGoogle));
    EXPECT_TRUE(can_access_page(extension, kExampleGoogle));

    // Switch back to default scope for extension.
    updater.SetUsesDefaultHostRestrictions(extension.get());
    EXPECT_TRUE(
        extension->permissions_data()->UsesDefaultPolicyHostRestrictions());
    default_policy_blocked_hosts.ClearPatterns();
    default_policy_allowed_hosts.ClearPatterns();
    updater.SetDefaultPolicyHostRestrictions(default_policy_blocked_hosts,
                                             default_policy_allowed_hosts);
  }
}

TEST_F(PermissionsUpdaterTest,
       UpdatingRuntimeGrantedPermissionsWithOptionalPermissions) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddOptionalAPIPermission("tabs").Build();

  PermissionsUpdater updater(profile());
  updater.InitializePermissions(extension.get());
  // Grant the active permissions, as if the extension had just been installed.
  updater.GrantActivePermissions(extension.get());

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  // Initially, there should be no runtime-granted permissions or granted
  // permissions.
  EXPECT_TRUE(prefs->GetRuntimeGrantedPermissions(extension->id())->IsEmpty());
  EXPECT_TRUE(prefs->GetGrantedPermissions(extension->id())->IsEmpty());

  APIPermissionSet apis;
  apis.insert(APIPermissionID::kTab);
  PermissionSet optional_permissions(std::move(apis), ManifestPermissionSet(),
                                     URLPatternSet(), URLPatternSet());

  // Granting permissions should update both runtime-granted permissions and
  // granted permissions.
  permissions_test_util::GrantOptionalPermissionsAndWaitForCompletion(
      profile(), *extension, optional_permissions);
  EXPECT_EQ(optional_permissions,
            *prefs->GetRuntimeGrantedPermissions(extension->id()));
  EXPECT_EQ(optional_permissions,
            *prefs->GetGrantedPermissions(extension->id()));

  // Removing permissions with REMOVE_SOFT should not remove the permission
  // from runtime-granted permissions or granted permissions; this happens when
  // the extension opts into lower privilege.
  permissions_test_util::RevokeOptionalPermissionsAndWaitForCompletion(
      profile(), *extension, optional_permissions,
      PermissionsUpdater::REMOVE_SOFT);
  EXPECT_EQ(optional_permissions,
            *prefs->GetRuntimeGrantedPermissions(extension->id()));
  EXPECT_EQ(optional_permissions,
            *prefs->GetGrantedPermissions(extension->id()));

  // Removing permissions with REMOVE_HARD should remove the permission from
  // runtime-granted and granted permissions; this happens when the user chooses
  // to revoke the permission.
  // Note: we need to add back the permission first, so it shows up as a
  // revokable permission.
  // TODO(devlin): Inactive, but granted, permissions should be revokable.
  permissions_test_util::GrantOptionalPermissionsAndWaitForCompletion(
      profile(), *extension, optional_permissions);
  permissions_test_util::RevokeOptionalPermissionsAndWaitForCompletion(
      profile(), *extension, optional_permissions,
      PermissionsUpdater::REMOVE_HARD);
  EXPECT_TRUE(prefs->GetRuntimeGrantedPermissions(extension->id())->IsEmpty());
  EXPECT_TRUE(prefs->GetGrantedPermissions(extension->id())->IsEmpty());
}

TEST_F(PermissionsUpdaterTest,
       UpdatingRuntimeGrantedPermissionsWithRuntimePermissions) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddHostPermission("*://*/*").Build();

  PermissionsUpdater updater(profile());
  updater.InitializePermissions(extension.get());
  // Grant the active permissions, as if the extension had just been installed.
  updater.GrantActivePermissions(extension.get());
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  // Initially, there should be no runtime-granted permissions.
  EXPECT_TRUE(prefs->GetRuntimeGrantedPermissions(extension->id())->IsEmpty());
  std::unique_ptr<const PermissionSet> initial_granted_permissions =
      prefs->GetGrantedPermissions(extension->id());
  // Granted permissions should contain the required permissions from the
  // extension.
  EXPECT_TRUE(initial_granted_permissions->explicit_hosts().ContainsPattern(
      URLPattern(Extension::kValidHostPermissionSchemes, "*://*/*")));

  URLPatternSet explicit_hosts({URLPattern(
      Extension::kValidHostPermissionSchemes, "https://example.com/*")});
  PermissionSet runtime_granted_permissions(
      APIPermissionSet(), ManifestPermissionSet(), std::move(explicit_hosts),
      URLPatternSet());

  // Granting runtime-granted permissions should update the runtime granted
  // permissions store in preferences, but *not* granted permissions in
  // preferences.
  permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
      profile(), *extension, runtime_granted_permissions);
  EXPECT_EQ(runtime_granted_permissions,
            *prefs->GetRuntimeGrantedPermissions(extension->id()));
  EXPECT_EQ(*initial_granted_permissions,
            *prefs->GetGrantedPermissions(extension->id()));

  // Removing runtime-granted permissions should not remove the permission
  // from runtime-granted permissions; granted permissions should remain
  // unchanged.
  permissions_test_util::RevokeRuntimePermissionsAndWaitForCompletion(
      profile(), *extension, runtime_granted_permissions);

  EXPECT_TRUE(prefs->GetRuntimeGrantedPermissions(extension->id())->IsEmpty());
  EXPECT_EQ(*initial_granted_permissions,
            *prefs->GetGrantedPermissions(extension->id()));
}

TEST_F(PermissionsUpdaterTest, RevokingPermissionsWithRuntimeHostPermissions) {
  InitializeEmptyExtensionService();

  constexpr struct {
    const char* permission;
    const char* test_url;
  } test_cases[] = {
      {"http://*/*", "http://foo.com"},
      {"http://google.com/*", "http://google.com"},
  };

  for (const auto& test_case : test_cases) {
    std::string test_name =
        base::StringPrintf("%s, %s", test_case.permission, test_case.test_url);
    SCOPED_TRACE(test_name);
    scoped_refptr<const Extension> extension =
        CreateExtensionWithOptionalPermissions(
            base::Value::List(),
            base::Value::List().Append(test_case.permission), test_name);
    PermissionsUpdater updater(profile());
    updater.InitializePermissions(extension.get());

    ScriptingPermissionsModifier(profile(), extension)
        .SetWithholdHostPermissions(true);

    // Host access was withheld, so the extension shouldn't have access to the
    // test site.
    const GURL kOrigin(test_case.test_url);

    EXPECT_FALSE(extension->permissions_data()
                     ->active_permissions()
                     .HasExplicitAccessToOrigin(kOrigin));
    EXPECT_TRUE(updater.GetRevokablePermissions(extension.get())->IsEmpty());
    EXPECT_TRUE(extension->permissions_data()
                    ->withheld_permissions()
                    .HasExplicitAccessToOrigin(kOrigin));

    URLPatternSet url_pattern_set;
    url_pattern_set.AddOrigin(URLPattern::SCHEME_ALL, kOrigin);
    const PermissionSet permission_set(
        APIPermissionSet(), ManifestPermissionSet(), std::move(url_pattern_set),
        URLPatternSet());
    // Give the extension access to the test site. Now, the test site permission
    // should be revokable.
    permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
        profile(), *extension, permission_set);
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .HasExplicitAccessToOrigin(kOrigin));
    EXPECT_TRUE(updater.GetRevokablePermissions(extension.get())
                    ->HasExplicitAccessToOrigin(kOrigin));

    // Revoke the test site permission. The extension should no longer have
    // access to test site, and the revokable permissions should be empty.
    permissions_test_util::RevokeRuntimePermissionsAndWaitForCompletion(
        profile(), *extension, permission_set);
    EXPECT_FALSE(extension->permissions_data()
                     ->active_permissions()
                     .HasExplicitAccessToOrigin(kOrigin));
    EXPECT_TRUE(extension->permissions_data()
                    ->withheld_permissions()
                    .HasExplicitAccessToOrigin(kOrigin));
    EXPECT_TRUE(updater.GetRevokablePermissions(extension.get())->IsEmpty());
  }
}

TEST_F(PermissionsUpdaterTest, ChromeFaviconIsNotARevokableHost) {
  InitializeEmptyExtensionService();

  URLPattern chrome_favicon_pattern(Extension::kValidHostPermissionSchemes,
                                    "chrome://favicon/");

  {
    // This test exercises chrome://favicon, which is only available in MV2.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("favicon extension")
            .SetManifestVersion(2)
            .AddHostPermissions({"https://example.com/*", "chrome://favicon/*"})
            .Build();
    URLPattern example_com_pattern(Extension::kValidHostPermissionSchemes,
                                   "https://example.com/*");
    PermissionsUpdater updater(profile());
    updater.InitializePermissions(extension.get());

    // To start, the extension should have both example.com and chrome://favicon
    // permissions.
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .explicit_hosts()
                    .ContainsPattern(chrome_favicon_pattern));
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .explicit_hosts()
                    .ContainsPattern(example_com_pattern));

    // Only example.com should be revokable - chrome://favicon is not a real
    // host permission.
    std::unique_ptr<const PermissionSet> revokable_permissions =
        updater.GetRevokablePermissions(extension.get());
    EXPECT_FALSE(revokable_permissions->explicit_hosts().ContainsPattern(
        chrome_favicon_pattern));
    EXPECT_TRUE(revokable_permissions->explicit_hosts().ContainsPattern(
        example_com_pattern));

    // Withholding host permissions shouldn't withhold example.com.
    ScriptingPermissionsModifier(profile(), extension)
        .SetWithholdHostPermissions(true);
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .explicit_hosts()
                    .ContainsPattern(chrome_favicon_pattern));
    EXPECT_FALSE(extension->permissions_data()
                     ->active_permissions()
                     .explicit_hosts()
                     .ContainsPattern(example_com_pattern));
  }
  {
    // This test exercises chrome://favicon, which is only available in MV2.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("all urls extension")
            .SetManifestVersion(2)
            .AddHostPermission("<all_urls>")
            .Build();
    URLPattern all_urls_pattern(
        Extension::kValidHostPermissionSchemes &
            ~(URLPattern::SCHEME_CHROMEUI | URLPattern::SCHEME_FILE),
        "<all_urls>");
    PermissionsUpdater updater(profile());
    updater.InitializePermissions(extension.get());

    // <all_urls> (strangely) includes the chrome://favicon/ permission.
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .explicit_hosts()
                    .ContainsPattern(chrome_favicon_pattern));
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .explicit_hosts()
                    .ContainsPattern(all_urls_pattern));

    std::unique_ptr<const PermissionSet> revokable_permissions =
        updater.GetRevokablePermissions(extension.get());
    EXPECT_FALSE(revokable_permissions->explicit_hosts().ContainsPattern(
        chrome_favicon_pattern));
    EXPECT_TRUE(revokable_permissions->explicit_hosts().ContainsPattern(
        all_urls_pattern));

    ScriptingPermissionsModifier(profile(), extension)
        .SetWithholdHostPermissions(true);
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .explicit_hosts()
                    .ContainsPattern(chrome_favicon_pattern));
    EXPECT_FALSE(extension->permissions_data()
                     ->active_permissions()
                     .explicit_hosts()
                     .ContainsPattern(all_urls_pattern));
  }
}

// Tests runtime-granting permissions beyond what are explicitly requested by
// the extension.
TEST_F(PermissionsUpdaterTest, GrantingBroadRuntimePermissions) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddHostPermission("https://maps.google.com/*")
          .Build();

  const URLPattern kMapsPattern(Extension::kValidHostPermissionSchemes,
                                "https://maps.google.com/*");
  const URLPattern kAllGooglePattern(Extension::kValidHostPermissionSchemes,
                                     "https://*.google.com/*");

  // Withhold host permissions. Effective hosts should be empty.
  PermissionsUpdater updater(profile());
  updater.InitializePermissions(extension.get());
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);
  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  .effective_hosts()
                  .is_empty());

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  {
    // Verify initial state. The extension "active" permissions in preferences
    // represent the permissions that would be active on the extension without
    // the runtime host permissions feature. Thus, this should include the
    // requested host permissions, and nothing more.
    std::unique_ptr<const PermissionSet> active_prefs =
        prefs->GetDesiredActivePermissions(extension->id());
    EXPECT_TRUE(active_prefs->effective_hosts().ContainsPattern(kMapsPattern));
    EXPECT_FALSE(
        active_prefs->effective_hosts().ContainsPattern(kAllGooglePattern));

    // Runtime granted permissions should not contain any permissions (all
    // hosts are withheld).
    std::unique_ptr<const PermissionSet> runtime_granted_prefs =
        prefs->GetRuntimeGrantedPermissions(extension->id());
    EXPECT_FALSE(
        runtime_granted_prefs->effective_hosts().ContainsPattern(kMapsPattern));
    EXPECT_FALSE(runtime_granted_prefs->effective_hosts().ContainsPattern(
        kAllGooglePattern));
  }

  // Grant permission to all google.com domains.
  const PermissionSet runtime_permissions(
      APIPermissionSet(), ManifestPermissionSet(),
      URLPatternSet({kAllGooglePattern}), URLPatternSet());
  permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
      profile(), *extension, runtime_permissions);

  // The extension object's permission should never include un-requested
  // permissions, so it should only include maps.google.com.
  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  .effective_hosts()
                  .ContainsPattern(kMapsPattern));
  EXPECT_FALSE(extension->permissions_data()
                   ->active_permissions()
                   .effective_hosts()
                   .ContainsPattern(kAllGooglePattern));

  {
    // The active permissions in preferences should reflect the extension's
    // permission state without the runtime host permissions feature, so should
    // still include exactly the requested permissions.
    std::unique_ptr<const PermissionSet> active_prefs =
        prefs->GetDesiredActivePermissions(extension->id());
    EXPECT_TRUE(active_prefs->effective_hosts().ContainsPattern(kMapsPattern));
    EXPECT_FALSE(
        active_prefs->effective_hosts().ContainsPattern(kAllGooglePattern));
    // The runtime-granted permissions should include all permissions that have
    // been granted, which in this case includes google.com subdomains.
    std::unique_ptr<const PermissionSet> runtime_granted_prefs =
        prefs->GetRuntimeGrantedPermissions(extension->id());
    EXPECT_TRUE(
        runtime_granted_prefs->effective_hosts().ContainsPattern(kMapsPattern));
    EXPECT_TRUE(runtime_granted_prefs->effective_hosts().ContainsPattern(
        kAllGooglePattern));
  }

  // Revoke the host permission.
  permissions_test_util::RevokeRuntimePermissionsAndWaitForCompletion(
      profile(), *extension, runtime_permissions);

  EXPECT_FALSE(extension->permissions_data()
                   ->active_permissions()
                   .effective_hosts()
                   .ContainsPattern(kMapsPattern));

  {
    // Active permissions in the preferences should remain constant (unaffected
    // by the runtime host permissions feature).
    std::unique_ptr<const PermissionSet> active_prefs =
        prefs->GetDesiredActivePermissions(extension->id());
    EXPECT_TRUE(active_prefs->effective_hosts().ContainsPattern(kMapsPattern));
    EXPECT_FALSE(
        active_prefs->effective_hosts().ContainsPattern(kAllGooglePattern));
    // The runtime granted preferences should be empty again.
    std::unique_ptr<const PermissionSet> runtime_granted_prefs =
        prefs->GetRuntimeGrantedPermissions(extension->id());
    EXPECT_FALSE(
        runtime_granted_prefs->effective_hosts().ContainsPattern(kMapsPattern));
    EXPECT_FALSE(runtime_granted_prefs->effective_hosts().ContainsPattern(
        kAllGooglePattern));
  }
}

// Validates that we don't overwrite an extension's desired active permissions
// based on its current active permissions during an optional permissions grant.
// Regression test for https://crbug.com/1343643.
TEST_F(PermissionsUpdaterTest,
       DontOverwriteDesiredActivePermissionsOnOptionalPermissionsGrant) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      CreateExtensionWithOptionalPermissions(
          /*optional_permissions=*/base::Value::List().Append("tabs"),
          /*permissions=*/
          base::Value::List().Append("https://example.com/*"),
          "optional grant");
  ASSERT_TRUE(extension);

  {
    // Grant the active permissions, as if the extension had just been
    // installed.
    PermissionsUpdater updater(profile());
    updater.InitializePermissions(extension.get());
    updater.GrantActivePermissions(extension.get());
  }

  // Withhold host permissions. This shouldn't affect the extension's
  // desired active permissions.
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  GURL example_com("https://example.com");
  EXPECT_FALSE(extension->permissions_data()->HasHostPermission(example_com));
  EXPECT_TRUE(prefs->GetDesiredActivePermissions(extension->id())
                  ->effective_hosts()
                  .MatchesURL(example_com));

  {
    // Grant an optional permission.
    APIPermissionSet apis;
    apis.insert(APIPermissionID::kTab);
    permissions_test_util::GrantOptionalPermissionsAndWaitForCompletion(
        profile(), *extension,
        PermissionSet(std::move(apis), ManifestPermissionSet(), URLPatternSet(),
                      URLPatternSet()));
  }

  // Verify the desired active permissions. The extension should still have
  // example.com as a desired host.
  EXPECT_FALSE(extension->permissions_data()->HasHostPermission(example_com));
  EXPECT_TRUE(prefs->GetDesiredActivePermissions(extension->id())
                  ->effective_hosts()
                  .MatchesURL(example_com));
}

// Validates that we don't overwrite an extension's desired active permissions
// based on its initial effective active permissions on load (which could be
// different, in the case of withheld host permissions).
// Regression test for https://crbug.com/1343643.
TEST_F(PermissionsUpdaterTest,
       DontOverwriteDesiredActivePermissionsOnExtensionLoad) {
  InitializeEmptyExtensionService();

  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "manifest_version": 3,
           "version": "0.1",
           "host_permissions": ["<all_urls>"]
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);

  scoped_refptr<const Extension> extension =
      ChromeTestExtensionLoader(profile()).LoadExtension(
          test_dir.UnpackedPath());

  const ExtensionId id = extension->id();
  ASSERT_TRUE(registry()->enabled_extensions().Contains(id));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  // The extension's desired active permissions should include <all_urls>.
  EXPECT_TRUE(prefs->GetDesiredActivePermissions(id)
                  ->effective_hosts()
                  .MatchesAllURLs());

  // Withhold host permissions. This shouldn't affect the extension's desired
  // active permissions, which should still include <all_urls>.
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);
  EXPECT_TRUE(prefs->GetDesiredActivePermissions(extension->id())
                  ->effective_hosts()
                  .MatchesAllURLs());
  EXPECT_FALSE(extension->permissions_data()
                   ->active_permissions()
                   .effective_hosts()
                   .MatchesAllURLs());

  // Reload extensions.
  service()->ReloadExtensionsForTest();
  extension = registry()->enabled_extensions().GetByID(id);
  ASSERT_TRUE(extension);

  // The extension's desired active permissions should remain unchanged, and
  // should include <all_urls>.
  EXPECT_TRUE(prefs->GetDesiredActivePermissions(id)
                  ->effective_hosts()
                  .MatchesAllURLs());
  EXPECT_FALSE(extension->permissions_data()
                   ->active_permissions()
                   .effective_hosts()
                   .MatchesAllURLs());
}

// Validates that extension desired active permissions are restored to a sane
// state on extension load (including all required permissions).
TEST_F(PermissionsUpdaterTest, DesiredActivePermissionsAreFixedOnLoad) {
  InitializeEmptyExtensionService();

  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "manifest_version": 3,
           "version": "0.1",
           "permissions": ["tabs"],
           "host_permissions": ["https://requested.example/*"]
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);

  scoped_refptr<const Extension> extension =
      ChromeTestExtensionLoader(profile()).LoadExtension(
          test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const ExtensionId id = extension->id();
  ASSERT_TRUE(registry()->enabled_extensions().Contains(id));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  const GURL requested_url("https://requested.example");
  const GURL unrequested_url("https://unrequested.example");

  // The extension's desired active permissions should include example.com and
  // tabs.
  {
    std::unique_ptr<const PermissionSet> desired =
        prefs->GetDesiredActivePermissions(id);
    EXPECT_TRUE(desired->effective_hosts().MatchesURL(requested_url));
    EXPECT_FALSE(desired->effective_hosts().MatchesURL(unrequested_url));
    EXPECT_TRUE(desired->HasAPIPermission(APIPermissionID::kTab));
    EXPECT_FALSE(desired->HasAPIPermission(APIPermissionID::kBookmark));
  }

  // Mangle the desired permissions in prefs (a la pref corruption, bugs, etc).
  {
    APIPermissionSet apis;
    apis.insert(APIPermissionID::kBookmark);
    URLPatternSet patterns;
    patterns.AddOrigin(Extension::kValidHostPermissionSchemes, unrequested_url);
    prefs->SetDesiredActivePermissions(
        id, PermissionSet(std::move(apis), ManifestPermissionSet(),
                          std::move(patterns), URLPatternSet()));
  }

  // Reload extensions.
  service()->ReloadExtensionsForTest();
  extension = registry()->enabled_extensions().GetByID(id);
  ASSERT_TRUE(extension);

  // The extension's desired active permissions should have been restored to
  // their sane state of example.com and tabs.
  {
    std::unique_ptr<const PermissionSet> desired =
        prefs->GetDesiredActivePermissions(id);
    EXPECT_TRUE(desired->effective_hosts().MatchesURL(requested_url));
    EXPECT_FALSE(desired->effective_hosts().MatchesURL(unrequested_url));
    EXPECT_TRUE(desired->HasAPIPermission(APIPermissionID::kTab));
    EXPECT_FALSE(desired->HasAPIPermission(APIPermissionID::kBookmark));
  }
}

class PermissionsUpdaterTestWithEnhancedHostControls
    : public PermissionsUpdaterTest {
 public:
  PermissionsUpdaterTestWithEnhancedHostControls() {
    std::vector<base::test::FeatureRef> enabled_features = {
        extensions_features::kExtensionsMenuAccessControl,
        extensions_features::kExtensionsMenuAccessControlWithPermittedSites};
    std::vector<base::test::FeatureRef> disabled_features = {};
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  ~PermissionsUpdaterTestWithEnhancedHostControls() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests the behavior of revoking permissions from the extension while the
// user has specified a set of sites that all extensions are allowed to run on.
TEST_F(PermissionsUpdaterTestWithEnhancedHostControls,
       RevokingPermissionsWithUserPermittedSites) {
  InitializeEmptyExtensionService();

  // Install and initialize an extension that wants to run everywhere.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddHostPermission("<all_urls>").Build();

  {
    PermissionsUpdater updater(profile());
    updater.InitializePermissions(extension.get());
    updater.GrantActivePermissions(extension.get());
  }

  // Note that the PermissionsManger requires the extension to be in the
  // ExtensionRegistry, so add it through the ExtensionService.
  service()->AddExtension(extension.get());

  const GURL first_url("http://first.example");
  const GURL second_url("http://second.example");

  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  {
    // Simulate the user allowing all extensions to run on `first_url`.
    PermissionsManagerWaiter waiter(permissions_manager);
    permissions_manager->AddUserPermittedSite(url::Origin::Create(first_url));
    waiter.WaitForUserPermissionsSettingsChange();
  }

  auto get_site_access = [extension](const GURL& url) {
    return extension->permissions_data()->GetPageAccess(url, -1, nullptr);
  };

  auto has_desired_active_permission_for_url = [extension,
                                                prefs](const GURL& url) {
    return prefs->GetDesiredActivePermissions(extension->id())
        ->effective_hosts()
        .MatchesURL(url);
  };

  auto has_runtime_permission_for_url = [extension, prefs](const GURL& url) {
    return prefs->GetRuntimeGrantedPermissions(extension->id())
        ->effective_hosts()
        .MatchesURL(url);
  };

  auto has_granted_permission_for_url = [extension, prefs](const GURL& url) {
    return prefs->GetGrantedPermissions(extension->id())
        ->effective_hosts()
        .MatchesURL(url);
  };

  // By default, the extension should have permission to both sites, since it
  // has access to all URLs.
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed, get_site_access(first_url));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed, get_site_access(second_url));
  // The desired permission should include both, as well, as should the
  // granted.
  EXPECT_TRUE(has_desired_active_permission_for_url(first_url));
  EXPECT_TRUE(has_desired_active_permission_for_url(second_url));
  EXPECT_TRUE(has_granted_permission_for_url(first_url));
  EXPECT_TRUE(has_granted_permission_for_url(second_url));
  // The extension does not yet have any runtime granted permissions.
  EXPECT_FALSE(has_runtime_permission_for_url(first_url));
  EXPECT_FALSE(has_runtime_permission_for_url(second_url));

  // Withhold host permissions from the extension.
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  // The extension should be allowed to run on `first_url`, since the
  // user indicated all extensions can always run there. However, it should not
  // be allowed on `second_url`.
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed, get_site_access(first_url));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_site_access(second_url));
  // The desired permissions (indicating the extension's desired state) and
  // the granted permissions (indicating the install-time granted permissions)
  // should be unchanged, including both sites.
  EXPECT_TRUE(has_desired_active_permission_for_url(first_url));
  EXPECT_TRUE(has_desired_active_permission_for_url(second_url));
  EXPECT_TRUE(has_granted_permission_for_url(first_url));
  EXPECT_TRUE(has_granted_permission_for_url(second_url));
  // The runtime permissions should also be unchanged. Even though the extension
  // is allowed to run on `first_url`, it does not have runtime access to
  // that site (this is important if the user later removes the site from
  // permitted sites).
  EXPECT_FALSE(has_runtime_permission_for_url(first_url));
  EXPECT_FALSE(has_runtime_permission_for_url(second_url));

  // Now, grant the extension explicit access to `second_url`.
  ScriptingPermissionsModifier(profile(), extension)
      .GrantHostPermission(second_url);

  // The extension should now be allowed to run on both sites.
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed, get_site_access(first_url));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed, get_site_access(second_url));
  // Desired and granted permissions remain unchanged.
  EXPECT_TRUE(has_desired_active_permission_for_url(first_url));
  EXPECT_TRUE(has_desired_active_permission_for_url(second_url));
  EXPECT_TRUE(has_granted_permission_for_url(first_url));
  EXPECT_TRUE(has_granted_permission_for_url(second_url));
  // The extension should have runtime access for `second_url`, since it
  // was granted explicit access to it by the user.
  EXPECT_FALSE(has_runtime_permission_for_url(first_url));
  EXPECT_TRUE(has_runtime_permission_for_url(second_url));

  {
    // (Temporarily) add `second_url` as a user-permitted site.
    PermissionsManagerWaiter waiter(permissions_manager);
    permissions_manager->AddUserPermittedSite(url::Origin::Create(second_url));
    waiter.WaitForUserPermissionsSettingsChange();
  }

  // All sites should be accessible; permissions should be unchanged.
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed, get_site_access(first_url));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed, get_site_access(second_url));
  EXPECT_TRUE(has_desired_active_permission_for_url(first_url));
  EXPECT_TRUE(has_desired_active_permission_for_url(second_url));
  EXPECT_TRUE(has_granted_permission_for_url(first_url));
  EXPECT_TRUE(has_granted_permission_for_url(second_url));
  EXPECT_FALSE(has_runtime_permission_for_url(first_url));
  EXPECT_TRUE(has_runtime_permission_for_url(second_url));

  // Remove both sites from the permitted sites.
  {
    PermissionsManagerWaiter waiter(permissions_manager);
    permissions_manager->RemoveUserPermittedSite(
        url::Origin::Create(first_url));
    waiter.WaitForUserPermissionsSettingsChange();
  }
  {
    PermissionsManagerWaiter waiter(permissions_manager);
    permissions_manager->RemoveUserPermittedSite(
        url::Origin::Create(second_url));
    waiter.WaitForUserPermissionsSettingsChange();
  }

  // Now, `first_url` should be withheld, since it's no longer a permitted
  // site. However, `second_url` should still be accessible, because the
  // extension had explicit access to that site.
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld, get_site_access(first_url));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed, get_site_access(second_url));
  EXPECT_TRUE(has_desired_active_permission_for_url(first_url));
  EXPECT_TRUE(has_desired_active_permission_for_url(second_url));
  EXPECT_TRUE(has_granted_permission_for_url(first_url));
  EXPECT_TRUE(has_granted_permission_for_url(second_url));
  EXPECT_FALSE(has_runtime_permission_for_url(first_url));
  EXPECT_TRUE(has_runtime_permission_for_url(second_url));
}

}  // namespace extensions

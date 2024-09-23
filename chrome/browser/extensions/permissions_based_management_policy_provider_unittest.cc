// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions_based_management_policy_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/common/extensions/permissions/chrome_api_permissions.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/api_permission.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::APIPermissionID;

namespace extensions {

class PermissionsBasedManagementPolicyProviderTest : public testing::Test {
 public:
  typedef ExtensionManagementPrefUpdater<
      sync_preferences::TestingPrefServiceSyncable>
      PrefUpdater;

  PermissionsBasedManagementPolicyProviderTest()
      : profile_(std::make_unique<TestingProfile>()),
        pref_service_(profile_->GetTestingPrefService()),
        settings_(std::make_unique<ExtensionManagement>(profile_.get())),
        provider_(settings_.get()) {}

  void SetUp() override {}

  void TearDown() override {}

  // Get API permissions name for |id|, we cannot use arbitrary strings since
  // they will be ignored by ExtensionManagementService.
  std::string GetAPIPermissionName(APIPermissionID id) {
    for (const auto& perm : chrome_api_permissions::GetPermissionInfos()) {
      if (perm.id == id) {
        return perm.name;
      }
    }
    ADD_FAILURE() << "Permission not found: " << id;
    return std::string();
  }

  // Create an extension with specified |location|, |required_permissions| and
  // |optional_permissions|.
  scoped_refptr<const Extension> CreateExtensionWithPermission(
      mojom::ManifestLocation location,
      const base::Value::List* required_permissions,
      const base::Value::List* optional_permissions) {
    base::Value::Dict manifest_dict;
    manifest_dict.Set(manifest_keys::kName, "test");
    manifest_dict.Set(manifest_keys::kVersion, "0.1");
    manifest_dict.Set(manifest_keys::kManifestVersion, 2);
    if (required_permissions) {
      manifest_dict.Set(manifest_keys::kPermissions,
                        required_permissions->Clone());
    }
    if (optional_permissions) {
      manifest_dict.Set(manifest_keys::kOptionalPermissions,
                        optional_permissions->Clone());
    }
    std::string error;
    scoped_refptr<const Extension> extension = Extension::Create(
        base::FilePath(), location, manifest_dict, Extension::NO_FLAGS, &error);
    CHECK(extension.get()) << error;
    return extension;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<ExtensionManagement> settings_;

  PermissionsBasedManagementPolicyProvider provider_;
};

// Verifies that extensions with conflicting permissions cannot be loaded.
TEST_F(PermissionsBasedManagementPolicyProviderTest, APIPermissions) {
  // Prepares the extension manifest.
  base::Value::List required_permissions;
  required_permissions.Append(
      GetAPIPermissionName(APIPermissionID::kDownloads));
  required_permissions.Append(GetAPIPermissionName(APIPermissionID::kCookie));
  base::Value::List optional_permissions;
  optional_permissions.Append(GetAPIPermissionName(APIPermissionID::kProxy));

  scoped_refptr<const Extension> extension = CreateExtensionWithPermission(
      mojom::ManifestLocation::kExternalPolicyDownload, &required_permissions,
      &optional_permissions);

  std::u16string error16;
  // The extension should be allowed to be loaded by default.
  error16.clear();
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_TRUE(error16.empty());

  // Blocks kProxy by default. The test extension should still be allowed.
  {
    PrefUpdater pref(pref_service_.get());
    pref.AddBlockedPermission("*",
                              GetAPIPermissionName(APIPermissionID::kProxy));
  }
  error16.clear();
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_TRUE(error16.empty());

  // Blocks kCookie this time. The test extension should not be allowed now.
  {
    PrefUpdater pref(pref_service_.get());
    pref.AddBlockedPermission("*",
                              GetAPIPermissionName(APIPermissionID::kCookie));
  }
  error16.clear();
  EXPECT_FALSE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_FALSE(error16.empty());

  // Explictly allows kCookie for test extension. It should be allowed again.
  {
    PrefUpdater pref(pref_service_.get());
    pref.AddAllowedPermission(extension->id(),
                              GetAPIPermissionName(APIPermissionID::kCookie));
  }
  error16.clear();
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_TRUE(error16.empty());

  // Explictly blocks kCookie for test extension. It should still be allowed.
  {
    PrefUpdater pref(pref_service_.get());
    pref.AddBlockedPermission(extension->id(),
                              GetAPIPermissionName(APIPermissionID::kCookie));
  }
  error16.clear();
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_TRUE(error16.empty());

  // Any extension specific definition overrides all defaults, even if blank.
  {
    PrefUpdater pref(pref_service_.get());
    pref.UnsetBlockedPermissions(extension->id());
    pref.UnsetAllowedPermissions(extension->id());
    pref.ClearBlockedPermissions("*");
    pref.AddBlockedPermission(
        "*", GetAPIPermissionName(APIPermissionID::kDownloads));
  }
  error16.clear();
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_TRUE(error16.empty());

  // Blocks kDownloads by default. It should be blocked.
  {
    PrefUpdater pref(pref_service_.get());
    pref.UnsetPerExtensionSettings(extension->id());
    pref.UnsetPerExtensionSettings(extension->id());
    pref.ClearBlockedPermissions("*");
    pref.AddBlockedPermission(
        "*", GetAPIPermissionName(APIPermissionID::kDownloads));
  }
  error16.clear();
  EXPECT_FALSE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_FALSE(error16.empty());
  EXPECT_EQ("test (extension ID \"" + extension->id() +
                "\") is blocked by the administrator. ",
            base::UTF16ToASCII(error16));

  // Set custom error message to display to user when install blocked.
  const std::string blocked_install_message =
      "Visit https://example.com/exception";
  {
    PrefUpdater pref(pref_service_.get());
    pref.UnsetPerExtensionSettings(extension->id());
    pref.UnsetPerExtensionSettings(extension->id());
    pref.SetBlockedInstallMessage(extension->id(), blocked_install_message);
    pref.ClearBlockedPermissions("*");
    pref.AddBlockedPermission(
        extension->id(), GetAPIPermissionName(APIPermissionID::kDownloads));
  }
  error16.clear();
  EXPECT_FALSE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_FALSE(error16.empty());
  EXPECT_EQ("test (extension ID \"" + extension->id() +
                "\") is blocked by the administrator. " +
                blocked_install_message,
            base::UTF16ToASCII(error16));
}

}  // namespace extensions

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions_based_management_policy_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
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
#include "extensions/common/permissions/api_permission.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  std::string GetAPIPermissionName(APIPermission::ID id) {
    for (const auto& perm : chrome_api_permissions::GetPermissionInfos()) {
      if (perm.id == id)
        return perm.name;
    }
    ADD_FAILURE() << "Permission not found: " << id;
    return std::string();
  }

  // Create an extension with specified |location|, |required_permissions| and
  // |optional_permissions|.
  scoped_refptr<const Extension> CreateExtensionWithPermission(
      Manifest::Location location,
      const base::ListValue* required_permissions,
      const base::ListValue* optional_permissions) {
    base::DictionaryValue manifest_dict;
    manifest_dict.SetString(manifest_keys::kName, "test");
    manifest_dict.SetString(manifest_keys::kVersion, "0.1");
    manifest_dict.SetInteger(manifest_keys::kManifestVersion, 2);
    if (required_permissions) {
      manifest_dict.Set(manifest_keys::kPermissions,
                        required_permissions->CreateDeepCopy());
    }
    if (optional_permissions) {
      manifest_dict.Set(manifest_keys::kOptionalPermissions,
                        optional_permissions->CreateDeepCopy());
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
  sync_preferences::TestingPrefServiceSyncable* pref_service_;
  std::unique_ptr<ExtensionManagement> settings_;

  PermissionsBasedManagementPolicyProvider provider_;
};

// Verifies that extensions with conflicting permissions cannot be loaded.
TEST_F(PermissionsBasedManagementPolicyProviderTest, APIPermissions) {
  // Prepares the extension manifest.
  base::ListValue required_permissions;
  required_permissions.AppendString(
      GetAPIPermissionName(APIPermission::kDownloads));
  required_permissions.AppendString(
      GetAPIPermissionName(APIPermission::kCookie));
  base::ListValue optional_permissions;
  optional_permissions.AppendString(
      GetAPIPermissionName(APIPermission::kProxy));

  scoped_refptr<const Extension> extension =
      CreateExtensionWithPermission(Manifest::EXTERNAL_POLICY_DOWNLOAD,
                                    &required_permissions,
                                    &optional_permissions);

  base::string16 error16;
  // The extension should be allowed to be loaded by default.
  error16.clear();
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_TRUE(error16.empty());

  // Blocks kProxy by default. The test extension should still be allowed.
  {
    PrefUpdater pref(pref_service_);
    pref.AddBlockedPermission("*",
                              GetAPIPermissionName(APIPermission::kProxy));
  }
  error16.clear();
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_TRUE(error16.empty());

  // Blocks kCookie this time. The test extension should not be allowed now.
  {
    PrefUpdater pref(pref_service_);
    pref.AddBlockedPermission("*",
                              GetAPIPermissionName(APIPermission::kCookie));
  }
  error16.clear();
  EXPECT_FALSE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_FALSE(error16.empty());

  // Explictly allows kCookie for test extension. It should be allowed again.
  {
    PrefUpdater pref(pref_service_);
    pref.AddAllowedPermission(extension->id(),
                              GetAPIPermissionName(APIPermission::kCookie));
  }
  error16.clear();
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_TRUE(error16.empty());

  // Explictly blocks kCookie for test extension. It should still be allowed.
  {
    PrefUpdater pref(pref_service_);
    pref.AddBlockedPermission(extension->id(),
                              GetAPIPermissionName(APIPermission::kCookie));
  }
  error16.clear();
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_TRUE(error16.empty());

  // Any extension specific definition overrides all defaults, even if blank.
  {
    PrefUpdater pref(pref_service_);
    pref.UnsetBlockedPermissions(extension->id());
    pref.UnsetAllowedPermissions(extension->id());
    pref.ClearBlockedPermissions("*");
    pref.AddBlockedPermission("*",
                              GetAPIPermissionName(APIPermission::kDownloads));
  }
  error16.clear();
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_TRUE(error16.empty());

  // Blocks kDownloads by default. It should be blocked.
  {
    PrefUpdater pref(pref_service_);
    pref.UnsetPerExtensionSettings(extension->id());
    pref.UnsetPerExtensionSettings(extension->id());
    pref.ClearBlockedPermissions("*");
    pref.AddBlockedPermission("*",
                              GetAPIPermissionName(APIPermission::kDownloads));
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
    PrefUpdater pref(pref_service_);
    pref.UnsetPerExtensionSettings(extension->id());
    pref.UnsetPerExtensionSettings(extension->id());
    pref.SetBlockedInstallMessage(extension->id(), blocked_install_message);
    pref.ClearBlockedPermissions("*");
    pref.AddBlockedPermission(extension->id(),
                              GetAPIPermissionName(APIPermission::kDownloads));
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

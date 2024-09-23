// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/device_local_account_management_policy_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chromeos/ash/components/login/login_state/scoped_test_public_session_login_state.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;

namespace chromeos {

namespace {

scoped_refptr<const extensions::Extension> CreateExtensionFromValues(
    const std::string& id,
    ManifestLocation location,
    base::Value::Dict& values,
    int flags) {
  values.Set(extensions::manifest_keys::kName, "test");
  values.Set(extensions::manifest_keys::kVersion, "0.1");
  values.Set(extensions::manifest_keys::kManifestVersion, 2);
  std::string error;
  return extensions::Extension::Create(base::FilePath(), location, values,
                                       flags, id, &error);
}

scoped_refptr<const extensions::Extension> CreateRegularExtension(
    const std::string& id) {
  base::Value::Dict values;
  return CreateExtensionFromValues(id, ManifestLocation::kInternal, values,
                                   extensions::Extension::NO_FLAGS);
}

scoped_refptr<const extensions::Extension> CreateExternalComponentExtension() {
  base::Value::Dict values;
  return CreateExtensionFromValues(std::string(),
                                   ManifestLocation::kExternalComponent, values,
                                   extensions::Extension::NO_FLAGS);
}

scoped_refptr<const extensions::Extension> CreateComponentExtension() {
  base::Value::Dict values;
  return CreateExtensionFromValues(std::string(), ManifestLocation::kComponent,
                                   values, extensions::Extension::NO_FLAGS);
}

scoped_refptr<const extensions::Extension> CreatePlatformAppWithExtraValues(
    const base::Value::Dict& extra_values,
    ManifestLocation location,
    int flags) {
  base::Value::Dict values;
  values.SetByDottedPath("app.background.page", "background.html");
  values.Merge(extra_values.Clone());
  return CreateExtensionFromValues(std::string(), location, values, flags);
}

scoped_refptr<const extensions::Extension> CreatePlatformApp() {
  base::Value::Dict values;
  return CreatePlatformAppWithExtraValues(values, ManifestLocation::kInternal,
                                          extensions::Extension::NO_FLAGS);
}

}  // namespace

TEST(DeviceLocalAccountManagementPolicyProviderTest, PublicSession) {
  DeviceLocalAccountManagementPolicyProvider provider(
      policy::DeviceLocalAccountType::kPublicSession);
  // Set the login state to a public session.
  ash::ScopedTestPublicSessionLoginState login_state;

  // Verify that if an extension's location has been whitelisted for use in
  // public sessions, the extension can be installed.
  scoped_refptr<const extensions::Extension> extension =
      CreateExternalComponentExtension();
  ASSERT_TRUE(extension.get());
  std::u16string error;
  EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_EQ(std::u16string(), error);
  error.clear();

  extension = CreateComponentExtension();
  ASSERT_TRUE(extension.get());
  EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_EQ(std::u16string(), error);
  error.clear();

  // Verify that if neither the location, type nor the ID of an extension have
  // been whitelisted for use in public sessions, the extension cannot be
  // installed.
  extension = CreateRegularExtension(std::string());
  ASSERT_TRUE(extension.get());
  EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_NE(std::u16string(), error);
  error.clear();

  // Verify that a minimal platform app can be installed from location
  // EXTERNAL_POLICY.
  {
    base::Value::Dict values;
    extension = CreatePlatformAppWithExtraValues(
        values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a minimal platform app can be installed from location
  // EXTERNAL_POLICY_DOWNLOAD.
  {
    base::Value::Dict values;
    extension = CreatePlatformAppWithExtraValues(
        values, ManifestLocation::kExternalPolicyDownload,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a minimal platform app cannot be installed from location
  // UNPACKED.
  {
    base::Value::Dict values;
    extension = CreatePlatformAppWithExtraValues(
        values, ManifestLocation::kUnpacked, extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with all safe manifest entries can be installed.
  {
    base::Value::Dict values;
    values.Set(extensions::manifest_keys::kDescription, "something");
    values.Set(extensions::manifest_keys::kShortName, "something else");
    base::Value::List permissions;
    permissions.Append("alarms");
    permissions.Append("background");
    values.Set(extensions::manifest_keys::kPermissions, std::move(permissions));
    base::Value::List optional_permissions;
    optional_permissions.Append("alarms");
    optional_permissions.Append("background");
    values.Set(extensions::manifest_keys::kOptionalPermissions,
               std::move(optional_permissions));
    extension = CreatePlatformAppWithExtraValues(
        values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with a safe manifest entry under "app" can be
  // installed.
  {
    base::Value::Dict values;
    values.SetByDottedPath("app.content_security_policy", "something2");
    extension = CreatePlatformAppWithExtraValues(
        values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a hosted app with a safe manifest entry under "app" can be
  // installed.
  {
    base::Value::Dict values;
    values.Set(extensions::manifest_keys::kApp, base::Value::Dict());
    values.SetByDottedPath(extensions::manifest_keys::kWebURLs,
                           base::Value::List());
    values.SetByDottedPath("app.content_security_policy", "something2");
    extension = CreateExtensionFromValues(
        std::string(), ManifestLocation::kExternalPolicy, values,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with a url_handlers manifest entry and which is
  // installed through the web store can be installed.
  {
    base::Value::List matches;
    matches.Append("https://example.com/*");
    base::Value::Dict values;
    values.SetByDottedPath("url_handlers.example_com.matches",
                           std::move(matches));
    values.SetByDottedPath("url_handlers.example_com.title", "example title");

    extension = CreatePlatformAppWithExtraValues(
        values, ManifestLocation::kExternalPolicy,
        extensions::Extension::FROM_WEBSTORE);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with remote URL permissions can be installed.
  {
    base::Value::List permissions;
    permissions.Append("https://example.com/");
    permissions.Append("http://example.com/");
    permissions.Append("ftp://example.com/");
    base::Value::Dict values;
    values.Set(extensions::manifest_keys::kPermissions, std::move(permissions));

    extension = CreatePlatformAppWithExtraValues(
        values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with socket dictionary permission can be
  // installed.
  {
    base::Value::Dict socket;
    base::Value::List tcp_list;
    tcp_list.Append("tcp-connect");
    socket.Set("socket", std::move(tcp_list));
    base::Value::List permissions;
    permissions.Append(std::move(socket));
    base::Value::Dict values;
    values.Set(extensions::manifest_keys::kPermissions, std::move(permissions));

    extension = CreatePlatformAppWithExtraValues(
        values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that an extension can be installed.
  {
    base::Value::Dict values;
    extension = CreateExtensionFromValues(
        std::string(), ManifestLocation::kExternalPolicy, values,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a shared_module can be installed.
  {
    base::Value::Dict values;
    values.SetByDottedPath("export.whitelist", base::Value::List());  // nocheck
    extension = CreateExtensionFromValues(
        std::string(), ManifestLocation::kExternalPolicy, values,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a theme can be installed.
  {
    base::Value::Dict values;
    values.Set("theme", base::Value::Dict());
    extension = CreateExtensionFromValues(
        std::string(), ManifestLocation::kExternalPolicy, values,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }
}

TEST(DeviceLocalAccountManagementPolicyProviderTest, KioskAppSessions) {
  static constexpr policy::DeviceLocalAccountType kTypes[] = {
      policy::DeviceLocalAccountType::kKioskApp,
      policy::DeviceLocalAccountType::kWebKioskApp,
  };

  for (auto type : kTypes) {
    LOG(INFO) << "Testing device local account type = "<< static_cast<int>(type);
    DeviceLocalAccountManagementPolicyProvider provider(type);

    // Verify that a platform app can be installed.
    scoped_refptr<const extensions::Extension> extension = CreatePlatformApp();
    ASSERT_TRUE(extension.get());
    std::u16string error;
    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();

    // Verify that an extension whose location has been whitelisted for use in
    // other types of device-local accounts cannot be installed in a single-app
    // kiosk session.
    extension = CreateExternalComponentExtension();
    ASSERT_TRUE(extension.get());
    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();

    extension = CreateComponentExtension();
    ASSERT_TRUE(extension.get());
    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }
}

}  // namespace chromeos

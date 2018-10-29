// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/permissions_updater_delegate_chromeos.h"

#include <string>

#include "base/files/file_path.h"
#include "base/values.h"
#include "chromeos/login/login_state.h"
#include "chromeos/login/scoped_test_public_session_login_state.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/permissions/mock_manifest_permission.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kWhitelistedId[] = "cbkkbcmdlboombapidmoeolnmdacpkch";
const char kBogusId[] = "bogus";

scoped_refptr<const Extension> CreateExtension(const std::string& id) {
  return ExtensionBuilder("test")
      .SetLocation(Manifest::INTERNAL)
      .SetID(id)
      .Build();
}

std::unique_ptr<const PermissionSet> CreatePermissions(
    bool include_clipboard = true) {
  APIPermissionSet apis;
  apis.insert(APIPermission::kAudio);
  apis.insert(APIPermission::kFullscreen);
  if (include_clipboard)
    apis.insert(APIPermission::kClipboardRead);
  ManifestPermissionSet manifest;
  manifest.insert(new MockManifestPermission("author"));
  manifest.insert(new MockManifestPermission("background"));
  URLPatternSet explicit_hosts({
      URLPattern(URLPattern::SCHEME_ALL, "http://www.google.com/*"),
      URLPattern(URLPattern::SCHEME_ALL, "<all_urls>")});
  URLPatternSet scriptable_hosts({
    URLPattern(URLPattern::SCHEME_ALL, "http://www.wikipedia.com/*")});
  auto permissions = std::make_unique<const PermissionSet>(
      apis, manifest, explicit_hosts, scriptable_hosts);
  return permissions;
}

}  // namespace

TEST(PermissionsUpdaterDelegateChromeOSTest, NoFilteringOutsidePublicSession) {
  PermissionsUpdaterDelegateChromeOS delegate;
  ASSERT_FALSE(chromeos::LoginState::IsInitialized());

  // Whitelisted extension outside PS, nothing filtered.
  auto extension = CreateExtension(kWhitelistedId);
  auto granted_permissions = CreatePermissions();
  delegate.InitializePermissions(extension.get(), &granted_permissions);
  EXPECT_EQ(*CreatePermissions(), *granted_permissions);

  // Bogus extension ID (never whitelisted) outside PS, nothing filtered.
  extension = CreateExtension(kBogusId);
  granted_permissions = CreatePermissions();
  delegate.InitializePermissions(extension.get(), &granted_permissions);
  EXPECT_EQ(*CreatePermissions(), *granted_permissions);
}

TEST(PermissionsUpdaterDelegateChromeOSTest,
     FilterNonWhitelistedInsidePublicSession) {
  chromeos::ScopedTestPublicSessionLoginState login_state;
  PermissionsUpdaterDelegateChromeOS delegate;

  // Whitelisted extension, nothing gets filtered.
  auto extension = CreateExtension(kWhitelistedId);
  auto granted_permissions = CreatePermissions();
  delegate.InitializePermissions(extension.get(), &granted_permissions);
  EXPECT_EQ(*CreatePermissions(), *granted_permissions);

  // Bogus extension ID (never whitelisted), ClipboardRead filtered out,
  // everything else stays.
  extension = CreateExtension(kBogusId);
  granted_permissions = CreatePermissions();
  delegate.InitializePermissions(extension.get(), &granted_permissions);
  EXPECT_EQ(*CreatePermissions(false), *granted_permissions);
}

}  // namespace extensions

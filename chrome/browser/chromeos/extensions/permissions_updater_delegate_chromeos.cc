// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/permissions_updater_delegate_chromeos.h"

#include "chrome/browser/chromeos/extensions/device_local_account_management_policy_provider.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/url_pattern_set.h"

namespace extensions {

PermissionsUpdaterDelegateChromeOS::PermissionsUpdaterDelegateChromeOS() {}

PermissionsUpdaterDelegateChromeOS::~PermissionsUpdaterDelegateChromeOS() {}

void PermissionsUpdaterDelegateChromeOS::InitializePermissions(
    const Extension* extension,
    std::unique_ptr<const PermissionSet>* granted_permissions) {
  if (!profiles::ArePublicSessionRestrictionsEnabled() ||
      chromeos::DeviceLocalAccountManagementPolicyProvider::IsWhitelisted(
          extension->id()) ||
      !(*granted_permissions)
           ->HasAPIPermission(mojom::APIPermissionID::kClipboardRead)) {
    return;
  }
  // Revoke kClipboardRead permission (used in Public Sessions to secure
  // clipboard read functionality). This forceful removal of permission is safe
  // since the clipboard pasting code checks for this permission before doing
  // the paste (the end result is just an empty paste).
  APIPermissionSet api_permission_set;
  api_permission_set.insert(mojom::APIPermissionID::kClipboardRead);
  *granted_permissions = PermissionSet::CreateDifference(
      **granted_permissions,
      PermissionSet(std::move(api_permission_set), ManifestPermissionSet(),
                    URLPatternSet(), URLPatternSet()));
}

}  // namespace extensions

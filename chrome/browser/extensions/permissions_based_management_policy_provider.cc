// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions_based_management_policy_provider.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_management.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

PermissionsBasedManagementPolicyProvider::
    PermissionsBasedManagementPolicyProvider(ExtensionManagement* settings)
    : settings_(settings) {}

PermissionsBasedManagementPolicyProvider::
    ~PermissionsBasedManagementPolicyProvider() {}

std::string
PermissionsBasedManagementPolicyProvider::GetDebugPolicyProviderName() const {
#ifdef NDEBUG
  NOTREACHED_IN_MIGRATION();
  return std::string();
#else
  return "Controlled by enterprise policy, restricting extension permissions.";
#endif
}

bool PermissionsBasedManagementPolicyProvider::UserMayLoad(
    const Extension* extension,
    std::u16string* error) const {
  // Component extensions are always allowed.
  if (Manifest::IsComponentLocation(extension->location())) {
    return true;
  }

  if (!settings_->IsPermissionSetAllowed(
          extension, PermissionsParser::GetRequiredPermissions(extension))) {
    if (error) {
      *error = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_CANT_INSTALL_POLICY_BLOCKED,
          base::UTF8ToUTF16(extension->name()),
          base::UTF8ToUTF16(extension->id()),
          base::UTF8ToUTF16(settings_->BlockedInstallMessage(extension->id())));
    }
    return false;
  }

  return true;
}

}  // namespace extensions

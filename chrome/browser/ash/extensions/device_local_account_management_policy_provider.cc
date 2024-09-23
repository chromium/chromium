// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/device_local_account_management_policy_provider.h"

#include "base/dcheck_is_on.h"
#include "base/immediate_crash.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/extensions/extensions_permissions_tracker.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::mojom::ManifestLocation;

namespace chromeos {

DeviceLocalAccountManagementPolicyProvider::
    DeviceLocalAccountManagementPolicyProvider(
        policy::DeviceLocalAccountType account_type)
    : account_type_(account_type) {}

DeviceLocalAccountManagementPolicyProvider::
    ~DeviceLocalAccountManagementPolicyProvider() = default;

std::string
DeviceLocalAccountManagementPolicyProvider::GetDebugPolicyProviderName() const {
#if DCHECK_IS_ON()
  return "allowlist for device-local accounts";
#else
  base::ImmediateCrash();
#endif
}

bool DeviceLocalAccountManagementPolicyProvider::UserMayLoad(
    const extensions::Extension* extension,
    std::u16string* error) const {
  switch (account_type_) {
    case policy::DeviceLocalAccountType::kPublicSession:
    case policy::DeviceLocalAccountType::kSamlPublicSession:
      // For Managed Guest Sessions, allow component & force-installed
      // extensions.
      if (extension->location() == ManifestLocation::kExternalComponent ||
          extension->location() == ManifestLocation::kComponent ||
          extension->location() == ManifestLocation::kExternalPolicyDownload ||
          extension->location() == ManifestLocation::kExternalPolicy) {
        return true;
      }

      // Allow extension IDs in the MGS allowlist.
      if (extensions::IsAllowlistedForManagedGuestSession(extension->id())) {
        return true;
      }
      break;
    case policy::DeviceLocalAccountType::kKioskApp:
    case policy::DeviceLocalAccountType::kWebKioskApp:
    case policy::DeviceLocalAccountType::kKioskIsolatedWebApp:
      // For single-app kiosk sessions, allow platform apps, extensions and
      // shared modules.
      if (extension->GetType() == extensions::Manifest::TYPE_PLATFORM_APP ||
          extension->GetType() == extensions::Manifest::TYPE_SHARED_MODULE ||
          extension->GetType() == extensions::Manifest::TYPE_EXTENSION) {
        return true;
      }
      break;
  }

  // Disallow all other extensions.
  if (error) {
    *error = l10n_util::GetStringFUTF16(
        IDS_EXTENSION_CANT_INSTALL_IN_DEVICE_LOCAL_ACCOUNT,
        base::UTF8ToUTF16(extension->name()),
        base::UTF8ToUTF16(extension->id()));
  }
  return false;
}

}  // namespace chromeos

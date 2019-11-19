// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/standard_management_policy_provider.h"

#include <string>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_management.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// Returns whether the extension can be modified under admin policy or not, and
// fills |error| with corresponding error message if necessary.
bool AdminPolicyIsModifiable(const Extension* source_extension,
                             const Extension* extension,
                             base::string16* error) {
  // Component and force installed extensions can enable/disable all other
  // extensions including force installed ones (but component are off limits).
  const bool component_or_force_installed =
      source_extension &&
      (Manifest::IsComponentLocation(source_extension->location()) ||
       Manifest::IsPolicyLocation(source_extension->location()));

  bool is_modifiable = true;

  if (Manifest::IsComponentLocation(extension->location()))
    is_modifiable = false;
  if (!component_or_force_installed &&
      Manifest::IsPolicyLocation(extension->location())) {
    is_modifiable = false;
  }

  if (is_modifiable)
    return true;

  if (error) {
    *error = l10n_util::GetStringFUTF16(
        IDS_EXTENSION_CANT_MODIFY_POLICY_REQUIRED,
        base::UTF8ToUTF16(extension->name()));
  }

  return false;
}

}  // namespace

StandardManagementPolicyProvider::StandardManagementPolicyProvider(
    const ExtensionManagement* settings)
    : settings_(settings) {
}

StandardManagementPolicyProvider::~StandardManagementPolicyProvider() {
}

std::string
    StandardManagementPolicyProvider::GetDebugPolicyProviderName() const {
#if DCHECK_IS_ON()
  return "extension management policy controlled settings";
#else
  IMMEDIATE_CRASH();
#endif
}

bool StandardManagementPolicyProvider::UserMayLoad(
    const Extension* extension,
    base::string16* error) const {
  // Component extensions are always allowed, besides the camera app that can be
  // disabled by extension policy. This is a temporary solution until there's a
  // dedicated policy to disable the camera, at which point the special check in
  // the 'if' statement should be removed.
  // TODO(http://crbug.com/1002935)
  if (Manifest::IsComponentLocation(extension->location()) &&
      extension->id() != extension_misc::kCameraAppId) {
    return true;
  }

  // Shared modules are always allowed too: they only contain resources that
  // are used by other extensions. The extension that depends on the shared
  // module may be filtered by policy.
  if (extension->is_shared_module())
    return true;

  // Always allow bookmark apps. The fact that bookmark apps are an extension is
  // an internal implementation detail and hence they should not be controlled
  // by extension management policies. See crbug.com/786061.
  // TODO(calamity): This special case should be removed by removing bookmark
  // apps from external sources. See crbug.com/788245.
  if (extension->from_bookmark())
    return true;

  // Check whether the extension type is allowed.
  //
  // If you get a compile error here saying that the type you added is not
  // handled by the switch statement below, please consider whether enterprise
  // policy should be able to disallow extensions of the new type. If so, add
  // a branch to the second block and add a line to the definition of
  // kAllowedTypesMap in extension_management_constants.h.
  switch (extension->GetType()) {
    case Manifest::TYPE_UNKNOWN:
      break;
    case Manifest::TYPE_EXTENSION:
    case Manifest::TYPE_THEME:
    case Manifest::TYPE_USER_SCRIPT:
    case Manifest::TYPE_HOSTED_APP:
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
    case Manifest::TYPE_PLATFORM_APP:
    case Manifest::TYPE_SHARED_MODULE:
    case Manifest::TYPE_LOGIN_SCREEN_EXTENSION: {
      if (!settings_->IsAllowedManifestType(extension->GetType(),
                                            extension->id()))
        return ReturnLoadError(extension, error);
      break;
    }
    case Manifest::NUM_LOAD_TYPES:
      NOTREACHED();
  }

  ExtensionManagement::InstallationMode installation_mode =
      settings_->GetInstallationMode(extension);
  if (installation_mode == ExtensionManagement::INSTALLATION_BLOCKED ||
      installation_mode == ExtensionManagement::INSTALLATION_REMOVED) {
    return ReturnLoadError(extension, error);
  }

  return true;
}

bool StandardManagementPolicyProvider::UserMayInstall(
    const Extension* extension,
    base::string16* error) const {
  ExtensionManagement::InstallationMode installation_mode =
      settings_->GetInstallationMode(extension);

  // Force-installed extensions cannot be overwritten manually.
  if (!Manifest::IsPolicyLocation(extension->location()) &&
      installation_mode == ExtensionManagement::INSTALLATION_FORCED) {
    return ReturnLoadError(extension, error);
  }

  return UserMayLoad(extension, error);
}

bool StandardManagementPolicyProvider::UserMayModifySettings(
    const Extension* extension,
    base::string16* error) const {
  return AdminPolicyIsModifiable(nullptr, extension, error);
}

bool StandardManagementPolicyProvider::ExtensionMayModifySettings(
    const Extension* source_extension,
    const Extension* extension,
    base::string16* error) const {
  return AdminPolicyIsModifiable(source_extension, extension, error);
}

bool StandardManagementPolicyProvider::MustRemainEnabled(
    const Extension* extension,
    base::string16* error) const {
  return !AdminPolicyIsModifiable(nullptr, extension, error);
}

bool StandardManagementPolicyProvider::MustRemainDisabled(
    const Extension* extension,
    disable_reason::DisableReason* reason,
    base::string16* error) const {
  std::string required_version;
  if (!settings_->CheckMinimumVersion(extension, &required_version)) {
    if (reason)
      *reason = disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY;
    if (error) {
      *error = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_DISABLED_UPDATE_REQUIRED_BY_POLICY,
          base::UTF8ToUTF16(extension->name()),
          base::ASCIIToUTF16(required_version));
    }
    return true;
  }
  return false;
}

bool StandardManagementPolicyProvider::MustRemainInstalled(
    const Extension* extension,
    base::string16* error) const {
  ExtensionManagement::InstallationMode mode =
      settings_->GetInstallationMode(extension);
  // Disallow removing of recommended extension, to avoid re-install it
  // again while policy is reload. But disabling of recommended extension is
  // allowed.
  if (mode == ExtensionManagement::INSTALLATION_FORCED ||
      mode == ExtensionManagement::INSTALLATION_RECOMMENDED) {
    if (error) {
      *error = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_CANT_UNINSTALL_POLICY_REQUIRED,
          base::UTF8ToUTF16(extension->name()));
    }
    return true;
  }
  return false;
}

bool StandardManagementPolicyProvider::ShouldForceUninstall(
    const Extension* extension,
    base::string16* error) const {
  if (UserMayLoad(extension, error))
    return false;
  if (settings_->GetInstallationMode(extension) ==
      ExtensionManagement::INSTALLATION_REMOVED) {
    return true;
  }
  return false;
}

bool StandardManagementPolicyProvider::ReturnLoadError(
    const extensions::Extension* extension,
    base::string16* error) const {
  if (error) {
    *error = l10n_util::GetStringFUTF16(
        IDS_EXTENSION_CANT_INSTALL_POLICY_BLOCKED,
        base::UTF8ToUTF16(extension->name()),
        base::UTF8ToUTF16(extension->id()),
        base::UTF8ToUTF16(settings_->BlockedInstallMessage(extension->id())));
  }
  return false;
}

}  // namespace extensions

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/standard_management_policy_provider.h"

#include <string>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/grit/generated_resources.h"
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
                             std::u16string* error) {
  // Component and force installed extensions can enable/disable all other
  // extensions including force installed ones (but component are off limits).
  const bool component_or_force_installed =
      source_extension &&
      (Manifest::IsComponentLocation(source_extension->location()) ||
       Manifest::IsPolicyLocation(source_extension->location()));

  // We also specifically disallow the Webstore to modify force installed
  // extensions even though it is a component extension, because it doesn't
  // need this capability and it can open up interesting attacks if it's
  // leveraged via bookmarklets or devtools.
  // TODO(crbug.com/40239460): This protection should be expanded by also
  // blocking bookmarklets on the Webstore Origin through checks on the Blink
  // side.
  const bool is_webstore_hosted_app =
      source_extension && source_extension->id() == extensions::kWebStoreAppId;

  bool is_modifiable = true;

  if (Manifest::IsComponentLocation(extension->location()))
    is_modifiable = false;
  if ((!component_or_force_installed || is_webstore_hosted_app) &&
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
    ExtensionManagement* settings,
    Profile* profile)
    : profile_(profile), settings_(settings) {}

StandardManagementPolicyProvider::~StandardManagementPolicyProvider() {
}

std::string
    StandardManagementPolicyProvider::GetDebugPolicyProviderName() const {
#if DCHECK_IS_ON()
  return "extension management policy controlled settings";
#else
  base::ImmediateCrash();
#endif
}

bool StandardManagementPolicyProvider::UserMayLoad(
    const Extension* extension,
    std::u16string* error) const {
  if (Manifest::IsComponentLocation(extension->location())) {
    return true;
  }

  // Shared modules are always allowed too: they only contain resources that
  // are used by other extensions. The extension that depends on the shared
  // module may be filtered by policy.
  if (extension->is_shared_module())
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
    case Manifest::TYPE_LOGIN_SCREEN_EXTENSION:
    case Manifest::TYPE_CHROMEOS_SYSTEM_EXTENSION: {
      if (!settings_->IsAllowedManifestType(extension->GetType(),
                                            extension->id()))
        return ReturnLoadError(extension, error);
      break;
    }
    case Manifest::NUM_LOAD_TYPES:
      NOTREACHED_IN_MIGRATION();
  }

  ExtensionManagement::InstallationMode installation_mode =
      settings_->GetInstallationMode(extension);
  if (installation_mode == ExtensionManagement::INSTALLATION_BLOCKED ||
      installation_mode == ExtensionManagement::INSTALLATION_REMOVED) {
    return ReturnLoadError(extension, error);
  }

  if (!settings_->IsAllowedManifestVersion(extension)) {
    if (error) {
      *error = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_MANIFEST_VERSION_NOT_SUPPORTED,
          base::UTF8ToUTF16(extension->name()));
    }
    return false;
  }

  return true;
}

bool StandardManagementPolicyProvider::UserMayInstall(
    const Extension* extension,
    std::u16string* error) const {
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
    std::u16string* error) const {
  return AdminPolicyIsModifiable(nullptr, extension, error);
}

bool StandardManagementPolicyProvider::ExtensionMayModifySettings(
    const Extension* source_extension,
    const Extension* extension,
    std::u16string* error) const {
  return AdminPolicyIsModifiable(source_extension, extension, error);
}

bool StandardManagementPolicyProvider::MustRemainEnabled(
    const Extension* extension,
    std::u16string* error) const {
  return !AdminPolicyIsModifiable(nullptr, extension, error);
}

bool StandardManagementPolicyProvider::MustRemainDisabled(
    const Extension* extension,
    disable_reason::DisableReason* reason,
    std::u16string* error) const {
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

  if (!settings_->IsAllowedByUnpublishedAvailabilityPolicy(extension)) {
    if (reason) {
      *reason = disable_reason::DISABLE_PUBLISHED_IN_STORE_REQUIRED_BY_POLICY;
    }
    if (error) {
      *error = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_DISABLED_PUBLISHED_IN_STORE_REQUIRED_BY_POLICY,
          base::UTF8ToUTF16(extension->name()));
    }
    return true;
  }

  if (!settings_->IsAllowedByUnpackedDeveloperModePolicy(*extension)) {
    if (reason) {
      *reason = disable_reason::DISABLE_UNSUPPORTED_DEVELOPER_EXTENSION;
    }
    if (error) {
      // TODO(crbug.com/362756477): Replace temporary string with disable
      // unsupported developer string once ready.
      *error = u"Unpacked extension blocked by developer mode requirement.";
    }
    return true;
  }

  if (settings_->ShouldBlockForceInstalledOffstoreExtension(*extension)) {
    if (reason) {
      *reason = disable_reason::DISABLE_NOT_VERIFIED;
    }
    if (error) {
      *error = l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_ADDED_WITHOUT_KNOWLEDGE,
          l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE));
    }
    return true;
  }

  return false;
}

bool StandardManagementPolicyProvider::MustRemainInstalled(
    const Extension* extension,
    std::u16string* error) const {
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
    std::u16string* error) const {
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
    std::u16string* error) const {
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

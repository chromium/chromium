// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webstore_private/extension_install_status.h"

#include "base/containers/contains.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permission_set.h"

namespace extensions {
namespace {

// A helper function to determine if an extension from web store with given
// information should be blocked by enterprise policy. It checks extension's
// installation mode, permission and manifest type.
// Returns true if the extension |mode| is blocked, removed or allowed by
// wildcard/update_url but blocked by |manifest type| or |required permissions|.
bool IsExtensionInstallBlockedByPolicy(
    ExtensionManagement* extension_management,
    ExtensionManagement::InstallationMode mode,
    const ExtensionId& extension_id,
    const std::string& update_url,
    Manifest::Type manifest_type,
    const PermissionSet& required_permissions) {
  switch (mode) {
    case ExtensionManagement::INSTALLATION_BLOCKED:
    case ExtensionManagement::INSTALLATION_REMOVED:
      return true;
    case ExtensionManagement::INSTALLATION_FORCED:
    case ExtensionManagement::INSTALLATION_RECOMMENDED:
      return false;
    case ExtensionManagement::INSTALLATION_ALLOWED:
      break;
  }

  if (extension_management->IsInstallationExplicitlyAllowed(extension_id)) {
    return false;
  }

  // Extension is allowed by wildcard or update_url, checks required permissions
  // and manifest type.
  // TODO(crbug.com/40133205): Find out the right way to handle extension policy
  // priority.
  if (manifest_type != Manifest::Type::TYPE_UNKNOWN &&
      !extension_management->IsAllowedManifestType(manifest_type,
                                                   extension_id)) {
    return true;
  }

  if (!extension_management->IsPermissionSetAllowed(extension_id, update_url,
                                                    required_permissions)) {
    return true;
  }

  return false;
}

}  // namespace

ExtensionInstallStatus GetWebstoreExtensionInstallStatus(
    const ExtensionId& extension_id,
    Profile* profile) {
  return GetWebstoreExtensionInstallStatus(
      extension_id, profile, Manifest::Type::TYPE_UNKNOWN, PermissionSet());
}

ExtensionInstallStatus GetWebstoreExtensionInstallStatus(
    const ExtensionId& extension_id,
    Profile* profile,
    const Manifest::Type manifest_type,
    const PermissionSet& required_permission_set,
    int manifest_version) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));

  const GURL update_url = extension_urls::GetWebstoreUpdateUrl();
  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(profile);
  // Always use webstore update url to check the installation mode because this
  // function is used by webstore private API only and there may not be any
  // |Extension| instance. Note that we don't handle the case where an offstore
  // extension with an identical ID is installed.
  ExtensionManagement::InstallationMode mode =
      extension_management->GetInstallationMode(extension_id,
                                                update_url.spec());

  if (mode == ExtensionManagement::INSTALLATION_FORCED ||
      mode == ExtensionManagement::INSTALLATION_RECOMMENDED)
    return kForceInstalled;

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);

  // Check if parent approval is needed for a supervised user to install
  // a new extension.
  if (base::FeatureList::IsEnabled(
          supervised_user::
              kExposedParentalControlNeededForExtensionInstallation) &&
      !registry->GetInstalledExtension(extension_id) &&
      supervised_user::AreExtensionsPermissionsEnabled(profile) &&
      !supervised_user::SupervisedUserCanSkipExtensionParentApprovals(
          profile) &&
      !base::Contains(profile->GetPrefs()->GetDict(
                          prefs::kSupervisedUserApprovedExtensions),
                      extension_id)) {
    return kCustodianApprovalRequiredForInstallation;
  }
  // Check if parent approval is needed for a supervised user to enable
  // an existing extension which is missing parental approval.
  if (registry->GetInstalledExtension(extension_id) &&
      ExtensionPrefs::Get(profile)->HasDisableReason(
          extension_id, disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED)) {
    return kCustodianApprovalRequired;
  }

  if (registry->enabled_extensions().Contains(extension_id)) {
    return kEnabled;
  }

  if (registry->terminated_extensions().Contains(extension_id)) {
    return kTerminated;
  }

  if (registry->blocklisted_extensions().Contains(extension_id)) {
    return kBlocklisted;
  }

  // When manifest version is not allowed, the extension is blocked and can't be
  // requested.
  if (!extension_management->IsAllowedManifestVersion(
          manifest_version, extension_id, manifest_type)) {
    return kBlockedByPolicy;
  }

  bool is_blocked_by_policy = IsExtensionInstallBlockedByPolicy(
      extension_management, mode, extension_id, update_url.spec(),
      manifest_type, required_permission_set);

  // Check if it's possible for the extension to be requested.
  if (is_blocked_by_policy) {
    // The ability to request extension installs is not available if the
    // extension request policy is disabled.
    if (!profile->GetPrefs()->GetBoolean(
            prefs::kCloudExtensionRequestEnabled)) {
      return kBlockedByPolicy;
    }

    // An extension which is explicitly blocked by enterprise policy can't be
    // requested anymore.
    if (extension_management->IsInstallationExplicitlyBlocked(extension_id)) {
      return kBlockedByPolicy;
    }
  }

  // Check if the extension is using an unsupported manifest version.
  ManifestV2ExperimentManager* mv2_experiment_manager =
      ManifestV2ExperimentManager::Get(profile);
  // At this point, we don't know what the extension manifest location really
  // is (because it's not installed). We pretend it will be kInternal, which
  // reflects an extension installed by the webstore.
  constexpr mojom::ManifestLocation kManifestLocation =
      mojom::ManifestLocation::kInternal;
  if (mv2_experiment_manager->ShouldBlockExtensionInstallation(
          extension_id, manifest_version, manifest_type, kManifestLocation,
          HashedExtensionId(extension_id))) {
    // The extension is using a deprecated manifest version and should not
    // be installable.
    return kDeprecatedManifestVersion;
  }

  // If an installed extension is disabled due to policy, return kCanRequest or
  // kRequestPending instead of kDisabled.
  // By doing so, user can still request an installed and policy blocked
  // extension.
  if (is_blocked_by_policy) {
    if (profile->GetPrefs()
            ->GetDict(prefs::kCloudExtensionRequestIds)
            .Find(extension_id)) {
      return kRequestPending;
    }

    return kCanRequest;
  }

  if (registry->disabled_extensions().Contains(extension_id)) {
    bool is_corrupted = ExtensionPrefs::Get(profile)->HasDisableReason(
        extension_id, disable_reason::DISABLE_CORRUPTED);
    return is_corrupted ? kCorrupted : kDisabled;
  }

  return kInstallable;
}

}  // namespace extensions

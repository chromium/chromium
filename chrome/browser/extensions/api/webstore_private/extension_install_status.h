// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_EXTENSION_INSTALL_STATUS_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_EXTENSION_INSTALL_STATUS_H_

#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"

class Profile;

namespace extensions {

class PermissionSet;

enum ExtensionInstallStatus {
  // Extension is blocked by policy but can be requested.
  kCanRequest,
  // Extension install request has been sent and is waiting to be reviewed.
  kRequestPending,
  // Extension is blocked by policy and can not be requested.
  kBlockedByPolicy,
  // Extension is not installed and has not not been blocked by policy.
  kInstallable,
  // Extension has been installed and it's enabled.
  kEnabled,
  // Extension has been installed but it's disabled and not blocked by policy.
  kDisabled,
  // Extension has been installed but it's terminated.
  kTerminated,
  // Extension is blocklisted.
  kBlocklisted,
  // Existing extension requires custodian approval to enable.
  kCustodianApprovalRequired,
  // New extension requires custodian approval to be installed.
  kCustodianApprovalRequiredForInstallation,
  // Extension is force installed or recommended by policy.
  kForceInstalled,
  // The extension may not be installed because it uses an unsupported manifest
  // version.
  kDeprecatedManifestVersion,
  // Extension has been installed but it's corrupted.
  kCorrupted,
};

// Returns the Extension install status for a Chrome web store extension with
// |extension_id| in |profile|. Note that this function won't check whether the
// extension's manifest type, required permissions are blocked by enterprise
// policy. type blocking or permission blocking or manifest version. Please use
// this function only if manifest file is not available.
ExtensionInstallStatus GetWebstoreExtensionInstallStatus(
    const ExtensionId& extension_id,
    Profile* profile);

// Returns the Extension install status for a Chrome web store extension with
// `extension_id` in `profile`. Also check if `manifest_type`, any permission
// in `required_permission_set` is blocked by enterprise policy or
// `manifest_version` is allowed.  `manifest_version` is only valid for
// TYPE_EXTENSION.
ExtensionInstallStatus GetWebstoreExtensionInstallStatus(
    const ExtensionId& extension_id,
    Profile* profile,
    const Manifest::Type manifest_type,
    const PermissionSet& required_permission_set,
    int manifest_version = 3);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_EXTENSION_INSTALL_STATUS_H_

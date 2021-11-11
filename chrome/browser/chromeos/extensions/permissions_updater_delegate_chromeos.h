// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_PERMISSIONS_UPDATER_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_PERMISSIONS_UPDATER_DELEGATE_CHROMEOS_H_

#include <memory>

#include "chrome/browser/extensions/permissions_updater.h"

namespace extensions {

class Extension;
class PermissionSet;

// In Public Sessions, apps and extensions are force-installed by admin policy
// so the user does not get a chance to review the permissions for these apps.
// This is not acceptable from a security/privacy standpoint, therefore we
// remove unsecure permissions (whitelisted apps and extensions are exempt from
// this - eg. remote desktop clients).
class PermissionsUpdaterDelegateChromeOS : public PermissionsUpdater::Delegate {
 public:
  PermissionsUpdaterDelegateChromeOS();

  PermissionsUpdaterDelegateChromeOS(
      const PermissionsUpdaterDelegateChromeOS&) = delete;
  PermissionsUpdaterDelegateChromeOS& operator=(
      const PermissionsUpdaterDelegateChromeOS&) = delete;

  ~PermissionsUpdaterDelegateChromeOS() override;

  // PermissionsUpdater::Delegate
  void InitializePermissions(
      const Extension* extension,
      std::unique_ptr<const PermissionSet>* granted_permissions) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_PERMISSIONS_UPDATER_DELEGATE_CHROMEOS_H_

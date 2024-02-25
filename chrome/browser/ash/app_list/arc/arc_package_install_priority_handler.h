// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PACKAGE_INSTALL_PRIORITY_HANDLER_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PACKAGE_INSTALL_PRIORITY_HANDLER_H_

#include <string>
#include <unordered_map>

#include "base/memory/raw_ptr.h"

#include "ash/components/arc/mojom/app.mojom.h"

class Profile;

namespace arc {

// A class that handles install priority to Play Store.
// Handles install request from app sync and fast app reinstall and update
// install priority based on user interaction with promised icon.
class ArcPackageInstallPriorityHandler {
 public:
  explicit ArcPackageInstallPriorityHandler(Profile* profile);
  ArcPackageInstallPriorityHandler(const ArcPackageInstallPriorityHandler&) =
      delete;
  ArcPackageInstallPriorityHandler& operator=(
      const ArcPackageInstallPriorityHandler&) = delete;
  ~ArcPackageInstallPriorityHandler();

  // Called when profile is shutting down.
  void Shutdown();

  // Used when user try to interact with promised icon.
  // Will try to upgrade install priority if possible.
  void PromotePackageInstall(const std::string& package_name);

  // Requests to install a package from sync with given priority.
  // When called from synced source, |priority| should be InstallPriority::kLow.
  // When called from PromotePackageInstall, |priority| should be
  // InstallPriority::kMedium.
  void InstallSyncedPacakge(const std::string& pacakge_name,
                            arc::mojom::InstallPriority priority);

  // TODO(lgcheng) add methond for install fast app reinstall apps.

  void ClearPackage(const std::string& package_name);

  // Called when connection to Android is closed.
  void Clear();

  arc::mojom::InstallPriority GetInstallPriorityForTesting(
      const std::string& package_name) const;

 private:
  raw_ptr<Profile> profile_;
  std::unordered_map<std::string, arc::mojom::InstallPriority>
      synced_pacakge_priority_map_;
  // TODO(lgcheng) add record for fast app reinstall apps.
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PACKAGE_INSTALL_PRIORITY_HANDLER_H_

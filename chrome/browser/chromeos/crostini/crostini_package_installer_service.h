// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PACKAGE_INSTALLER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PACKAGE_INSTALLER_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_package_installer_notification.h"
#include "components/keyed_service/core/keyed_service.h"

namespace crostini {

class CrostiniPackageInstallerService
    : public KeyedService,
      public InstallLinuxPackageProgressObserver {
 public:
  static CrostiniPackageInstallerService* GetForProfile(Profile* profile);

  explicit CrostiniPackageInstallerService(Profile* profile);
  ~CrostiniPackageInstallerService() override;

  // KeyedService:
  void Shutdown() override;

  void NotificationClosed(CrostiniPackageInstallerNotification* notification);

  // The package installer service caches the most recent retrieved package
  // info, for use in a package install notification.
  // TODO(timloh): Actually cache the values.
  void GetLinuxPackageInfo(
      const std::string& vm_name,
      const std::string& container_name,
      const std::string& package_path,
      CrostiniManager::GetLinuxPackageInfoCallback callback);

  // Install a Linux package. If successfully started, a system notification
  // will be used to display further updates.
  void InstallLinuxPackage(
      const std::string& vm_name,
      const std::string& container_name,
      const std::string& package_path,
      CrostiniManager::InstallLinuxPackageCallback callback);

  // InstallLinuxPackageProgressObserver:
  void OnInstallLinuxPackageProgress(
      const std::string& vm_name,
      const std::string& container_name,
      InstallLinuxPackageProgressStatus result,
      int progress_percent,
      const std::string& failure_reason) override;

 private:
  // Wraps the callback provided in GetLinuxPackageInfo().
  void OnGetLinuxPackageInfo(
      const std::string& vm_name,
      const std::string& container_name,
      CrostiniManager::GetLinuxPackageInfoCallback callback,
      const LinuxPackageInfo& linux_package_info);

  // Wraps the callback provided in InstallLinuxPackage().
  void OnInstallLinuxPackage(
      const std::string& vm_name,
      const std::string& container_name,
      CrostiniManager::InstallLinuxPackageCallback callback,
      CrostiniResult result,
      const std::string& failure_reason);

  std::string GetUniqueNotificationId();

  Profile* profile_;

  // Keyed on <vm_name, container_name>. A container can only have one install
  // running at a time, but we need to keep notifications around until they're
  // dismissed.
  std::map<std::pair<std::string, std::string>,
           std::unique_ptr<CrostiniPackageInstallerNotification>>
      running_notifications_;
  std::vector<std::unique_ptr<CrostiniPackageInstallerNotification>>
      finished_notifications_;

  int next_notification_id = 0;

  base::WeakPtrFactory<CrostiniPackageInstallerService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniPackageInstallerService);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PACKAGE_INSTALLER_SERVICE_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PACKAGE_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PACKAGE_SERVICE_H_

#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_package_notification.h"
#include "chrome/browser/chromeos/crostini/crostini_package_operation_status.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "storage/browser/file_system/file_system_url.h"

namespace crostini {

class CrostiniPackageService : public KeyedService,
                               public LinuxPackageOperationProgressObserver,
                               public PendingAppListUpdatesObserver,
                               public VmShutdownObserver {
 public:
  using StateChangeCallback =
      base::RepeatingCallback<void(PackageOperationStatus)>;

  static CrostiniPackageService* GetForProfile(Profile* profile);

  explicit CrostiniPackageService(Profile* profile);
  ~CrostiniPackageService() override;

  // For testing: Set a callback that will be called each time a notification
  // is set to a new state.
  void SetNotificationStateChangeCallbackForTesting(
      StateChangeCallback state_change_callback);

  // KeyedService:
  void Shutdown() override;

  void NotificationCompleted(CrostiniPackageNotification* notification);

  // The package installer service caches the most recent retrieved package
  // info, for use in a package install notification.
  // TODO(timloh): Actually cache the values.
  void GetLinuxPackageInfo(
      const std::string& vm_name,
      const std::string& container_name,
      const storage::FileSystemURL& package_url,
      CrostiniManager::GetLinuxPackageInfoCallback callback);

  // LinuxPackageOperationProgressObserver:
  void OnInstallLinuxPackageProgress(const ContainerId& container_id,
                                     InstallLinuxPackageProgressStatus status,
                                     int progress_percent) override;

  void OnUninstallPackageProgress(const ContainerId& container_id,
                                  UninstallPackageProgressStatus status,
                                  int progress_percent) override;

  // PendingAppListUpdatesObserver:
  void OnPendingAppListUpdates(const ContainerId& container_id,
                               int count) override;

  // VmShutdownObserver
  void OnVmShutdown(const std::string& vm_name) override;

  // (Eventually) install a Linux package. If successfully started, a system
  // notification will be used to display further updates.
  void QueueInstallLinuxPackage(
      const std::string& vm_name,
      const std::string& container_name,
      const storage::FileSystemURL& package_url,
      CrostiniManager::InstallLinuxPackageCallback callback);

  // (Eventually) uninstall the package identified by |app_id|. If successfully
  // started, a system notification will be used to display further updates.
  void QueueUninstallApplication(const std::string& app_id);

 private:
  // The user can request new operations while a different operation is in
  // progress. Rather than sending a request which will fail, just queue the
  // request until the previous one is done.
  struct QueuedInstall;
  struct QueuedUninstall;

  bool ContainerHasRunningOperation(const ContainerId& container_id) const;
  bool ContainerHasQueuedOperation(const ContainerId& container_id) const;

  // Creates a new notification and adds it to running_notifications_.
  // |app_name| is the name of the application being modified, if any -- for
  // installs, it will be blank, but for uninstalls, it will have the localized
  // name of the application in UTF8.
  // If there is a running notification, it will be set to error state. Caller
  // should check before calling this if a different behavior is desired.
  void CreateRunningNotification(
      const ContainerId& container_id,
      CrostiniPackageNotification::NotificationType notification_type,
      const std::string& app_name);

  // Creates a new uninstall notification and adds it to queued_uninstalls_.
  void CreateQueuedUninstall(const ContainerId& container_id,
                             const std::string& app_id,
                             const std::string& app_name);

  // Creates a new install notification and adds it to queued_installs_.
  void CreateQueuedInstall(
      const ContainerId& container_id,
      const std::string& package,
      CrostiniManager::InstallLinuxPackageCallback callback);

  // Sets the operation status of the current operation. Sets the notification
  // window's current state and updates containers_with_running_operations_.
  // Note that if status is |SUCCEEDED| or |FAILED|, this may kick off another
  // operation from the queued_uninstalls_ list.
  void UpdatePackageOperationStatus(const ContainerId& container_id,
                                    PackageOperationStatus status,
                                    int progress_percent);

  // Callback between sharing and invoking GetLinuxPackageInfo().
  void OnSharePathForGetLinuxPackageInfo(
      const std::string& vm_name,
      const std::string& container_name,
      const storage::FileSystemURL& package_url,
      const base::FilePath& package_path,
      CrostiniManager::GetLinuxPackageInfoCallback callback,
      bool share_success,
      const std::string& share_failure_reason);

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
      CrostiniResult result);

  // Kicks off an uninstall of the given app. Never queues the operation. Helper
  // for QueueUninstallApplication (if the operation can be performed
  // immediately) and StartQueuedOperation.
  void UninstallApplication(
      const CrostiniRegistryService::Registration& registration,
      const std::string& app_id);

  // Callback when the Crostini container is up and ready to accept messages.
  // Used by the uninstall flow only.
  void OnCrostiniRunningForUninstall(const ContainerId& container_id,
                                     const std::string& desktop_file_id,
                                     CrostiniResult result);

  // Callback for CrostiniManager::UninstallPackageOwningFile().
  void OnUninstallPackageOwningFile(const ContainerId& container_id,
                                    CrostiniResult result);

  // Kick off the next operation in the queue for the given container.
  void StartQueuedOperation(const ContainerId& container_id);

  std::string GetUniqueNotificationId();

  Profile* profile_;

  // The notifications in the RUNNING state for each container.
  std::map<ContainerId, std::unique_ptr<CrostiniPackageNotification>>
      running_notifications_;

  // Installs we want to run when the current operation is done.
  std::map<ContainerId, std::queue<QueuedInstall>> queued_installs_;

  // Uninstalls we want to run when the current operation is done.
  std::map<ContainerId, std::queue<QueuedUninstall>> queued_uninstalls_;

  // Notifications in a finished state (either SUCCEEDED or FAILED). We need
  // to keep notifications around until they are dismissed even if we don't
  // update them any more.
  std::vector<std::unique_ptr<CrostiniPackageNotification>>
      finished_notifications_;

  // A map storing which containers have currently pending app list update
  // operations. If a container is not present in the map, we assume no pending
  // updates.
  std::set<ContainerId> has_pending_app_list_updates_;

  // Called each time a notification is set to a new state.
  StateChangeCallback testing_state_change_callback_;

  int next_notification_id_ = 0;

  base::WeakPtrFactory<CrostiniPackageService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniPackageService);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PACKAGE_SERVICE_H_

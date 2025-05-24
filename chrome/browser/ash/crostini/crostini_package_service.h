// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_PACKAGE_SERVICE_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_PACKAGE_SERVICE_H_

#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_package_notification.h"
#include "chrome/browser/ash/crostini/crostini_package_operation_status.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "storage/browser/file_system/file_system_url.h"

namespace crostini {

class CrostiniPackageService : public KeyedService,
                               public LinuxPackageOperationProgressObserver,
                               public PendingAppListUpdatesObserver,
                               public ash::VmShutdownObserver {
 public:
  using StateChangeCallback =
      base::RepeatingCallback<void(PackageOperationStatus)>;

  explicit CrostiniPackageService(Profile* profile);

  CrostiniPackageService(const CrostiniPackageService&) = delete;
  CrostiniPackageService& operator=(const CrostiniPackageService&) = delete;

  ~CrostiniPackageService() override;

  // For testing: Set a callback that will be called each time a notification
  // is set to a new state.
  void SetNotificationStateChangeCallbackForTesting(
      StateChangeCallback state_change_callback);

  // KeyedService:
  void Shutdown() override;

  void NotificationCompleted(CrostiniPackageNotification* notification);

  void GetLinuxPackageInfo(
      const guest_os::GuestId& container_id,
      const storage::FileSystemURL& package_url,
      CrostiniManager::GetLinuxPackageInfoCallback callback);

  // LinuxPackageOperationProgressObserver:
  void OnInstallLinuxPackageProgress(const guest_os::GuestId& container_id,
                                     InstallLinuxPackageProgressStatus status,
                                     int progress_percent,
                                     const std::string& error_message) override;

  void OnUninstallPackageProgress(const guest_os::GuestId& container_id,
                                  UninstallPackageProgressStatus status,
                                  int progress_percent) override;

  // PendingAppListUpdatesObserver:
  void OnPendingAppListUpdates(const guest_os::GuestId& container_id,
                               int count) override;

  // ash::VmShutdownObserver
  void OnVmShutdown(const std::string& vm_name) override;

  // (Eventually) install a Linux package. If successfully started, a system
  // notification will be used to display further updates.
  void QueueInstallLinuxPackage(
      const guest_os::GuestId& container_id,
      const storage::FileSystemURL& package_url,
      CrostiniManager::InstallLinuxPackageCallback callback);

  // (Eventually) uninstall the package identified by |app_id|. If successfully
  // started, a system notification will be used to display further updates.
  void QueueUninstallApplication(const std::string& app_id);

  CrostiniManager::RestartId GetRestartIdForTesting();

 private:
  // The user can request new operations while a different operation is in
  // progress. Rather than sending a request which will fail, just queue the
  // request until the previous one is done.
  struct QueuedInstall;
  struct QueuedUninstall;

  bool ContainerHasRunningOperation(
      const guest_os::GuestId& container_id) const;
  bool ContainerHasQueuedOperation(const guest_os::GuestId& container_id) const;

  // Creates a new notification and adds it to running_notifications_.
  // |app_name| is the name of the application being modified, if any -- for
  // installs, it will be blank, but for uninstalls, it will have the localized
  // name of the application in UTF8.
  // If there is a running notification, it will be set to error state. Caller
  // should check before calling this if a different behavior is desired.
  void CreateRunningNotification(
      const guest_os::GuestId& container_id,
      CrostiniPackageNotification::NotificationType notification_type,
      const std::string& app_name);

  // Creates a new uninstall notification and adds it to queued_uninstalls_.
  void CreateQueuedUninstall(const guest_os::GuestId& container_id,
                             const std::string& app_id,
                             const std::string& app_name);

  // Creates a new install notification and adds it to queued_installs_.
  void CreateQueuedInstall(
      const guest_os::GuestId& container_id,
      const std::string& package,
      CrostiniManager::InstallLinuxPackageCallback callback);

  // Sets the operation status of the current operation. Sets the notification
  // window's current state and updates containers_with_running_operations_.
  // Note that if status is |SUCCEEDED| or |FAILED|, this may kick off another
  // operation from the queued_uninstalls_ list. When status is |FAILED|, the
  // |error_message| will contain an error reported by the installation process.
  void UpdatePackageOperationStatus(const guest_os::GuestId& container_id,
                                    PackageOperationStatus status,
                                    int progress_percent,
                                    const std::string& error_message = {});

  // Callback between sharing and invoking GetLinuxPackageInfo().
  void OnSharePathForGetLinuxPackageInfo(
      const guest_os::GuestId& container_id,
      const storage::FileSystemURL& package_url,
      const base::FilePath& package_path,
      CrostiniManager::GetLinuxPackageInfoCallback callback,
      CrostiniResult result);

  // Wraps the callback provided in GetLinuxPackageInfo().
  void OnGetLinuxPackageInfo(
      const guest_os::GuestId& container_id,
      CrostiniManager::GetLinuxPackageInfoCallback callback,
      const LinuxPackageInfo& linux_package_info);

  // Wraps the callback provided in InstallLinuxPackage().
  void OnInstallLinuxPackage(
      const guest_os::GuestId& container_id,
      CrostiniManager::InstallLinuxPackageCallback callback,
      CrostiniResult result);

  // Kicks off an uninstall of the given app. Never queues the operation. Helper
  // for QueueUninstallApplication (if the operation can be performed
  // immediately) and StartQueuedOperation.
  void UninstallApplication(
      const guest_os::GuestOsRegistryService::Registration& registration,
      const std::string& app_id);

  // Callback when the Crostini container is up and ready to accept messages.
  // Used by the uninstall flow only.
  void OnCrostiniRunningForUninstall(const guest_os::GuestId& container_id,
                                     const std::string& desktop_file_id,
                                     CrostiniResult result);

  // Callback for CrostiniManager::UninstallPackageOwningFile().
  void OnUninstallPackageOwningFile(const guest_os::GuestId& container_id,
                                    CrostiniResult result);

  // Kick off the next operation in the queue for the given container.
  void StartQueuedOperation(const guest_os::GuestId& container_id);

  std::string GetUniqueNotificationId();

  raw_ptr<Profile> profile_;

  // The notifications in the RUNNING state for each container.
  std::map<guest_os::GuestId, std::unique_ptr<CrostiniPackageNotification>>
      running_notifications_;

  // Installs we want to run when the current operation is done.
  std::map<guest_os::GuestId, std::queue<QueuedInstall>> queued_installs_;

  // Uninstalls we want to run when the current operation is done.
  std::map<guest_os::GuestId, std::queue<QueuedUninstall>> queued_uninstalls_;

  // Notifications in a finished state (either SUCCEEDED or FAILED). We need
  // to keep notifications around until they are dismissed even if we don't
  // update them any more.
  std::vector<std::unique_ptr<CrostiniPackageNotification>>
      finished_notifications_;

  // A map storing which containers have currently pending app list update
  // operations. If a container is not present in the map, we assume no pending
  // updates.
  std::set<guest_os::GuestId> has_pending_app_list_updates_;

  // Called each time a notification is set to a new state.
  StateChangeCallback testing_state_change_callback_;

  int next_notification_id_ = 0;

  CrostiniManager::RestartId restart_id_for_testing_;

  base::WeakPtrFactory<CrostiniPackageService> weak_ptr_factory_{this};
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_PACKAGE_SERVICE_H_

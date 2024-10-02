// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_package_service.h"

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/profiles/profile.h"

namespace crostini {

namespace {

PackageOperationStatus InstallStatusToOperationStatus(
    InstallLinuxPackageProgressStatus status) {
  switch (status) {
    case InstallLinuxPackageProgressStatus::SUCCEEDED:
      return PackageOperationStatus::SUCCEEDED;
    case InstallLinuxPackageProgressStatus::FAILED:
      return PackageOperationStatus::FAILED;
    case InstallLinuxPackageProgressStatus::DOWNLOADING:
    case InstallLinuxPackageProgressStatus::INSTALLING:
      return PackageOperationStatus::RUNNING;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

PackageOperationStatus UninstallStatusToOperationStatus(
    UninstallPackageProgressStatus status) {
  switch (status) {
    case UninstallPackageProgressStatus::SUCCEEDED:
      return PackageOperationStatus::SUCCEEDED;
    case UninstallPackageProgressStatus::FAILED:
      return PackageOperationStatus::FAILED;
    case UninstallPackageProgressStatus::UNINSTALLING:
      return PackageOperationStatus::RUNNING;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace

struct CrostiniPackageService::QueuedInstall {
  QueuedInstall(const std::string& package_path,
                CrostiniManager::InstallLinuxPackageCallback callback,
                std::unique_ptr<CrostiniPackageNotification> notification)
      : package_path(package_path),
        callback(std::move(callback)),
        notification(std::move(notification)) {}
  ~QueuedInstall() = default;

  std::string package_path;
  CrostiniManager::InstallLinuxPackageCallback callback;

  // Notification displaying "install queued"
  std::unique_ptr<CrostiniPackageNotification> notification;
};

struct CrostiniPackageService::QueuedUninstall {
  QueuedUninstall(
      const std::string& app_id,
      std::unique_ptr<CrostiniPackageNotification> notification_argument)
      : app_id(app_id), notification(std::move(notification_argument)) {}
  ~QueuedUninstall() = default;

  // App to uninstall
  std::string app_id;

  // Notification displaying "uninstall queued"
  std::unique_ptr<CrostiniPackageNotification> notification;
};

CrostiniPackageService::CrostiniPackageService(Profile* profile)
    : profile_(profile) {
  CrostiniManager* manager = CrostiniManager::GetForProfile(profile);

  manager->AddLinuxPackageOperationProgressObserver(this);
  manager->AddPendingAppListUpdatesObserver(this);
  manager->AddVmShutdownObserver(this);
}

CrostiniPackageService::~CrostiniPackageService() = default;

void CrostiniPackageService::Shutdown() {
  CrostiniManager* manager = CrostiniManager::GetForProfile(profile_);
  manager->RemoveLinuxPackageOperationProgressObserver(this);
  manager->RemovePendingAppListUpdatesObserver(this);
  manager->RemoveVmShutdownObserver(this);

  // CrostiniPackageNotification registers itself as a GuestOsRegistryService
  // observer, so they need to be destroyed here while the
  // GuestOsRegistryService still exists.
  running_notifications_.clear();
  queued_installs_.clear();
  queued_uninstalls_.clear();
  finished_notifications_.clear();
}

void CrostiniPackageService::SetNotificationStateChangeCallbackForTesting(
    StateChangeCallback state_change_callback) {
  testing_state_change_callback_ = std::move(state_change_callback);
}

void CrostiniPackageService::NotificationCompleted(
    CrostiniPackageNotification* notification) {
  for (auto it = finished_notifications_.begin();
       it != finished_notifications_.end(); ++it) {
    if (it->get() == notification) {
      finished_notifications_.erase(it);
      return;
    }
  }
  // Notifications should never delete themselves while queued or running.
  NOTREACHED_IN_MIGRATION();
}

void CrostiniPackageService::GetLinuxPackageInfo(
    const guest_os::GuestId& container_id,
    const storage::FileSystemURL& package_url,
    CrostiniManager::GetLinuxPackageInfoCallback callback) {
  base::FilePath path;
  if (!file_manager::util::ConvertFileSystemURLToPathInsideCrostini(
          profile_, package_url, &path)) {
    LinuxPackageInfo info;
    info.success = false;
    info.failure_reason = "Invalid package url: " + package_url.DebugString();
    return std::move(callback).Run(info);
  }

  // Share path if it is not in crostini.
  if (package_url.mount_filesystem_id() !=
      file_manager::util::GetCrostiniMountPointName(profile_)) {
    CrostiniManager::RestartOptions options;
    options.share_paths.push_back(package_url.path());
    restart_id_for_testing_ =
        CrostiniManager::GetForProfile(profile_)->RestartCrostiniWithOptions(
            container_id, std::move(options),
            base::BindOnce(
                &CrostiniPackageService::OnSharePathForGetLinuxPackageInfo,
                weak_ptr_factory_.GetWeakPtr(), container_id, package_url, path,
                std::move(callback)));
  } else {
    OnSharePathForGetLinuxPackageInfo(container_id, package_url, path,
                                      std::move(callback),
                                      CrostiniResult::SUCCESS);
  }
}

void CrostiniPackageService::OnSharePathForGetLinuxPackageInfo(
    const guest_os::GuestId& container_id,
    const storage::FileSystemURL& package_url,
    const base::FilePath& package_path,
    CrostiniManager::GetLinuxPackageInfoCallback callback,
    CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    LinuxPackageInfo info;
    info.success = false;
    info.failure_reason = "Error sharing package " + package_url.DebugString() +
                          ": " + CrostiniResultString(result);
    return std::move(callback).Run(info);
  }
  CrostiniManager::GetForProfile(profile_)->GetLinuxPackageInfo(
      container_id, package_path.value(),
      base::BindOnce(&CrostiniPackageService::OnGetLinuxPackageInfo,
                     weak_ptr_factory_.GetWeakPtr(), container_id,
                     std::move(callback)));
}

void CrostiniPackageService::QueueInstallLinuxPackage(
    const guest_os::GuestId& container_id,
    const storage::FileSystemURL& package_url,
    CrostiniManager::InstallLinuxPackageCallback callback) {
  base::FilePath path;
  if (!file_manager::util::ConvertFileSystemURLToPathInsideCrostini(
          profile_, package_url, &path)) {
    LOG(ERROR) << "Invalid install linux package: "
               << package_url.DebugString();
    return std::move(callback).Run(
        CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED);
  }

  if (ContainerHasRunningOperation(container_id)) {
    CreateQueuedInstall(container_id, path.value(), std::move(callback));
    return;
  }

  CreateRunningNotification(
      container_id,
      CrostiniPackageNotification::NotificationType::PACKAGE_INSTALL,
      /*app_name=*/"");

  CrostiniManager::GetForProfile(profile_)->InstallLinuxPackage(
      container_id, path.value(),
      base::BindOnce(&CrostiniPackageService::OnInstallLinuxPackage,
                     weak_ptr_factory_.GetWeakPtr(), container_id,
                     std::move(callback)));
}

void CrostiniPackageService::OnInstallLinuxPackageProgress(
    const guest_os::GuestId& container_id,
    InstallLinuxPackageProgressStatus status,
    int progress_percent,
    const std::string& error_message) {
  // Linux package install has two phases, downloading and installing, which we
  // map to a single progess percentage amount by dividing the range in half --
  // 0-50% for the downloading phase, 51-100% for the installing phase.
  int display_progress = progress_percent / 2;
  if (status == InstallLinuxPackageProgressStatus::INSTALLING) {
    display_progress += 50;  // Second phase
  }

  UpdatePackageOperationStatus(container_id,
                               InstallStatusToOperationStatus(status),
                               display_progress, error_message);
}

void CrostiniPackageService::OnUninstallPackageProgress(
    const guest_os::GuestId& container_id,
    UninstallPackageProgressStatus status,
    int progress_percent) {
  UpdatePackageOperationStatus(
      container_id, UninstallStatusToOperationStatus(status), progress_percent);
}

void CrostiniPackageService::OnVmShutdown(const std::string& vm_name) {
  // Making a notification as failed removes it from |running_notifications_|,
  // which invalidates the iterators. To avoid this, we record all the
  // containers that just shut down before removing any notifications.
  std::vector<guest_os::GuestId> to_remove;
  for (auto& running_notification : running_notifications_) {
    if (running_notification.first.vm_name == vm_name) {
      to_remove.push_back(running_notification.first);
    }
  }
  for (auto iter : to_remove) {
    // Use a loop because removing a notification from the running set can cause
    // a queued operation to start, which will also need to be removed.
    while (ContainerHasRunningOperation(iter)) {
      UpdatePackageOperationStatus(iter, PackageOperationStatus::FAILED, 0);
    }
  }
}

void CrostiniPackageService::QueueUninstallApplication(
    const std::string& app_id) {
  auto registration =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_)
          ->GetRegistration(app_id);
  if (!registration.has_value()) {
    LOG(ERROR)
        << "Tried to uninstall application that has already been uninstalled";
    return;
  }

  const guest_os::GuestId container_id(registration->VmType(),
                                       registration->VmName(),
                                       registration->ContainerName());
  const std::string app_name = registration->Name();
  if (ContainerHasRunningOperation(container_id)) {
    CreateQueuedUninstall(container_id, app_id, app_name);
    return;
  }

  CreateRunningNotification(
      container_id,
      CrostiniPackageNotification::NotificationType::APPLICATION_UNINSTALL,
      app_name);

  UninstallApplication(*registration, app_id);
}

bool CrostiniPackageService::ContainerHasRunningOperation(
    const guest_os::GuestId& container_id) const {
  return base::Contains(running_notifications_, container_id);
}

bool CrostiniPackageService::ContainerHasQueuedOperation(
    const guest_os::GuestId& container_id) const {
  return (base::Contains(queued_installs_, container_id) &&
          !queued_installs_.at(container_id).empty()) ||
         (base::Contains(queued_uninstalls_, container_id) &&
          !queued_uninstalls_.at(container_id).empty());
}

void CrostiniPackageService::CreateRunningNotification(
    const guest_os::GuestId& container_id,
    CrostiniPackageNotification::NotificationType notification_type,
    const std::string& app_name) {
  {  // Scope limit for |it|, which will become invalid shortly.
    auto it = running_notifications_.find(container_id);
    if (it != running_notifications_.end()) {
      // We could reach this if the final progress update signal from a previous
      // operation doesn't get sent, so we wouldn't end up moving the
      // previous notification out of running_notifications_. Clear it out by
      // moving to finished_notifications_.
      LOG(ERROR) << "Notification for package operation already exists.";
      it->second->ForceAllowAutoHide();
      finished_notifications_.emplace_back(std::move(it->second));
      running_notifications_.erase(it);
    }
  }

  running_notifications_[container_id] =
      std::make_unique<CrostiniPackageNotification>(
          profile_, notification_type, PackageOperationStatus::RUNNING,
          container_id, base::UTF8ToUTF16(app_name), GetUniqueNotificationId(),
          this);
}

void CrostiniPackageService::CreateQueuedUninstall(
    const guest_os::GuestId& container_id,
    const std::string& app_id,
    const std::string& app_name) {
  queued_uninstalls_[container_id].emplace(
      app_id,
      std::make_unique<CrostiniPackageNotification>(
          profile_,
          CrostiniPackageNotification::NotificationType::APPLICATION_UNINSTALL,
          PackageOperationStatus::QUEUED, container_id,
          base::UTF8ToUTF16(app_name), GetUniqueNotificationId(), this));
}

void CrostiniPackageService::CreateQueuedInstall(
    const guest_os::GuestId& container_id,
    const std::string& package,
    CrostiniManager::InstallLinuxPackageCallback callback) {
  queued_installs_[container_id].emplace(
      package, std::move(callback),
      std::make_unique<CrostiniPackageNotification>(
          profile_,
          CrostiniPackageNotification::NotificationType::PACKAGE_INSTALL,
          PackageOperationStatus::QUEUED, container_id,
          /*app_name=*/std::u16string(), GetUniqueNotificationId(), this));
}

void CrostiniPackageService::UpdatePackageOperationStatus(
    const guest_os::GuestId& container_id,
    PackageOperationStatus status,
    int progress_percent,
    const std::string& error_message) {
  // Update the notification window, if any.
  auto it = running_notifications_.find(container_id);
  if (it == running_notifications_.end()) {
    LOG(ERROR) << container_id << " has no notification to update";
    return;
  }
  if (it->second == nullptr) {
    LOG(ERROR) << container_id << " has null notification pointer";
    running_notifications_.erase(it);
    return;
  }

  // If an operation has finished, but there are still app list updates pending,
  // don't finish the flow yet.
  if (status == PackageOperationStatus::SUCCEEDED &&
      has_pending_app_list_updates_.count(container_id)) {
    status = PackageOperationStatus::WAITING_FOR_APP_REGISTRY_UPDATE;
  }

  it->second->UpdateProgress(status, progress_percent, error_message);

  if (status == PackageOperationStatus::SUCCEEDED ||
      status == PackageOperationStatus::FAILED) {
    finished_notifications_.emplace_back(std::move(it->second));
    running_notifications_.erase(it);

    // Kick off the next operation if we just finished one.
    if (ContainerHasQueuedOperation(container_id)) {
      StartQueuedOperation(container_id);
    }
  }
  if (testing_state_change_callback_) {
    testing_state_change_callback_.Run(status);
  }
}

void CrostiniPackageService::OnPendingAppListUpdates(
    const guest_os::GuestId& container_id,
    int count) {
  if (count != 0) {
    has_pending_app_list_updates_.insert(container_id);
  } else {
    has_pending_app_list_updates_.erase(container_id);
  }

  auto it = running_notifications_.find(container_id);
  if (it != running_notifications_.end()) {
    if (it->second->GetOperationStatus() ==
            PackageOperationStatus::WAITING_FOR_APP_REGISTRY_UPDATE &&
        count == 0) {
      UpdatePackageOperationStatus(container_id,
                                   PackageOperationStatus::SUCCEEDED, 100);
    }
  }
}

void CrostiniPackageService::OnGetLinuxPackageInfo(
    const guest_os::GuestId& container_id,
    CrostiniManager::GetLinuxPackageInfoCallback callback,
    const LinuxPackageInfo& linux_package_info) {
  std::move(callback).Run(linux_package_info);
}

void CrostiniPackageService::OnInstallLinuxPackage(
    const guest_os::GuestId& container_id,
    CrostiniManager::InstallLinuxPackageCallback callback,
    CrostiniResult result) {
  std::move(callback).Run(result);
  if (result != CrostiniResult::SUCCESS) {
    UpdatePackageOperationStatus(container_id, PackageOperationStatus::FAILED,
                                 0);
  }
}

void CrostiniPackageService::UninstallApplication(
    const guest_os::GuestOsRegistryService::Registration& registration,
    const std::string& app_id) {
  const guest_os::GuestId container_id(registration.VmType(),
                                       registration.VmName(),
                                       registration.ContainerName());

  // Policies can change under us, and crostini may now be forbidden.
  if (!CrostiniFeatures::Get()->IsAllowedNow(profile_)) {
    LOG(ERROR) << "Can't uninstall because policy no longer allows Crostini";
    UpdatePackageOperationStatus(container_id, PackageOperationStatus::FAILED,
                                 0);
    return;
  }

  // If Crostini is not running, launch it. This is a no-op if Crostini is
  // already running.
  CrostiniManager::GetForProfile(profile_)->RestartCrostini(
      container_id,
      base::BindOnce(&CrostiniPackageService::OnCrostiniRunningForUninstall,
                     weak_ptr_factory_.GetWeakPtr(), container_id,
                     registration.DesktopFileId()));
}

void CrostiniPackageService::OnCrostiniRunningForUninstall(
    const guest_os::GuestId& container_id,
    const std::string& desktop_file_id,
    CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to launch Crostini; uninstall aborted";
    UpdatePackageOperationStatus(container_id, PackageOperationStatus::FAILED,
                                 0);
    return;
  }
  CrostiniManager::GetForProfile(profile_)->UninstallPackageOwningFile(
      container_id, desktop_file_id,
      base::BindOnce(&CrostiniPackageService::OnUninstallPackageOwningFile,
                     weak_ptr_factory_.GetWeakPtr(), container_id));
}

void CrostiniPackageService::OnUninstallPackageOwningFile(
    const guest_os::GuestId& container_id,
    CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    // Let user know the uninstall failed.
    UpdatePackageOperationStatus(container_id, PackageOperationStatus::FAILED,
                                 0);
    return;
  }
  // Otherwise, just leave the notification alone in the "running" state.
  // SUCCESS just means we successfully *started* the uninstall.
}

void CrostiniPackageService::StartQueuedOperation(
    const guest_os::GuestId& container_id) {
  auto uninstall_queue_iter = queued_uninstalls_.find(container_id);
  if (uninstall_queue_iter != queued_uninstalls_.end() &&
      !uninstall_queue_iter->second.empty()) {
    std::string app_id;
    std::queue<QueuedUninstall>& uninstall_queue = uninstall_queue_iter->second;
    {  // Scope |next|; it becomes an invalid reference when we pop()
      QueuedUninstall& next = uninstall_queue.front();

      next.notification->UpdateProgress(PackageOperationStatus::RUNNING, 0);
      running_notifications_.emplace(container_id,
                                     std::move(next.notification));

      app_id = next.app_id;
      uninstall_queue.pop();  // Invalidates |next|
    }

    auto registration =
        guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_)
            ->GetRegistration(app_id);

    // It's possible that some other process has uninstalled this application
    // already. If this happens, we want to skip the notification directly to
    // the success state.
    if (registration.has_value()) {
      UninstallApplication(*registration, app_id);
    } else {
      // Note that this may call StartQueuedOperation, so we must allow for
      // potential re-entrancy.
      UpdatePackageOperationStatus(container_id,
                                   PackageOperationStatus::SUCCEEDED, 100);
    }

    // As recursive calls to StartQueuedOperation might delete |uninstall_queue|
    // and invalidate |uninstall_queue_iter| we must look it up again.
    uninstall_queue_iter = queued_uninstalls_.find(container_id);
    if (uninstall_queue_iter != queued_uninstalls_.end() &&
        uninstall_queue_iter->second.empty()) {
      // Clean up memory.
      queued_uninstalls_.erase(uninstall_queue_iter);
      // Invalidates |uninstall_queue_iter|.
    }
    return;
  }

  auto install_queue_iter = queued_installs_.find(container_id);
  if (install_queue_iter != queued_installs_.end() &&
      !install_queue_iter->second.empty()) {
    std::string package_path;
    CrostiniManager::InstallLinuxPackageCallback callback;

    std::queue<QueuedInstall>& install_queue = install_queue_iter->second;
    {  // Scope |next|; it becomes an invalid reference when we pop()
      QueuedInstall& next = install_queue.front();

      next.notification->UpdateProgress(PackageOperationStatus::RUNNING, 0);
      running_notifications_.emplace(container_id,
                                     std::move(next.notification));

      package_path = std::move(next.package_path);
      callback = std::move(next.callback);
      install_queue.pop();  // Invalidates |next|
    }

    CrostiniManager::GetForProfile(profile_)->InstallLinuxPackage(
        container_id, package_path,
        base::BindOnce(&CrostiniPackageService::OnInstallLinuxPackage,
                       weak_ptr_factory_.GetWeakPtr(), container_id,
                       std::move(callback)));

    // InstallLinuxPackage shouldn't be able to recursively call this method,
    // but as future proofing consider |install_queue_iter| to be invalidated
    // anyway.
    install_queue_iter = queued_installs_.find(container_id);
    if (install_queue_iter != queued_installs_.end() &&
        install_queue_iter->second.empty()) {
      // Clean up memory.
      queued_installs_.erase(install_queue_iter);
      // Invalidates |install_queue_iter|.
    }
    return;
  }

  NOTREACHED_IN_MIGRATION();
}

std::string CrostiniPackageService::GetUniqueNotificationId() {
  return base::StringPrintf("crostini_package_operation_%d",
                            next_notification_id_++);
}

CrostiniManager::RestartId CrostiniPackageService::GetRestartIdForTesting() {
  return restart_id_for_testing_;
}

}  // namespace crostini

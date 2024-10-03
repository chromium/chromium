// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/ansible/ansible_management_service.h"

#include <memory>
#include <sstream>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/crostini/crostini_ansible_software_config_view.h"
#include "components/prefs/pref_service.h"

namespace crostini {

const char kCrostiniDefaultAnsibleVersion[] =
    "ansible;2.2.1.0-2+deb9u1;all;debian-stable-main";

namespace {

ash::CiceroneClient* GetCiceroneClient() {
  return ash::CiceroneClient::Get();
}

}  // namespace

AnsibleConfiguration::AnsibleConfiguration(
    std::string playbook,
    base::FilePath path,
    base::OnceCallback<void(bool success)> callback)
    : playbook(playbook), path(path), callback(std::move(callback)) {}
AnsibleConfiguration::AnsibleConfiguration(
    base::FilePath path,
    base::OnceCallback<void(bool success)> callback)
    : AnsibleConfiguration("", path, std::move(callback)) {}

AnsibleConfiguration::~AnsibleConfiguration() {}

AnsibleManagementService::AnsibleManagementService(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}
AnsibleManagementService::~AnsibleManagementService() = default;

void AnsibleManagementService::ConfigureContainer(
    const guest_os::GuestId& container_id,
    base::FilePath playbook_path,
    base::OnceCallback<void(bool success)> callback) {
  if (configuration_tasks_.count(container_id) > 0) {
    LOG(ERROR) << "Attempting to configure a container which is already being "
                  "configured";
    std::move(callback).Run(false);
    return;
  }
  if (container_id == DefaultContainerId() &&
      !ShouldConfigureDefaultContainer(profile_)) {
    LOG(ERROR) << "Trying to configure default Crostini container when it "
               << "should not be configured";
    std::move(callback).Run(false);
    return;
  }
  // Add ourselves as an observer if we aren't already awaiting.
  if (configuration_tasks_.empty()) {
    CrostiniManager::GetForProfile(profile_)
        ->AddLinuxPackageOperationProgressObserver(this);
  }
  configuration_tasks_.emplace(
      std::make_pair(container_id, std::make_unique<AnsibleConfiguration>(
                                       playbook_path, std::move(callback))));

  for (auto& observer : observers_) {
    observer.OnAnsibleSoftwareConfigurationStarted(container_id);
  }
  CreateUiElement(container_id);
  CrostiniManager::GetForProfile(profile_)->InstallLinuxPackageFromApt(
      container_id, kCrostiniDefaultAnsibleVersion,
      base::BindOnce(&AnsibleManagementService::OnInstallAnsibleInContainer,
                     weak_ptr_factory_.GetWeakPtr(), container_id));
}

void AnsibleManagementService::CreateUiElement(
    const guest_os::GuestId& container_id) {
  ui_elements_[container_id] = views::DialogDelegate::CreateDialogWidget(
      std::make_unique<CrostiniAnsibleSoftwareConfigView>(profile_,
                                                          container_id),
      nullptr, nullptr);
  ui_elements_[container_id]->Show();
}

views::Widget* AnsibleManagementService::GetDialogWidgetForTesting(
    const guest_os::GuestId& container_id) {
  return ui_elements_.count(container_id) > 0 ? ui_elements_[container_id]
                                              : nullptr;
}

void AnsibleManagementService::AddConfigurationTaskForTesting(
    const guest_os::GuestId& container_id,
    views::Widget* widget) {
  configuration_tasks_[container_id] = std::make_unique<AnsibleConfiguration>(
      base::FilePath(), base::BindOnce([](bool success) {}));
  ui_elements_[container_id] = widget;
}

void AnsibleManagementService::OnInstallAnsibleInContainer(
    const guest_os::GuestId& container_id,
    CrostiniResult result) {
  // Check if cancelled.
  if (IsCancelled(container_id)) {
    return;
  }
  if (result == CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED) {
    LOG(ERROR) << "Ansible installation failed";
    OnConfigurationFinished(container_id, false);
    return;
  }

  DCHECK_NE(result, CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE);

  DCHECK_EQ(result, CrostiniResult::SUCCESS);
  VLOG(1) << "Ansible installation has been started successfully";
  // Waiting for Ansible installation progress being reported.
  for (auto& observer : observers_) {
    observer.OnAnsibleSoftwareInstall(container_id);
  }
}

void AnsibleManagementService::OnInstallLinuxPackageProgress(
    const guest_os::GuestId& container_id,
    InstallLinuxPackageProgressStatus status,
    int progress_percent,
    const std::string& error_message) {
  // Check if cancelled.
  if (IsCancelled(container_id)) {
    return;
  }
  std::stringstream status_line;
  switch (status) {
    case InstallLinuxPackageProgressStatus::SUCCEEDED: {
      GetAnsiblePlaybookToApply(container_id);
      return;
    }
    case InstallLinuxPackageProgressStatus::FAILED:
      LOG(ERROR) << "Ansible installation failed";
      OnConfigurationFinished(container_id, false);
      return;
    // TODO(okalitova): Report Ansible downloading/installation progress.
    case InstallLinuxPackageProgressStatus::DOWNLOADING:
      status_line << "Ansible downloading progress: " << progress_percent
                  << "%";
      VLOG(1) << status_line.str();
      for (auto& observer : observers_) {
        observer.OnAnsibleSoftwareConfigurationProgress(
            container_id, std::vector<std::string>({status_line.str()}));
      }
      return;
    case InstallLinuxPackageProgressStatus::INSTALLING:
      status_line << "Ansible installing progress: " << progress_percent << "%";
      VLOG(1) << status_line.str();
      for (auto& observer : observers_) {
        observer.OnAnsibleSoftwareConfigurationProgress(
            container_id, std::vector<std::string>({status_line.str()}));
      }
      return;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void AnsibleManagementService::GetAnsiblePlaybookToApply(
    const guest_os::GuestId& container_id) {
  // Check if cancelled.
  if (IsCancelled(container_id)) {
    return;
  }
  const base::FilePath& ansible_playbook_file_path =
      configuration_tasks_[container_id]->path;
  bool success = base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(base::ReadFileToString, ansible_playbook_file_path,
                     &configuration_tasks_[container_id]->playbook),
      base::BindOnce(&AnsibleManagementService::OnAnsiblePlaybookRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), container_id));
  if (!success) {
    LOG(ERROR) << "Failed to post task to retrieve Ansible playbook content";
    OnConfigurationFinished(container_id, false);
  }
}

void AnsibleManagementService::OnAnsiblePlaybookRetrieved(
    const guest_os::GuestId& container_id,
    bool success) {
  // Check if cancelled.
  if (IsCancelled(container_id)) {
    return;
  }
  if (!success) {
    LOG(ERROR) << "Failed to retrieve Ansible playbook content";
    OnConfigurationFinished(container_id, false);
    return;
  }

  ApplyAnsiblePlaybook(container_id);
}

void AnsibleManagementService::ApplyAnsiblePlaybook(
    const guest_os::GuestId& container_id) {
  // Check if cancelled.
  if (IsCancelled(container_id)) {
    return;
  }
  if (!GetCiceroneClient()->IsApplyAnsiblePlaybookProgressSignalConnected()) {
    // Technically we could still start the application, but we wouldn't be able
    // to detect when the application completes, successfully or otherwise.
    LOG(ERROR)
        << "Attempted to apply playbook when progress signal not connected.";
    OnConfigurationFinished(container_id, false);
    return;
  }

  vm_tools::cicerone::ApplyAnsiblePlaybookRequest request;
  request.set_owner_id(CryptohomeIdForProfile(profile_));
  request.set_vm_name(container_id.vm_name);
  request.set_container_name(container_id.container_name);
  request.set_playbook(configuration_tasks_[container_id]->playbook);

  GetCiceroneClient()->ApplyAnsiblePlaybook(
      std::move(request),
      base::BindOnce(&AnsibleManagementService::OnApplyAnsiblePlaybook,
                     weak_ptr_factory_.GetWeakPtr(), container_id));
}

void AnsibleManagementService::OnApplyAnsiblePlaybook(
    const guest_os::GuestId& container_id,
    std::optional<vm_tools::cicerone::ApplyAnsiblePlaybookResponse> response) {
  // Check if cancelled.
  if (IsCancelled(container_id)) {
    return;
  }
  if (!response) {
    LOG(ERROR) << "Failed to apply Ansible playbook. Empty response.";
    OnConfigurationFinished(container_id, false);
    return;
  }

  if (response->status() ==
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::FAILED) {
    LOG(ERROR) << "Failed to apply Ansible playbook: "
               << response->failure_reason();
    OnConfigurationFinished(container_id, false);
    return;
  }

  VLOG(1) << "Ansible playbook application has been started successfully";
  // Waiting for Ansible playbook application progress being reported.
  // TODO(https://crbug.com/1043060): Add a timeout after which we stop waiting.
  for (auto& observer : observers_) {
    observer.OnApplyAnsiblePlaybook(container_id);
  }
}

void AnsibleManagementService::OnApplyAnsiblePlaybookProgress(
    const vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal& signal) {
  guest_os::GuestId container_id(kCrostiniDefaultVmType, signal.vm_name(),
                                 signal.container_name());
  // Check if cancelled.
  if (IsCancelled(container_id)) {
    return;
  }
  switch (signal.status()) {
    case vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::SUCCEEDED:
      OnConfigurationFinished(container_id, true);
      break;
    case vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::FAILED:
      LOG(ERROR) << "Ansible playbook application has failed with reason:\n"
                 << signal.failure_details();
      OnConfigurationFinished(container_id, false);
      break;
    case vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::IN_PROGRESS:
      for (auto& observer : observers_) {
        observer.OnAnsibleSoftwareConfigurationProgress(
            container_id,
            std::vector<std::string>(signal.status_string().begin(),
                                     signal.status_string().end()));
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void AnsibleManagementService::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void AnsibleManagementService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AnsibleManagementService::OnUninstallPackageProgress(
    const guest_os::GuestId& container_id,
    UninstallPackageProgressStatus status,
    int progress_percent) {
  NOTIMPLEMENTED();
}

void AnsibleManagementService::OnConfigurationFinished(
    const guest_os::GuestId& container_id,
    bool success) {
  // Check if cancelled.
  if (IsCancelled(container_id)) {
    return;
  }
  if (success && container_id == DefaultContainerId()) {
    profile_->GetPrefs()->SetBoolean(prefs::kCrostiniDefaultContainerConfigured,
                                     true);
  }
  for (auto& observer : observers_) {
    observer.OnAnsibleSoftwareConfigurationFinished(container_id, success);
  }
  for (auto& observer : observers_) {
    // Interactive prompt currently only occurs when there has been a failure.
    observer.OnAnsibleSoftwareConfigurationUiPrompt(container_id, !success);
  }
}

void AnsibleManagementService::RetryConfiguration(
    const guest_os::GuestId& container_id) {
  // We're not 100% sure where we lost connection, so we'll have to restart from
  // the very beginning.
  DCHECK_GT(configuration_tasks_.count(container_id), 0u);
  VLOG(1) << "Retrying configuration";
  CrostiniManager::GetForProfile(profile_)->InstallLinuxPackageFromApt(
      container_id, kCrostiniDefaultAnsibleVersion,
      base::BindOnce(&AnsibleManagementService::OnInstallAnsibleInContainer,
                     weak_ptr_factory_.GetWeakPtr(), container_id));
}

void AnsibleManagementService::CancelConfiguration(
    const guest_os::GuestId& container_id) {
  DCHECK_GT(configuration_tasks_.count(container_id), 0u);
  OnConfigurationFinished(container_id, false);
  CompleteConfiguration(container_id, false);
}

void AnsibleManagementService::CompleteConfiguration(
    const guest_os::GuestId& container_id,
    bool success) {
  // Check if cancelled.
  if (IsCancelled(container_id)) {
    return;
  }
  auto callback = std::move(configuration_tasks_[container_id]->callback);
  configuration_tasks_.erase(configuration_tasks_.find(container_id));

  ui_elements_[container_id]->CloseWithReason(
      views::Widget::ClosedReason::kUnspecified);
  ui_elements_.erase(container_id);

  // Clean up our observer if no more packages are awaiting this.
  if (configuration_tasks_.empty()) {
    CrostiniManager::GetForProfile(profile_)
        ->RemoveLinuxPackageOperationProgressObserver(this);
  }
  std::move(callback).Run(success);
}

bool AnsibleManagementService::IsCancelled(
    const guest_os::GuestId& container_id) {
  return configuration_tasks_.count(container_id) == 0;
}

}  // namespace crostini

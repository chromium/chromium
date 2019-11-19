// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/ansible/ansible_management_service.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/chromeos/crostini/crostini_manager_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/pref_service.h"

namespace crostini {

namespace {

class AnsibleManagementServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AnsibleManagementService* GetForProfile(Profile* profile) {
    return static_cast<AnsibleManagementService*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static AnsibleManagementServiceFactory* GetInstance() {
    static base::NoDestructor<AnsibleManagementServiceFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<AnsibleManagementServiceFactory>;

  AnsibleManagementServiceFactory()
      : BrowserContextKeyedServiceFactory(
            "AnsibleManagementService",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(CrostiniManagerFactory::GetInstance());
  }

  ~AnsibleManagementServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    return new AnsibleManagementService(profile);
  }
};

chromeos::CiceroneClient* GetCiceroneClient() {
  return chromeos::DBusThreadManager::Get()->GetCiceroneClient();
}

}  // namespace

AnsibleManagementService* AnsibleManagementService::GetForProfile(
    Profile* profile) {
  return AnsibleManagementServiceFactory::GetForProfile(profile);
}

AnsibleManagementService::AnsibleManagementService(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}
AnsibleManagementService::~AnsibleManagementService() = default;

void AnsibleManagementService::ConfigureDefaultContainer(
    base::OnceCallback<void(bool success)> callback) {
  DCHECK(!configuration_finished_callback_);
  configuration_finished_callback_ = std::move(callback);

  // TODO(okalitova): Reflect configuration progress in installer view when
  // Crostini is being installed.

  // Popup dialog is shown in case Crostini has already been installed.
  if (!CrostiniManager::GetForProfile(profile_)->GetInstallerViewStatus())
    ShowCrostiniAnsibleSoftwareConfigView(profile_);

  CrostiniManager::GetForProfile(profile_)
      ->AddLinuxPackageOperationProgressObserver(this);

  CrostiniManager::GetForProfile(profile_)->InstallLinuxPackageFromApt(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      kCrostiniDefaultAnsibleVersion,
      base::BindOnce(
          &AnsibleManagementService::OnInstallAnsibleInDefaultContainer,
          weak_ptr_factory_.GetWeakPtr()));
}

void AnsibleManagementService::OnInstallAnsibleInDefaultContainer(
    CrostiniResult result) {
  if (result == CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED) {
    LOG(ERROR) << "Ansible installation failed";
    OnConfigurationFinished(false);
    return;
  }

  DCHECK_NE(result, CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE);

  DCHECK_EQ(result, CrostiniResult::SUCCESS);
  VLOG(1) << "Ansible installation has been started successfully";
  // Waiting for Ansible installation progress being reported.
}

void AnsibleManagementService::OnInstallLinuxPackageProgress(
    const ContainerId& container_id,
    InstallLinuxPackageProgressStatus status,
    int progress_percent) {
  DCHECK_EQ(container_id.vm_name, kCrostiniDefaultVmName);
  DCHECK_EQ(container_id.container_name, kCrostiniDefaultContainerName);

  switch (status) {
    case InstallLinuxPackageProgressStatus::SUCCEEDED: {
      CrostiniManager::GetForProfile(profile_)
          ->RemoveLinuxPackageOperationProgressObserver(this);
      GetAnsiblePlaybookToApply();
      return;
    }
    case InstallLinuxPackageProgressStatus::FAILED:
      LOG(ERROR) << "Ansible installation failed";
      CrostiniManager::GetForProfile(profile_)
          ->RemoveLinuxPackageOperationProgressObserver(this);
      OnConfigurationFinished(false);
      return;
    // TODO(okalitova): Report Ansible downloading/installation progress.
    case InstallLinuxPackageProgressStatus::DOWNLOADING:
      VLOG(1) << "Ansible downloading progress: " << progress_percent << "%";
      return;
    case InstallLinuxPackageProgressStatus::INSTALLING:
      VLOG(1) << "Ansible installing progress: " << progress_percent << "%";
      return;
    default:
      NOTREACHED();
  }
}

void AnsibleManagementService::GetAnsiblePlaybookToApply() {
  const base::FilePath& ansible_playbook_file_path =
      profile_->GetPrefs()->GetFilePath(
          prefs::kCrostiniAnsiblePlaybookFilePath);
  bool success = base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(base::ReadFileToString, ansible_playbook_file_path,
                     &playbook_),
      base::BindOnce(&AnsibleManagementService::OnAnsiblePlaybookRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
  if (!success) {
    LOG(ERROR) << "Failed to post task to retrieve Ansible playbook content";
    OnConfigurationFinished(false);
  }
}

void AnsibleManagementService::OnAnsiblePlaybookRetrieved(bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to retrieve Ansible playbook content";
    OnConfigurationFinished(false);
    return;
  }

  ApplyAnsiblePlaybookToDefaultContainer();
}

void AnsibleManagementService::ApplyAnsiblePlaybookToDefaultContainer() {
  if (!GetCiceroneClient()->IsApplyAnsiblePlaybookProgressSignalConnected()) {
    // Technically we could still start the application, but we wouldn't be able
    // to detect when the application completes, successfully or otherwise.
    LOG(ERROR)
        << "Attempted to apply playbook when progress signal not connected.";
    OnConfigurationFinished(false);
    return;
  }

  vm_tools::cicerone::ApplyAnsiblePlaybookRequest request;
  request.set_owner_id(CryptohomeIdForProfile(profile_));
  request.set_vm_name(std::move(kCrostiniDefaultVmName));
  request.set_container_name(std::move(kCrostiniDefaultContainerName));
  request.set_playbook(std::move(playbook_));

  GetCiceroneClient()->ApplyAnsiblePlaybook(
      std::move(request),
      base::BindOnce(&AnsibleManagementService::OnApplyAnsiblePlaybook,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AnsibleManagementService::OnApplyAnsiblePlaybook(
    base::Optional<vm_tools::cicerone::ApplyAnsiblePlaybookResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to apply Ansible playbook. Empty response.";
    OnConfigurationFinished(false);
    return;
  }

  if (response->status() ==
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::FAILED) {
    LOG(ERROR) << "Failed to apply Ansible playbook: "
               << response->failure_reason();
    OnConfigurationFinished(false);
    return;
  }

  VLOG(1) << "Ansible playbook application has been started successfully";
  // Waiting for Ansible playbook application progress being reported.
}

void AnsibleManagementService::OnApplyAnsiblePlaybookProgress(
    vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::Status status) {
  switch (status) {
    case vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::SUCCEEDED:
      OnConfigurationFinished(true);
      break;
    case vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::FAILED:
      LOG(ERROR) << "Ansible playbook application has failed";
      OnConfigurationFinished(false);
      break;
    case vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::IN_PROGRESS:
      // TODO(okalitova): Report Ansible playbook application progress.
      break;
    default:
      NOTREACHED();
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
    const ContainerId& container_id,
    UninstallPackageProgressStatus status,
    int progress_percent) {
  NOTIMPLEMENTED();
}

void AnsibleManagementService::OnConfigurationFinished(bool success) {
  DCHECK(configuration_finished_callback_);
  std::move(configuration_finished_callback_).Run(success);
  for (auto& observer : observers_) {
    observer.OnAnsibleSoftwareConfigurationFinished(success);
  }
}

}  // namespace crostini

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_package_installer_service.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "chrome/browser/chromeos/crostini/crostini_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace crostini {

namespace {

class CrostiniPackageInstallerServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static CrostiniPackageInstallerService* GetForProfile(Profile* profile) {
    return static_cast<CrostiniPackageInstallerService*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static CrostiniPackageInstallerServiceFactory* GetInstance() {
    static base::NoDestructor<CrostiniPackageInstallerServiceFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<CrostiniPackageInstallerServiceFactory>;

  CrostiniPackageInstallerServiceFactory()
      : BrowserContextKeyedServiceFactory(
            "CrostiniPackageInstallerService",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(CrostiniManagerFactory::GetInstance());
  }

  ~CrostiniPackageInstallerServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    return new CrostiniPackageInstallerService(profile);
  }
};

}  // namespace

CrostiniPackageInstallerService* CrostiniPackageInstallerService::GetForProfile(
    Profile* profile) {
  return CrostiniPackageInstallerServiceFactory::GetForProfile(profile);
}

CrostiniPackageInstallerService::CrostiniPackageInstallerService(
    Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {
  CrostiniManager::GetForProfile(profile)
      ->AddInstallLinuxPackageProgressObserver(this);
}

CrostiniPackageInstallerService::~CrostiniPackageInstallerService() = default;

void CrostiniPackageInstallerService::Shutdown() {
  CrostiniManager::GetForProfile(profile_)
      ->RemoveInstallLinuxPackageProgressObserver(this);
}

void CrostiniPackageInstallerService::NotificationClosed(
    CrostiniPackageInstallerNotification* notification) {
  for (auto it = running_notifications_.begin();
       it != running_notifications_.end(); ++it) {
    if (it->second.get() == notification) {
      running_notifications_.erase(it);
      return;
    }
  }

  for (auto it = finished_notifications_.begin();
       it != finished_notifications_.end(); ++it) {
    if (it->get() == notification) {
      finished_notifications_.erase(it);
      return;
    }
  }

  NOTREACHED();
}

void CrostiniPackageInstallerService::GetLinuxPackageInfo(
    const std::string& vm_name,
    const std::string& container_name,
    const std::string& package_path,
    CrostiniManager::GetLinuxPackageInfoCallback callback) {
  CrostiniManager::GetForProfile(profile_)->GetLinuxPackageInfo(
      profile_, vm_name, container_name, package_path,
      base::BindOnce(&CrostiniPackageInstallerService::OnGetLinuxPackageInfo,
                     weak_ptr_factory_.GetWeakPtr(), vm_name, container_name,
                     std::move(callback)));
}

void CrostiniPackageInstallerService::InstallLinuxPackage(
    const std::string& vm_name,
    const std::string& container_name,
    const std::string& package_path,
    CrostiniManager::InstallLinuxPackageCallback callback) {
  CrostiniManager::GetForProfile(profile_)->InstallLinuxPackage(
      vm_name, container_name, package_path,
      base::BindOnce(&CrostiniPackageInstallerService::OnInstallLinuxPackage,
                     weak_ptr_factory_.GetWeakPtr(), vm_name, container_name,
                     std::move(callback)));
}

void CrostiniPackageInstallerService::OnInstallLinuxPackageProgress(
    const std::string& vm_name,
    const std::string& container_name,
    InstallLinuxPackageProgressStatus result,
    int progress_percent,
    const std::string& failure_reason) {
  auto it =
      running_notifications_.find(std::make_pair(vm_name, container_name));
  if (it == running_notifications_.end())
    return;
  it->second->UpdateProgress(result, progress_percent, failure_reason);

  if (result == InstallLinuxPackageProgressStatus::SUCCEEDED ||
      result == InstallLinuxPackageProgressStatus::FAILED) {
    finished_notifications_.emplace_back(std::move(it->second));
    running_notifications_.erase(it);
  }
}

void CrostiniPackageInstallerService::OnGetLinuxPackageInfo(
    const std::string& vm_name,
    const std::string& container_name,
    CrostiniManager::GetLinuxPackageInfoCallback callback,
    const LinuxPackageInfo& linux_package_info) {
  std::move(callback).Run(linux_package_info);
  if (!linux_package_info.success)
    return;
}

void CrostiniPackageInstallerService::OnInstallLinuxPackage(
    const std::string& vm_name,
    const std::string& container_name,
    CrostiniManager::InstallLinuxPackageCallback callback,
    CrostiniResult result,
    const std::string& failure_reason) {
  std::move(callback).Run(result, failure_reason);
  if (result != CrostiniResult::SUCCESS)
    return;

  std::unique_ptr<CrostiniPackageInstallerNotification>& notification =
      running_notifications_[std::make_pair(vm_name, container_name)];
  if (notification) {
    // We could reach this if the final progress update signal from a previous
    // package install doesn't get sent, so we wouldn't end up moving the
    // previous notification out of running_notifications_.
    LOG(ERROR) << "Notification for package install already exists.";
    return;
  }

  notification = std::make_unique<CrostiniPackageInstallerNotification>(
      profile_, GetUniqueNotificationId(), this);
}

std::string CrostiniPackageInstallerService::GetUniqueNotificationId() {
  return base::StringPrintf("crostini_package_install_%d",
                            next_notification_id++);
}

}  // namespace crostini

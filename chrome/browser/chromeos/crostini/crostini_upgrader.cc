// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_upgrader.h"

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/crostini/crostini_export_import.h"
#include "chrome/browser/chromeos/crostini/crostini_export_import_status_tracker.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace crostini {

namespace {

class CrostiniUpgraderFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CrostiniUpgrader* GetForProfile(Profile* profile) {
    return static_cast<CrostiniUpgrader*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static CrostiniUpgraderFactory* GetInstance() {
    static base::NoDestructor<CrostiniUpgraderFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<CrostiniUpgraderFactory>;

  CrostiniUpgraderFactory()
      : BrowserContextKeyedServiceFactory(
            "CrostiniUpgraderService",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(CrostiniManagerFactory::GetInstance());
  }

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    return new CrostiniUpgrader(profile);
  }
};
}  // namespace

CrostiniUpgrader* CrostiniUpgrader::GetForProfile(Profile* profile) {
  return CrostiniUpgraderFactory::GetForProfile(profile);
}

CrostiniUpgrader::CrostiniUpgrader(Profile* profile)
    : profile_(profile),
      container_id_("", ""),
      pmc_observer_(this),
      backup_path_(base::nullopt) {
  CrostiniManager::GetForProfile(profile_)->AddUpgradeContainerProgressObserver(
      this);
}

CrostiniUpgrader::~CrostiniUpgrader() = default;

void CrostiniUpgrader::Shutdown() {
  CrostiniManager::GetForProfile(profile_)
      ->RemoveUpgradeContainerProgressObserver(this);
  upgrader_observers_.Clear();
}

void CrostiniUpgrader::AddObserver(CrostiniUpgraderUIObserver* observer) {
  upgrader_observers_.AddObserver(observer);
}

void CrostiniUpgrader::RemoveObserver(CrostiniUpgraderUIObserver* observer) {
  upgrader_observers_.RemoveObserver(observer);
}

CrostiniUpgrader::StatusTracker::StatusTracker(
    base::WeakPtr<CrostiniUpgrader> upgrader,
    ExportImportType type,
    base::FilePath path)
    : CrostiniExportImportStatusTracker(type, std::move(path)),
      upgrader_(upgrader) {}

CrostiniUpgrader::StatusTracker::~StatusTracker() = default;

void CrostiniUpgrader::StatusTracker::SetStatusRunningUI(int progress_percent) {
  if (type() == ExportImportType::EXPORT) {
    upgrader_->OnBackupProgress(progress_percent);
  } else {
    upgrader_->OnRestoreProgress(progress_percent);
  }
  if (has_notified_start_)
    return;
  for (auto& observer : upgrader_->upgrader_observers_) {
    observer.OnBackupMaybeStarted(/*did_start=*/true);
  }
  has_notified_start_ = true;
}

void CrostiniUpgrader::StatusTracker::SetStatusDoneUI() {
  if (type() == ExportImportType::EXPORT) {
    upgrader_->OnBackup(CrostiniResult::SUCCESS, path());
  } else {
    upgrader_->OnRestore(CrostiniResult::SUCCESS);
  }
}

void CrostiniUpgrader::StatusTracker::SetStatusCancelledUI() {
  // Cancelling the restore results in "success" i.e. we successfully didn't try
  // to restore. Cancelling the backup is a no-op that returns you to the
  // original screen.
  if (type() == ExportImportType::IMPORT) {
    upgrader_->OnRestore(CrostiniResult::SUCCESS);
  }
  for (auto& observer : upgrader_->upgrader_observers_) {
    observer.OnBackupMaybeStarted(/*did_start=*/false);
  }
}

void CrostiniUpgrader::StatusTracker::SetStatusFailedWithMessageUI(
    Status status,
    const std::u16string& message) {
  CrostiniResult result = CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED;
  if (status == Status::FAILED_INSUFFICIENT_SPACE) {
    result = CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_SPACE;
  }
  if (type() == ExportImportType::EXPORT) {
    upgrader_->OnBackup(result, base::nullopt);
  } else {
    upgrader_->OnRestore(result);
  }
}

void CrostiniUpgrader::Backup(const ContainerId& container_id,
                              bool show_file_chooser,
                              content::WebContents* web_contents) {
  if (show_file_chooser) {
    CrostiniExportImport::GetForProfile(profile_)->ExportContainer(
        container_id, web_contents, MakeFactory());
    return;
  }
  base::FilePath default_path =
      CrostiniExportImport::GetForProfile(profile_)->GetDefaultBackupPath();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::PathExists, default_path),
      base::BindOnce(&CrostiniUpgrader::OnBackupPathChecked,
                     weak_ptr_factory_.GetWeakPtr(), container_id, web_contents,
                     default_path));
}

void CrostiniUpgrader::OnBackupPathChecked(const ContainerId& container_id,
                                           content::WebContents* web_contents,
                                           base::FilePath path,
                                           bool path_exists) {
  if (path_exists) {
    CrostiniExportImport::GetForProfile(profile_)->ExportContainer(
        container_id, web_contents, MakeFactory());
  } else {
    CrostiniExportImport::GetForProfile(profile_)->ExportContainer(
        container_id, path, MakeFactory());
  }
}

void CrostiniUpgrader::OnBackup(CrostiniResult result,
                                base::Optional<base::FilePath> backup_path) {
  if (result != CrostiniResult::SUCCESS) {
    for (auto& observer : upgrader_observers_) {
      observer.OnBackupFailed();
    }
    return;
  }
  backup_path_ = backup_path;
  for (auto& observer : upgrader_observers_) {
    observer.OnBackupSucceeded(!backup_path.has_value());
  }
}

void CrostiniUpgrader::OnBackupProgress(int progress_percent) {
  for (auto& observer : upgrader_observers_) {
    observer.OnBackupProgress(progress_percent);
  }
}

void CrostiniUpgrader::StartPrechecks() {
  auto* pmc = chromeos::PowerManagerClient::Get();
  if (pmc_observer_.IsObserving(pmc)) {
    // This could happen if two StartPrechecks were run at the same time. If it
    // does, drop the second call.
    return;
  }

  prechecks_callback_ =
      base::BarrierClosure(2, /* Number of asynchronous prechecks to wait for */
                           base::BindOnce(&CrostiniUpgrader::DoPrechecks,
                                          weak_ptr_factory_.GetWeakPtr()));

  pmc_observer_.Add(pmc);
  pmc->RequestStatusUpdate();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                     base::FilePath(crostini::kHomeDirectory)),
      base::BindOnce(&CrostiniUpgrader::OnAvailableDiskSpace,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniUpgrader::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  // A battery can be FULL, CHARGING, DISCHARGING, or NOT_PRESENT. If we're on a
  // system with no battery, we can assume stable power from the fact that we
  // are running at all. Otherwise we want the battery to be full or charging. A
  // less conservative check is possible, but we can expect users to have access
  // to a charger.
  power_status_good_ = proto.battery_state() !=
                           power_manager::PowerSupplyProperties::DISCHARGING ||
                       proto.external_power() !=
                           power_manager::PowerSupplyProperties::DISCONNECTED;

  auto* pmc = chromeos::PowerManagerClient::Get();
  pmc_observer_.Remove(pmc);

  prechecks_callback_.Run();
}

void CrostiniUpgrader::OnAvailableDiskSpace(int64_t bytes) {
  free_disk_space_ = bytes;

  prechecks_callback_.Run();
}

void CrostiniUpgrader::DoPrechecks() {
  chromeos::crostini_upgrader::mojom::UpgradePrecheckStatus status;
  if (free_disk_space_ < kDiskRequired) {
    status = chromeos::crostini_upgrader::mojom::UpgradePrecheckStatus::
        INSUFFICIENT_SPACE;
  } else if (content::GetNetworkConnectionTracker()->IsOffline()) {
    status = chromeos::crostini_upgrader::mojom::UpgradePrecheckStatus::
        NETWORK_FAILURE;
  } else if (!power_status_good_) {
    status =
        chromeos::crostini_upgrader::mojom::UpgradePrecheckStatus::LOW_POWER;
  } else {
    status = chromeos::crostini_upgrader::mojom::UpgradePrecheckStatus::OK;
  }

  for (auto& observer : upgrader_observers_) {
    observer.PrecheckStatus(status);
  }
}

void CrostiniUpgrader::Upgrade(const ContainerId& container_id) {
  container_id_ = container_id;

  // Shut down the existing VM then upgrade. StopVm doesn't give an error if
  // the VM doesn't exist. That's fine.
  CrostiniManager::GetForProfile(profile_)->StopVm(
      container_id.vm_name,
      base::BindOnce(
          [](base::WeakPtr<CrostiniUpgrader> weak_this, CrostiniResult result) {
            if (!weak_this) {
              return;
            }
            if (result != CrostiniResult::SUCCESS) {
              LOG(ERROR) << "Unable to shut down vm before upgrade";
              weak_this->OnUpgrade(result);
              return;
            }

            CrostiniManager::GetForProfile(weak_this->profile_)
                ->UpgradeContainer(
                    weak_this->container_id_, ContainerVersion::STRETCH,
                    ContainerVersion::BUSTER,
                    base::BindOnce(&CrostiniUpgrader::OnUpgrade, weak_this));
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniUpgrader::OnUpgrade(CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "OnUpgrade result " << static_cast<int>(result);
    for (auto& observer : upgrader_observers_) {
      observer.OnUpgradeFailed();
    }
    return;
  }
}

void CrostiniUpgrader::Restore(const ContainerId& container_id,
                               content::WebContents* web_contents) {
  if (!backup_path_.has_value()) {
    CrostiniExportImport::GetForProfile(profile_)->ImportContainer(
        container_id, web_contents, MakeFactory());
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::PathExists, *backup_path_),
      base::BindOnce(&CrostiniUpgrader::OnRestorePathChecked,
                     weak_ptr_factory_.GetWeakPtr(), container_id, web_contents,
                     *backup_path_));
}

void CrostiniUpgrader::OnRestorePathChecked(const ContainerId& container_id,
                                            content::WebContents* web_contents,
                                            base::FilePath path,
                                            bool path_exists) {
  if (!path_exists) {
    CrostiniExportImport::GetForProfile(profile_)->ImportContainer(
        container_id, web_contents, MakeFactory());
  } else {
    CrostiniExportImport::GetForProfile(profile_)->ImportContainer(
        container_id, path, MakeFactory());
  }
}

void CrostiniUpgrader::OnRestore(CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    for (auto& observer : upgrader_observers_) {
      observer.OnRestoreFailed();
    }
    return;
  }
  for (auto& observer : upgrader_observers_) {
    observer.OnRestoreSucceeded();
  }
}

void CrostiniUpgrader::OnRestoreProgress(int progress_percent) {
  for (auto& observer : upgrader_observers_) {
    observer.OnRestoreProgress(progress_percent);
  }
}

void CrostiniUpgrader::Cancel() {
  CrostiniManager::GetForProfile(profile_)->CancelUpgradeContainer(
      container_id_, base::BindOnce(&CrostiniUpgrader::OnCancel,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniUpgrader::OnCancel(CrostiniResult result) {
  for (auto& observer : upgrader_observers_) {
    observer.OnCanceled();
  }
}

void CrostiniUpgrader::CancelBeforeStart() {
  for (auto& observer : upgrader_observers_) {
    observer.OnCanceled();
  }
}

void CrostiniUpgrader::OnUpgradeContainerProgress(
    const ContainerId& container_id,
    UpgradeContainerProgressStatus status,
    const std::vector<std::string>& messages) {
  if (container_id != container_id_) {
    return;
  }
  switch (status) {
    case UpgradeContainerProgressStatus::UPGRADING:
      for (auto& observer : upgrader_observers_) {
        observer.OnUpgradeProgress(messages);
      }
      break;
    case UpgradeContainerProgressStatus::SUCCEEDED:
      for (auto& observer : upgrader_observers_) {
        observer.OnUpgradeProgress(messages);
        observer.OnUpgradeSucceeded();
      }
      break;
    case UpgradeContainerProgressStatus::FAILED:
      for (auto& observer : upgrader_observers_) {
        observer.OnUpgradeProgress(messages);
        observer.OnUpgradeFailed();
      }
      break;
  }
}

// Return true if internal state allows starting upgrade.
bool CrostiniUpgrader::CanUpgrade() {
  return false;
}

CrostiniExportImport::OnceTrackerFactory CrostiniUpgrader::MakeFactory() {
  return base::BindOnce(
      [](base::WeakPtr<CrostiniUpgrader> upgrader, ExportImportType type,
         base::FilePath path)
          -> std::unique_ptr<CrostiniExportImportStatusTracker> {
        return std::make_unique<StatusTracker>(std::move(upgrader), type,
                                               std::move(path));
      },
      weak_ptr_factory_.GetWeakPtr());
}

}  // namespace crostini

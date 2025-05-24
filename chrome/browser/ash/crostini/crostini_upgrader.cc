// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_upgrader.h"

#include <optional>

#include "base/files/file_util.h"
#include "base/location.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crostini/crostini_export_import.h"
#include "chrome/browser/ash/crostini/crostini_export_import_factory.h"
#include "chrome/browser/ash/crostini/crostini_export_import_status_tracker.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/crostini_upgrader/crostini_upgrader.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace crostini {

namespace {

const char kLogFileBasename[] = "container_upgrade.log";

}  // namespace

CrostiniUpgrader::CrostiniUpgrader(Profile* profile)
    : profile_(profile),
      container_id_(kCrostiniDefaultVmType, "", ""),
      log_sequence_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      current_log_file_(std::nullopt),
      backup_path_(std::nullopt) {
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

void CrostiniUpgrader::PageOpened() {
  // Clear log path so any log messages get buffered.
  current_log_file_ = std::nullopt;
  // Clear the buffer, which may have been previously moved from.
  log_buffer_ = std::vector<std::string>();
}

void CrostiniUpgrader::CreateNewLogFile() {
  base::FilePath path =
      file_manager::util::GetMyFilesFolderForProfile(profile_).Append(
          kLogFileBasename);
  // Create the new log file on the blocking threadpool.
  log_sequence_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](base::FilePath path) -> std::optional<base::FilePath> {
            path = base::GetUniquePath(path);
            base::File file(path,
                            base::File::FLAG_READ | base::File::FLAG_CREATE);
            if (!file.IsValid()) {
              PLOG(ERROR) << "Failed to create log file!";
              return std::nullopt;
            }
            return path;
          },
          path),
      // Once the file is created, write out the buffered log messages.
      base::BindOnce(
          [](base::WeakPtr<CrostiniUpgrader> weak_this,
             std::optional<base::FilePath> path) {
            if (!weak_this) {
              return;
            }

            weak_this->current_log_file_ = path;
            if (path) {
              weak_this->WriteLogMessages(std::move(weak_this->log_buffer_));
              for (auto& observer : weak_this->upgrader_observers_) {
                observer.OnLogFileCreated(path->BaseName());
              }
            }
          },
          weak_ptr_factory_.GetWeakPtr()));
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
  if (has_notified_start_) {
    return;
  }
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
    upgrader_->OnBackup(result, std::nullopt);
  } else {
    upgrader_->OnRestore(result);
  }
}

void CrostiniUpgrader::Backup(
    const guest_os::GuestId& container_id,
    bool show_file_chooser,
    base::WeakPtr<content::WebContents> web_contents) {
  if (show_file_chooser) {
    CrostiniExportImportFactory::GetForProfile(profile_)->ExportContainer(
        container_id, web_contents.get(), MakeFactory());
    return;
  }
  base::FilePath default_path =
      CrostiniExportImportFactory::GetForProfile(profile_)
          ->GetDefaultBackupPath();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::PathExists, default_path),
      base::BindOnce(&CrostiniUpgrader::OnBackupPathChecked,
                     weak_ptr_factory_.GetWeakPtr(), container_id, web_contents,
                     default_path));
}

void CrostiniUpgrader::OnBackupPathChecked(
    const guest_os::GuestId& container_id,
    base::WeakPtr<content::WebContents> web_contents,
    base::FilePath path,
    bool path_exists) {
  if (!web_contents) {
    // Page has been closed, don't continue
    return;
  }

  if (path_exists) {
    CrostiniExportImportFactory::GetForProfile(profile_)->ExportContainer(
        container_id, web_contents.get(), MakeFactory());
  } else {
    CrostiniExportImportFactory::GetForProfile(profile_)->ExportContainer(
        container_id, path, MakeFactory());
  }
}

void CrostiniUpgrader::OnBackup(CrostiniResult result,
                                std::optional<base::FilePath> backup_path) {
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
  if (pmc_observation_.IsObservingSource(pmc)) {
    // This could happen if two StartPrechecks were run at the same time. If it
    // does, drop the second call.
    return;
  }

  prechecks_callback_ = base::BindOnce(&CrostiniUpgrader::DoPrechecks,
                                       weak_ptr_factory_.GetWeakPtr());

  pmc_observation_.Observe(pmc);
  pmc->RequestStatusUpdate();
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
  DCHECK(pmc_observation_.IsObservingSource(pmc));
  pmc_observation_.Reset();

  if (prechecks_callback_) {
    std::move(prechecks_callback_).Run();
  }
}

void CrostiniUpgrader::DoPrechecks() {
  ash::crostini_upgrader::mojom::UpgradePrecheckStatus status;
  if (content::GetNetworkConnectionTracker()->IsOffline()) {
    status =
        ash::crostini_upgrader::mojom::UpgradePrecheckStatus::NETWORK_FAILURE;
  } else if (!power_status_good_) {
    status = ash::crostini_upgrader::mojom::UpgradePrecheckStatus::LOW_POWER;
  } else {
    status = ash::crostini_upgrader::mojom::UpgradePrecheckStatus::OK;
  }

  for (auto& observer : upgrader_observers_) {
    observer.PrecheckStatus(status);
  }
}

void CrostiniUpgrader::Upgrade(const guest_os::GuestId& container_id) {
  container_id_ = container_id;

  if (!current_log_file_.has_value()) {
    CreateNewLogFile();
  }
  OnUpgradeContainerProgress(container_id,
                             UpgradeContainerProgressStatus::UPGRADING,
                             {"---- START OF UPGRADE ----"});

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

            auto target_version = ContainerVersion::BOOKWORM;

            CrostiniManager::GetForProfile(weak_this->profile_)
                ->UpgradeContainer(
                    weak_this->container_id_, target_version,
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

void CrostiniUpgrader::Restore(
    const guest_os::GuestId& container_id,
    base::WeakPtr<content::WebContents> web_contents) {
  if (!backup_path_.has_value()) {
    CrostiniExportImportFactory::GetForProfile(profile_)->ImportContainer(
        container_id, web_contents.get(), MakeFactory());
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::PathExists, *backup_path_),
      base::BindOnce(&CrostiniUpgrader::OnRestorePathChecked,
                     weak_ptr_factory_.GetWeakPtr(), container_id, web_contents,
                     *backup_path_));
}

void CrostiniUpgrader::OnRestorePathChecked(
    const guest_os::GuestId& container_id,
    base::WeakPtr<content::WebContents> web_contents,
    base::FilePath path,
    bool path_exists) {
  if (!web_contents) {
    // Page has been closed, don't continue
    return;
  }

  if (!path_exists) {
    CrostiniExportImportFactory::GetForProfile(profile_)->ImportContainer(
        container_id, web_contents.get(), MakeFactory());
  } else {
    CrostiniExportImportFactory::GetForProfile(profile_)->ImportContainer(
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
    const guest_os::GuestId& container_id,
    UpgradeContainerProgressStatus status,
    const std::vector<std::string>& messages) {
  if (container_id != container_id_) {
    return;
  }

  // Write `messages` to the log file, or append it to the buffer if the log
  // file is still pending.
  if (current_log_file_) {
    WriteLogMessages(messages);
  } else {
    log_buffer_.insert(log_buffer_.end(), messages.begin(), messages.end());
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

void CrostiniUpgrader::WriteLogMessages(std::vector<std::string> messages) {
  log_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::FilePath path, std::vector<std::string> messages) {
            for (const std::string& s : messages) {
              if (!base::AppendToFile(path, s + "\n")) {
                PLOG(ERROR) << "Failed to write logs";
              }
            }
          },
          *current_log_file_, std::move(messages)));
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

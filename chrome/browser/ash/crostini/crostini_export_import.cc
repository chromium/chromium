// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_export_import.h"

#include <algorithm>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace crostini {

CrostiniExportImport::CrostiniExportImport(Profile* profile)
    : profile_(profile) {
  CrostiniManager* manager = CrostiniManager::GetForProfile(profile_);
  manager->AddExportContainerProgressObserver(this);
  manager->AddImportContainerProgressObserver(this);
  manager->AddDiskImageProgressObserver(this);
}

CrostiniExportImport::~CrostiniExportImport() {
  if (select_folder_dialog_) {
    /* Lifecycle for SelectFileDialog is responsibility of calling code. */
    select_folder_dialog_->ListenerDestroyed();
  }
}

void CrostiniExportImport::Shutdown() {
  CrostiniManager* manager = CrostiniManager::GetForProfile(profile_);
  manager->RemoveExportContainerProgressObserver(this);
  manager->RemoveImportContainerProgressObserver(this);
  manager->RemoveDiskImageProgressObserver(this);
}

CrostiniExportImport::OperationData::OperationData(
    ExportImportType type,
    guest_os::GuestId container_id,
    OnceTrackerFactory tracker_factory)
    : type(type),
      container_id(std::move(container_id)),
      tracker_factory(std::move(tracker_factory)) {}

CrostiniExportImport::OperationData::~OperationData() = default;

void CrostiniExportImport::FillOperationData(ExportImportType type,
                                             guest_os::GuestId container_id,
                                             OnceTrackerFactory factory) {
  CHECK(!operation_data_);
  operation_data_ = std::make_unique<OperationData>(
      type, std::move(container_id), std::move(factory));
}

void CrostiniExportImport::FillOperationData(ExportImportType type,
                                             guest_os::GuestId container_id) {
  OnceTrackerFactory factory = base::BindOnce(
      [](Profile* profile, guest_os::GuestId container_id,
         std::string notification_id, ExportImportType type,
         base::FilePath path)
          -> std::unique_ptr<CrostiniExportImportStatusTracker> {
        return std::make_unique<CrostiniExportImportNotificationController>(
            profile, type, std::move(notification_id), std::move(path),
            std::move(container_id));
      },
      profile_, container_id, GetUniqueNotificationId());
  FillOperationData(type, std::move(container_id), std::move(factory));
}

void CrostiniExportImport::FillOperationData(ExportImportType type) {
  FillOperationData(type, DefaultContainerId());
}

void CrostiniExportImport::ExportContainer(guest_os::GuestId container_id,
                                           content::WebContents* web_contents) {
  FillOperationData(ExportImportType::EXPORT, std::move(container_id));
  OpenFileDialog(web_contents);
}

void CrostiniExportImport::ImportContainer(guest_os::GuestId container_id,
                                           content::WebContents* web_contents) {
  FillOperationData(ExportImportType::IMPORT, std::move(container_id));
  OpenFileDialog(web_contents);
}

void CrostiniExportImport::ExportContainer(guest_os::GuestId container_id,
                                           content::WebContents* web_contents,
                                           OnceTrackerFactory tracker_factory) {
  FillOperationData(ExportImportType::EXPORT, std::move(container_id),
                    std::move(tracker_factory));
  OpenFileDialog(web_contents);
}

void CrostiniExportImport::ImportContainer(guest_os::GuestId container_id,
                                           content::WebContents* web_contents,
                                           OnceTrackerFactory tracker_factory) {
  FillOperationData(ExportImportType::IMPORT, std::move(container_id),
                    std::move(tracker_factory));
  OpenFileDialog(web_contents);
}

base::FilePath CrostiniExportImport::GetDefaultBackupPath() const {
  return file_manager::util::GetMyFilesFolderForProfile(profile_).Append(
      base::UnlocalizedTimeFormatWithPattern(
          base::Time::Now(), "'chromeos-linux-'yyyy-MM-dd'.tini'"));
}

void CrostiniExportImport::OpenFileDialog(content::WebContents* web_contents) {
  if (!crostini::CrostiniFeatures::Get()->IsExportImportUIAllowed(profile_)) {
    return;
  }
  // Early return if the select file dialog is already active.
  if (select_folder_dialog_) {
    return;
  }

  CHECK(operation_data_);

  ui::SelectFileDialog::Type file_selector_mode;
  unsigned title = 0;
  base::FilePath default_path;
  const ui::SelectFileDialog::FileTypeInfo file_types{
      {FILE_PATH_LITERAL("tini"), FILE_PATH_LITERAL("tar.gz"),
       FILE_PATH_LITERAL("tgz")}};

  switch (operation_data_->type) {
    case ExportImportType::EXPORT:
      file_selector_mode = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
      title = IDS_SETTINGS_CROSTINI_EXPORT;
      default_path = GetDefaultBackupPath();
      break;
    case ExportImportType::IMPORT:
      file_selector_mode = ui::SelectFileDialog::SELECT_OPEN_FILE,
      title = IDS_SETTINGS_CROSTINI_IMPORT;
      default_path = file_manager::util::GetMyFilesFolderForProfile(profile_);
      break;
    case ExportImportType::EXPORT_DISK_IMAGE:
      file_selector_mode = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
      title = IDS_SETTINGS_CROSTINI_EXPORT;
      default_path = GetDefaultBackupPath();
      break;
    case ExportImportType::IMPORT_DISK_IMAGE:
      file_selector_mode = ui::SelectFileDialog::SELECT_OPEN_FILE,
      title = IDS_SETTINGS_CROSTINI_IMPORT;
      default_path = file_manager::util::GetMyFilesFolderForProfile(profile_);
      break;
  }

  select_folder_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));
  select_folder_dialog_->SelectFile(
      file_selector_mode, l10n_util::GetStringUTF16(title), default_path,
      &file_types, 0, base::FilePath::StringType(),
      web_contents->GetTopLevelNativeWindow());
}

void CrostiniExportImport::FileSelected(const ui::SelectedFileInfo& file,
                                        int index) {
  Start(file.path(), /* create_new_container= */ false, base::DoNothing());
  select_folder_dialog_.reset();
}

void CrostiniExportImport::FileSelectionCanceled() {
  if (operation_data_->tracker_factory) {
    // Create the status tracker so we can let it know the operation was
    // canceled.
    auto status_tracker = std::move(operation_data_->tracker_factory)
                              .Run(operation_data_->type, base::FilePath());
    status_tracker->SetStatusCancelled();
  }
  operation_data_.reset();
  select_folder_dialog_.reset();
}

void CrostiniExportImport::ExportContainer(
    guest_os::GuestId container_id,
    base::FilePath path,
    CrostiniManager::CrostiniResultCallback callback) {
  FillOperationData(ExportImportType::EXPORT, std::move(container_id));
  Start(path, /* create_new_container= */ false, std::move(callback));
}

void CrostiniExportImport::ImportContainer(
    guest_os::GuestId container_id,
    base::FilePath path,
    CrostiniManager::CrostiniResultCallback callback) {
  std::vector<guest_os::GuestId> existing_containers =
      guest_os::GetContainers(profile_, guest_os::VmType::TERMINA);
  if (!base::Contains(existing_containers, container_id)) {
    LOG(ERROR) << "Attempting to import Crostini container backup into "
                  "non-existent container: "
               << container_id;
  }
  FillOperationData(ExportImportType::IMPORT, std::move(container_id));
  Start(path, /* create_new_container= */ false, std::move(callback));
}

void CrostiniExportImport::CreateContainerFromImport(
    guest_os::GuestId container_id,
    base::FilePath path,
    CrostiniManager::CrostiniResultCallback callback) {
  FillOperationData(ExportImportType::IMPORT, std::move(container_id));
  Start(path, /* create_new_container= */ true, std::move(callback));
}

void CrostiniExportImport::ExportContainer(guest_os::GuestId container_id,
                                           base::FilePath path,
                                           OnceTrackerFactory tracker_factory) {
  FillOperationData(ExportImportType::EXPORT, std::move(container_id),
                    std::move(tracker_factory));
  Start(path, /* create_new_container= */ false, base::DoNothing());
}

void CrostiniExportImport::ImportContainer(guest_os::GuestId container_id,
                                           base::FilePath path,
                                           OnceTrackerFactory tracker_factory) {
  FillOperationData(ExportImportType::IMPORT, std::move(container_id),
                    std::move(tracker_factory));
  Start(path, /* create_new_container= */ false, base::DoNothing());
}

void CrostiniExportImport::Start(
    base::FilePath path,
    bool create_new_container,
    CrostiniManager::CrostiniResultCallback callback) {
  auto operation_data = std::move(operation_data_);
  if (!crostini::CrostiniFeatures::Get()->IsExportImportUIAllowed(profile_)) {
    return std::move(callback).Run(CrostiniResult::NOT_ALLOWED);
  }

  auto status_tracker = std::move(operation_data->tracker_factory)
                            .Run(operation_data->type, path);
  status_tracker->SetStatusRunning(0);

  auto it = status_trackers_.find(operation_data_->container_id);
  if (it != status_trackers_.end()) {
    // There is already an operation in progress. Ensure the existing
    // status_tracker is (re)displayed so the user knows why this new concurrent
    // operation failed, and show a failure status_tracker for the new request.
    it->second->ForceRedisplay();
    status_tracker->SetStatusFailedConcurrentOperation(it->second->type());
    return;
  } else {
    status_trackers_.emplace_hint(it, operation_data->container_id,
                                  std::move(status_tracker));
    for (auto& observer : observers_) {
      observer.OnCrostiniExportImportOperationStatusChanged(true);
    }
  }

  switch (operation_data->type) {
    case ExportImportType::EXPORT:
      base::ThreadPool::PostTaskAndReply(
          FROM_HERE, {base::MayBlock()},
          // Ensure file exists so that it can be shared.
          base::BindOnce(
              [](const base::FilePath& path) {
                base::File file(path, base::File::FLAG_CREATE_ALWAYS |
                                          base::File::FLAG_WRITE);
                DCHECK(file.IsValid()) << path << " is invalid";
              },
              path),
          base::BindOnce(
              &CrostiniExportImport::EnsureLxdStartedThenSharePath,
              weak_ptr_factory_.GetWeakPtr(), operation_data->container_id,
              path, false, create_new_container,
              base::BindOnce(&CrostiniExportImport::ExportAfterSharing,
                             weak_ptr_factory_.GetWeakPtr(),
                             operation_data->container_id, path,
                             std::move(callback))));
      break;
    case ExportImportType::IMPORT:
      CrostiniExportImport::EnsureLxdStartedThenSharePath(
          operation_data->container_id, path, /*persist=*/false,
          create_new_container,
          base::BindOnce(&CrostiniExportImport::ImportAfterSharing,
                         weak_ptr_factory_.GetWeakPtr(),
                         operation_data->container_id, path,
                         std::move(callback)));
      break;
    case ExportImportType::EXPORT_DISK_IMAGE:
      // TODO(b:345312503): restart VM if it was started?
      crostini::CrostiniManager::GetForProfile(profile_)->StopVm(
          operation_data->container_id.vm_name,
          base::BindOnce(&CrostiniExportImport::ExportDiskImage,
                         weak_ptr_factory_.GetWeakPtr(),
                         operation_data->container_id, path,
                         std::move(callback)));
      break;
    case ExportImportType::IMPORT_DISK_IMAGE:
      crostini::CrostiniManager::GetForProfile(profile_)->StopVm(
          operation_data->container_id.vm_name,
          base::BindOnce(&CrostiniExportImport::ImportDiskImage,
                         weak_ptr_factory_.GetWeakPtr(),
                         operation_data->container_id, path,
                         std::move(callback)));
      break;
  }
}

void CrostiniExportImport::ExportDiskImage(
    const guest_os::GuestId& container_id,
    const base::FilePath& path,
    CrostiniManager::CrostiniResultCallback callback,
    CrostiniResult result) {
  if (result == CrostiniResult::VM_STOP_FAILED) {
    LOG(ERROR) << "Unable to stop VM, cannot export disk image";
    std::move(callback).Run(CrostiniResult::DISK_IMAGE_FAILED);
    return;
  }

  ash::ProfileHelper* profile_helper = ash::ProfileHelper::Get();
  if (!profile_helper) {
    LOG(ERROR) << "Unable to get profile helper";
    std::move(callback).Run(CrostiniResult::DISK_IMAGE_FAILED);
    return;
  }
  user_manager::User* user = profile_helper->GetUserByProfile(profile_);

  if (!user) {
    LOG(ERROR) << "Unable to get user";
    std::move(callback).Run(CrostiniResult::DISK_IMAGE_FAILED);
    return;
  }

  CrostiniManager::GetForProfile(profile_)->ExportDiskImage(
      container_id, user->username_hash(), path, /*force=*/false,
      base::BindOnce(&CrostiniExportImport::AfterDiskImageOperation,
                     weak_ptr_factory_.GetWeakPtr(), container_id,
                     std::move(callback)));
}

void CrostiniExportImport::ImportDiskImage(
    const guest_os::GuestId& container_id,
    const base::FilePath& path,
    CrostiniManager::CrostiniResultCallback callback,
    CrostiniResult result) {
  if (result == CrostiniResult::VM_STOP_FAILED) {
    LOG(ERROR) << "Unable to stop VM, cannot import disk image";
    std::move(callback).Run(CrostiniResult::DISK_IMAGE_FAILED);
    return;
  }

  ash::ProfileHelper* profile_helper = ash::ProfileHelper::Get();
  if (!profile_helper) {
    LOG(ERROR) << "Unable to get profile helper";
    std::move(callback).Run(CrostiniResult::DISK_IMAGE_FAILED);
    return;
  }
  user_manager::User* user = profile_helper->GetUserByProfile(profile_);

  if (!user) {
    LOG(ERROR) << "Unable to get user";
    std::move(callback).Run(CrostiniResult::DISK_IMAGE_FAILED);
    return;
  }

  CrostiniManager::GetForProfile(profile_)->ImportDiskImage(
      container_id, user->username_hash(), path,
      base::BindOnce(&CrostiniExportImport::AfterDiskImageOperation,
                     weak_ptr_factory_.GetWeakPtr(), container_id,
                     std::move(callback)));
}

void CrostiniExportImport::AfterDiskImageOperation(
    const guest_os::GuestId& container_id,
    CrostiniManager::CrostiniResultCallback callback,
    CrostiniResult result) {
  auto it = status_trackers_.find(container_id);
  if (it == status_trackers_.end()) {
    NOTREACHED_IN_MIGRATION()
        << container_id << " has no status_tracker to update";
    return;
  }

  if (result == CrostiniResult::SUCCESS) {
    switch (it->second->status()) {
      case CrostiniExportImportStatusTracker::Status::CANCELLING: {
        // If a user requests to cancel, but the export completes before the
        // cancel can happen (|result| == SUCCESS), then removing the exported
        // file is functionally the same as a successful cancel.
        base::ThreadPool::PostTask(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
            base::GetDeleteFileCallback(it->second->path()));
        RemoveTracker(it)->SetStatusCancelled();
        break;
      }
      case CrostiniExportImportStatusTracker::Status::RUNNING:
        RemoveTracker(it)->SetStatusDone();
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  } else if (result == CrostiniResult::DISK_IMAGE_CANCELLED) {
    switch (it->second->status()) {
      case CrostiniExportImportStatusTracker::Status::CANCELLING: {
        // If a user requests to cancel, and the export is cancelled (|result|
        // == DISK_IMAGE_CANCELLED), then the partially exported file needs to
        // be cleaned up.
        base::ThreadPool::PostTask(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
            base::GetDeleteFileCallback(it->second->path()));
        RemoveTracker(it)->SetStatusCancelled();
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
    }
  } else {
    LOG(ERROR) << "Error exporting " << int(result);
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::GetDeleteFileCallback(it->second->path()));
    DCHECK(it->second->status() ==
               CrostiniExportImportStatusTracker::Status::RUNNING ||
           it->second->status() ==
               CrostiniExportImportStatusTracker::Status::CANCELLING);
    RemoveTracker(it)->SetStatusFailed();
  }

  std::move(callback).Run(result);
}

void CrostiniExportImport::EnsureLxdStartedThenSharePath(
    const guest_os::GuestId& container_id,
    const base::FilePath& path,
    bool persist,
    bool create_new_container,
    guest_os::GuestOsSharePath::SharePathCallback callback) {
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile_);
  crostini::CrostiniManager::RestartOptions options;
  options.stop_after_lxd_available = true;
  if (create_new_container) {
    options.restart_source = crostini::RestartSource::kMultiContainerCreation;
  }
  crostini_manager->RestartCrostiniWithOptions(
      container_id, std::move(options),
      base::BindOnce(&CrostiniExportImport::SharePath,
                     weak_ptr_factory_.GetWeakPtr(), container_id.vm_name, path,
                     std::move(callback)));
}

void CrostiniExportImport::SharePath(
    const std::string& vm_name,
    const base::FilePath& path,
    guest_os::GuestOsSharePath::SharePathCallback callback,
    crostini::CrostiniResult result) {
  auto vm_info =
      crostini::CrostiniManager::GetForProfile(profile_)->GetVmInfo(vm_name);
  if (result != CrostiniResult::SUCCESS || !vm_info.has_value()) {
    std::move(callback).Run(base::FilePath(), false,
                            base::StringPrintf("VM could not be started: %d",
                                               static_cast<int>(result)));
    return;
  }
  guest_os::GuestOsSharePathFactory::GetForProfile(profile_)->SharePath(
      vm_name, vm_info->info.seneschal_server_handle(), path,
      std::move(callback));
}

void CrostiniExportImport::ExportAfterSharing(
    const guest_os::GuestId& container_id,
    const base::FilePath& path,
    CrostiniManager::CrostiniResultCallback callback,
    const base::FilePath& container_path,
    bool result,
    const std::string& failure_reason) {
  if (!result) {
    LOG(ERROR) << "Error sharing for export host path=" << path.value()
               << ", container path=" << container_path.value() << ": "
               << failure_reason;
    auto it = status_trackers_.find(container_id);
    if (it != status_trackers_.end()) {
      RemoveTracker(it)->SetStatusFailed();
    } else {
      NOTREACHED_IN_MIGRATION()
          << container_id << " has no status_tracker to update";
    }
    return;
  }
  CrostiniManager::GetForProfile(profile_)->ExportLxdContainer(
      container_id, container_path,
      base::BindOnce(&CrostiniExportImport::OnExportComplete,
                     weak_ptr_factory_.GetWeakPtr(), base::Time::Now(),
                     container_id, std::move(callback)));
}

void CrostiniExportImport::OnExportComplete(
    const base::Time& start,
    const guest_os::GuestId& container_id,
    CrostiniManager::CrostiniResultCallback callback,
    CrostiniResult result,
    uint64_t container_size,
    uint64_t compressed_size) {
  auto it = status_trackers_.find(container_id);
  if (it == status_trackers_.end()) {
    NOTREACHED_IN_MIGRATION()
        << container_id << " has no status_tracker to update";
    return;
  }

  ExportContainerResult enum_hist_result = ExportContainerResult::kSuccess;
  if (result == CrostiniResult::SUCCESS) {
    switch (it->second->status()) {
      case CrostiniExportImportStatusTracker::Status::CANCELLING: {
        // If a user requests to cancel, but the export completes before the
        // cancel can happen (|result| == SUCCESS), then removing the exported
        // file is functionally the same as a successful cancel.
        base::ThreadPool::PostTask(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
            base::GetDeleteFileCallback(it->second->path()));
        RemoveTracker(it)->SetStatusCancelled();
        break;
      }
      case CrostiniExportImportStatusTracker::Status::RUNNING:
        UMA_HISTOGRAM_LONG_TIMES("Crostini.BackupTimeSuccess",
                                 base::Time::Now() - start);
        // Log backup size statistics.
        if (container_size && compressed_size) {
          base::UmaHistogramCustomCounts("Crostini.BackupContainerSizeLog2",
                                         std::round(std::log2(container_size)),
                                         0, 50, 50);
          base::UmaHistogramCustomCounts("Crostini.BackupCompressedSizeLog2",
                                         std::round(std::log2(compressed_size)),
                                         0, 50, 50);
          base::UmaHistogramPercentageObsoleteDoNotUse(
              "Crostini.BackupSizeRatio",
              std::round(compressed_size * 100.0 / container_size));
        }
        RemoveTracker(it)->SetStatusDone();
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  } else if (result == CrostiniResult::CONTAINER_EXPORT_IMPORT_CANCELLED) {
    switch (it->second->status()) {
      case CrostiniExportImportStatusTracker::Status::CANCELLING: {
        // If a user requests to cancel, and the export is cancelled (|result|
        // == CONTAINER_EXPORT_IMPORT_CANCELLED), then the partially exported
        // file needs to be cleaned up.
        base::ThreadPool::PostTask(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
            base::GetDeleteFileCallback(it->second->path()));
        RemoveTracker(it)->SetStatusCancelled();
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
    }
  } else {
    LOG(ERROR) << "Error exporting " << int(result);
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::GetDeleteFileCallback(it->second->path()));
    switch (result) {
      case CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED:
        enum_hist_result = ExportContainerResult::kFailedVmStopped;
        break;
      case CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STARTED:
        enum_hist_result = ExportContainerResult::kFailedVmStarted;
        break;
      default:
        DCHECK(result == CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED);
        enum_hist_result = ExportContainerResult::kFailed;
    }
    UMA_HISTOGRAM_LONG_TIMES("Crostini.BackupTimeFailed",
                             base::Time::Now() - start);
    DCHECK(it->second->status() ==
               CrostiniExportImportStatusTracker::Status::RUNNING ||
           it->second->status() ==
               CrostiniExportImportStatusTracker::Status::CANCELLING);
    RemoveTracker(it)->SetStatusFailed();
  }
  UMA_HISTOGRAM_ENUMERATION("Crostini.Backup", enum_hist_result);
  std::move(callback).Run(result);
}

void CrostiniExportImport::OnExportContainerProgress(
    const guest_os::GuestId& container_id,
    const StreamingExportStatus& status) {
  auto it = status_trackers_.find(container_id);
  if (it == status_trackers_.end()) {
    LOG(WARNING) << container_id
                 << " has no status_tracker to update, perhaps Chrome crashed "
                    "while an export was in progress.";
    return;
  }

  const auto files_percent = 100.0 * status.exported_files / status.total_files;
  const auto bytes_percent = 100.0 * status.exported_bytes / status.total_bytes;

  // Averaging the two percentages gives a more accurate estimation.
  // TODO(juwa): investigate more accurate approximations of percent.
  const auto percent = (files_percent + bytes_percent) / 2.0;

  it->second->SetStatusRunning(static_cast<int>(percent));
}

void CrostiniExportImport::ImportAfterSharing(
    const guest_os::GuestId& container_id,
    const base::FilePath& path,
    CrostiniManager::CrostiniResultCallback callback,
    const base::FilePath& container_path,
    bool result,
    const std::string& failure_reason) {
  if (!result) {
    LOG(ERROR) << "Error sharing for import path=" << path
               << ", container path=" << container_path.value() << ": "
               << failure_reason;
    auto it = status_trackers_.find(container_id);
    if (it != status_trackers_.end()) {
      RemoveTracker(it)->SetStatusFailed();
    } else {
      NOTREACHED_IN_MIGRATION()
          << container_id << " has no status_tracker to update";
    }
    return;
  }
  CrostiniManager::GetForProfile(profile_)->ImportLxdContainer(
      container_id, container_path,
      base::BindOnce(&CrostiniExportImport::OnImportComplete,
                     weak_ptr_factory_.GetWeakPtr(), base::Time::Now(),
                     container_id, std::move(callback)));
}

void CrostiniExportImport::OnImportComplete(
    const base::Time& start,
    const guest_os::GuestId& container_id,
    CrostiniManager::CrostiniResultCallback callback,
    CrostiniResult result) {
  auto it = status_trackers_.find(container_id);

  ImportContainerResult enum_hist_result = ImportContainerResult::kSuccess;
  if (result == CrostiniResult::SUCCESS) {
    UMA_HISTOGRAM_LONG_TIMES("Crostini.RestoreTimeSuccess",
                             base::Time::Now() - start);
    if (it != status_trackers_.end()) {
      switch (it->second->status()) {
        case CrostiniExportImportStatusTracker::Status::RUNNING:
          // If a user requests to cancel, but the import completes before the
          // cancel can happen, then the container will have been imported over
          // and the cancel will have failed. However the period of time in
          // which this can happen is very small (<5s), so it feels quite
          // natural to pretend the cancel did not happen, and instead display
          // success.
        case CrostiniExportImportStatusTracker::Status::CANCELLING:
          RemoveTracker(it)->SetStatusDone();
          break;
        default:
          NOTREACHED_IN_MIGRATION();
      }
    } else {
      NOTREACHED_IN_MIGRATION()
          << container_id << " has no status_tracker to update";
    }
  } else if (result ==
             crostini::CrostiniResult::CONTAINER_EXPORT_IMPORT_CANCELLED) {
    if (it != status_trackers_.end()) {
      switch (it->second->status()) {
        case CrostiniExportImportStatusTracker::Status::CANCELLING:
          RemoveTracker(it)->SetStatusCancelled();
          break;
        default:
          NOTREACHED_IN_MIGRATION();
      }
    } else {
      NOTREACHED_IN_MIGRATION()
          << container_id << " has no status_tracker to update";
    }
  } else {
    LOG(ERROR) << "Error importing " << int(result);
    switch (result) {
      case CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED:
        enum_hist_result = ImportContainerResult::kFailedVmStopped;
        break;
      case CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STARTED:
        enum_hist_result = ImportContainerResult::kFailedVmStarted;
        break;
      case CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_ARCHITECTURE:
        enum_hist_result = ImportContainerResult::kFailedArchitecture;
        break;
      case CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_SPACE:
        enum_hist_result = ImportContainerResult::kFailedSpace;
        break;
      default:
        DCHECK(result == CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED);
        enum_hist_result = ImportContainerResult::kFailed;
    }
    // If the operation didn't start successfully or the vm stops during the
    // import, then the status_tracker status will not have been set in
    // OnImportContainerProgress, so it needs to be updated.
    if (result == CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED ||
        result == CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED ||
        result == CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STARTED) {
      if (it != status_trackers_.end()) {
        DCHECK(it->second->status() ==
               CrostiniExportImportStatusTracker::Status::RUNNING);
        RemoveTracker(it)->SetStatusFailed();
      } else {
        NOTREACHED_IN_MIGRATION()
            << container_id << " has no status_tracker to update";
      }
    } else {
      DCHECK(it == status_trackers_.end())
          << container_id << " has unexpected status_tracker";
    }
    UMA_HISTOGRAM_LONG_TIMES("Crostini.RestoreTimeFailed",
                             base::Time::Now() - start);
  }
  UMA_HISTOGRAM_ENUMERATION("Crostini.Restore", enum_hist_result);

  // Restart from CrostiniManager.
  CrostiniManager::GetForProfile(profile_)->RestartCrostini(
      container_id, std::move(callback));
}

void CrostiniExportImport::OnDiskImageProgress(
    const guest_os::GuestId& container_id,
    DiskImageProgressStatus status,
    int progress) {
  auto it = status_trackers_.find(container_id);
  if (it == status_trackers_.end()) {
    LOG(WARNING) << container_id
                 << " has no status_tracker to update, perhaps Chrome crashed "
                    "while a disk image operation was in progress.";
    return;
  }

  switch (status) {
    case DiskImageProgressStatus::IN_PROGRESS:
      it->second->SetStatusRunning(progress);
      break;
    case DiskImageProgressStatus::FAILURE_SPACE:
      RemoveTracker(it)->SetStatusFailedInsufficientSpaceUnknownAmount();
      break;
    default:
      LOG(WARNING) << "Unknown disk image progress status: " << int(status);
  }
}

void CrostiniExportImport::OnImportContainerProgress(
    const guest_os::GuestId& container_id,
    ImportContainerProgressStatus status,
    int progress_percent,
    uint64_t progress_speed,
    const std::string& architecture_device,
    const std::string& architecture_container,
    uint64_t available_space,
    uint64_t minimum_required_space) {
  auto it = status_trackers_.find(container_id);
  if (it == status_trackers_.end()) {
    LOG(WARNING) << container_id
                 << " has no status_tracker to update, perhaps Chrome crashed "
                    "while an import was in progress.";
    return;
  }

  switch (status) {
    // Rescale UPLOAD:1-100 => 0-50.
    case ImportContainerProgressStatus::UPLOAD:
      it->second->SetStatusRunning(progress_percent / 2);
      break;
    // Rescale UNPACK:1-100 => 50-100.
    case ImportContainerProgressStatus::UNPACK:
      it->second->SetStatusRunning(50 + progress_percent / 2);
      break;
    // Failure, set error message.
    case ImportContainerProgressStatus::FAILURE_ARCHITECTURE:
      RemoveTracker(it)->SetStatusFailedArchitectureMismatch(
          architecture_container, architecture_device);
      break;
    case ImportContainerProgressStatus::FAILURE_SPACE:
      DCHECK_GE(minimum_required_space, available_space);
      RemoveTracker(it)->SetStatusFailedInsufficientSpace(
          minimum_required_space - available_space);
      break;
    default:
      LOG(WARNING) << "Unknown Export progress status " << int(status);
  }
}

std::string CrostiniExportImport::GetUniqueNotificationId() {
  return base::StringPrintf("crostini_export_import_%d",
                            next_status_tracker_id_++);
}

std::unique_ptr<CrostiniExportImportStatusTracker>
CrostiniExportImport::RemoveTracker(TrackerMap::iterator it) {
  DCHECK(it != status_trackers_.end());
  auto status_tracker = std::move(it->second);
  status_trackers_.erase(it);
  for (auto& observer : observers_) {
    observer.OnCrostiniExportImportOperationStatusChanged(false);
  }
  return status_tracker;
}

void CrostiniExportImport::CancelOperation(ExportImportType type,
                                           guest_os::GuestId container_id) {
  auto it = status_trackers_.find(container_id);
  if (it == status_trackers_.end()) {
    NOTREACHED_IN_MIGRATION()
        << container_id << " has no status_tracker to cancel";
    return;
  }

  it->second->SetStatusCancelling();

  auto& manager = *CrostiniManager::GetForProfile(profile_);

  switch (type) {
    case ExportImportType::EXPORT:
      manager.CancelExportLxdContainer(std::move(container_id));
      return;
    case ExportImportType::IMPORT:
      manager.CancelImportLxdContainer(std::move(container_id));
      return;
    case ExportImportType::EXPORT_DISK_IMAGE:
    case ExportImportType::IMPORT_DISK_IMAGE:
      manager.CancelDiskImageOp(std::move(container_id));
      return;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

bool CrostiniExportImport::GetExportImportOperationStatus() const {
  return status_trackers_.size() != 0;
}

base::WeakPtr<CrostiniExportImportNotificationController>
CrostiniExportImport::GetNotificationControllerForTesting(
    guest_os::GuestId container_id) {
  auto it = status_trackers_.find(container_id);
  if (it == status_trackers_.end()) {
    return nullptr;
  }
  return static_cast<CrostiniExportImportNotificationController*>(
             it->second.get())
      ->GetWeakPtr();
}

}  // namespace crostini

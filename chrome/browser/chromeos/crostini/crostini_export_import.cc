// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_export_import.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_manager_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"
#include "chrome/browser/chromeos/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/grit/generated_resources.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace crostini {

class CrostiniExportImportFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CrostiniExportImport* GetForProfile(Profile* profile) {
    return static_cast<CrostiniExportImport*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static CrostiniExportImportFactory* GetInstance() {
    static base::NoDestructor<CrostiniExportImportFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<CrostiniExportImportFactory>;

  CrostiniExportImportFactory()
      : BrowserContextKeyedServiceFactory(
            "CrostiniExportImportService",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(guest_os::GuestOsSharePathFactory::GetInstance());
    DependsOn(CrostiniManagerFactory::GetInstance());
  }

  ~CrostiniExportImportFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    return new CrostiniExportImport(profile);
  }
};

CrostiniExportImport* CrostiniExportImport::GetForProfile(Profile* profile) {
  return CrostiniExportImportFactory::GetForProfile(profile);
}

CrostiniExportImport::CrostiniExportImport(Profile* profile)
    : profile_(profile) {
  CrostiniManager* manager = CrostiniManager::GetForProfile(profile_);
  manager->AddExportContainerProgressObserver(this);
  manager->AddImportContainerProgressObserver(this);
}

CrostiniExportImport::~CrostiniExportImport() = default;

void CrostiniExportImport::Shutdown() {
  CrostiniManager* manager = CrostiniManager::GetForProfile(profile_);
  manager->RemoveExportContainerProgressObserver(this);
  manager->RemoveImportContainerProgressObserver(this);
}

CrostiniExportImport::OperationData::OperationData(
    ExportImportType type,
    ContainerId container_id,
    OnceTrackerFactory tracker_factory)
    : type(type),
      container_id(std::move(container_id)),
      tracker_factory(std::move(tracker_factory)) {}

CrostiniExportImport::OperationData::~OperationData() = default;

CrostiniExportImport::OperationData* CrostiniExportImport::NewOperationData(
    ExportImportType type,
    ContainerId container_id,
    OnceTrackerFactory factory) {
  auto operation_data = std::make_unique<OperationData>(
      type, std::move(container_id), std::move(factory));
  OperationData* operation_data_ptr = operation_data.get();
  // |operation_data_storage_| takes ownership.
  operation_data_storage_[operation_data_ptr] = std::move(operation_data);
  return operation_data_ptr;
}

CrostiniExportImport::OperationData* CrostiniExportImport::NewOperationData(
    ExportImportType type,
    ContainerId container_id) {
  OnceTrackerFactory factory = base::BindOnce(
      [](Profile* profile, ContainerId container_id,
         std::string notification_id, ExportImportType type,
         base::FilePath path)
          -> std::unique_ptr<CrostiniExportImportStatusTracker> {
        return std::make_unique<CrostiniExportImportNotificationController>(
            profile, type, std::move(notification_id), std::move(path),
            std::move(container_id));
      },
      profile_, container_id, GetUniqueNotificationId());
  return NewOperationData(type, std::move(container_id), std::move(factory));
}

CrostiniExportImport::OperationData* CrostiniExportImport::NewOperationData(
    ExportImportType type) {
  return NewOperationData(type, ContainerId::GetDefault());
}

void CrostiniExportImport::ExportContainer(content::WebContents* web_contents) {
  OpenFileDialog(NewOperationData(ExportImportType::EXPORT), web_contents);
}

void CrostiniExportImport::ImportContainer(content::WebContents* web_contents) {
  OpenFileDialog(NewOperationData(ExportImportType::IMPORT), web_contents);
}

void CrostiniExportImport::ExportContainer(ContainerId container_id,
                                           content::WebContents* web_contents,
                                           OnceTrackerFactory tracker_factory) {
  OpenFileDialog(
      NewOperationData(ExportImportType::EXPORT, std::move(container_id),
                       std::move(tracker_factory)),
      web_contents);
}

void CrostiniExportImport::ImportContainer(ContainerId container_id,
                                           content::WebContents* web_contents,
                                           OnceTrackerFactory tracker_factory) {
  OpenFileDialog(
      NewOperationData(ExportImportType::IMPORT, std::move(container_id),
                       std::move(tracker_factory)),
      web_contents);
}

base::FilePath CrostiniExportImport::GetDefaultBackupPath() const {
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  return file_manager::util::GetMyFilesFolderForProfile(profile_).Append(
      base::StringPrintf("chromeos-linux-%04d-%02d-%02d.tini", exploded.year,
                         exploded.month, exploded.day_of_month));
}

void CrostiniExportImport::OpenFileDialog(OperationData* operation_data,
                                          content::WebContents* web_contents) {
  if (!crostini::CrostiniFeatures::Get()->IsExportImportUIAllowed(profile_)) {
    return;
  }

  ui::SelectFileDialog::Type file_selector_mode;
  unsigned title = 0;
  base::FilePath default_path;
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.allowed_paths = ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH;
  file_types.extensions = {{"tini", "tar.gz", "tgz"}};

  switch (operation_data->type) {
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
  }

  select_folder_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));
  select_folder_dialog_->SelectFile(
      file_selector_mode, l10n_util::GetStringUTF16(title), default_path,
      &file_types, 0, base::FilePath::StringType(),
      web_contents->GetTopLevelNativeWindow(),
      static_cast<void*>(operation_data));
}

void CrostiniExportImport::FileSelected(const base::FilePath& path,
                                        int index,
                                        void* params) {
  Start(static_cast<OperationData*>(params), path, base::DoNothing());
}

void CrostiniExportImport::FileSelectionCanceled(void* params) {
  auto* operation_data = static_cast<OperationData*>(params);
  if (operation_data->tracker_factory) {
    // Create the status tracker so we can let it know the operation was
    // canceled.
    auto status_tracker = std::move(operation_data->tracker_factory)
                              .Run(operation_data->type, base::FilePath());
    status_tracker->SetStatusCancelled();
  }
  operation_data_storage_.erase(operation_data);
}

void CrostiniExportImport::ExportContainer(
    ContainerId container_id,
    base::FilePath path,
    CrostiniManager::CrostiniResultCallback callback) {
  Start(NewOperationData(ExportImportType::EXPORT, std::move(container_id)),
        path, std::move(callback));
}

void CrostiniExportImport::ImportContainer(
    ContainerId container_id,
    base::FilePath path,
    CrostiniManager::CrostiniResultCallback callback) {
  Start(NewOperationData(ExportImportType::IMPORT, std::move(container_id)),
        path, std::move(callback));
}

void CrostiniExportImport::ExportContainer(ContainerId container_id,
                                           base::FilePath path,
                                           OnceTrackerFactory tracker_factory) {
  Start(NewOperationData(ExportImportType::EXPORT, std::move(container_id),
                         std::move(tracker_factory)),
        path, base::DoNothing());
}

void CrostiniExportImport::ImportContainer(ContainerId container_id,
                                           base::FilePath path,
                                           OnceTrackerFactory tracker_factory) {
  Start(NewOperationData(ExportImportType::IMPORT, std::move(container_id),
                         std::move(tracker_factory)),
        path, base::DoNothing());
}

void CrostiniExportImport::Start(
    OperationData* operation_data,
    base::FilePath path,
    CrostiniManager::CrostiniResultCallback callback) {
  std::unique_ptr<OperationData> operation_data_storage(
      std::move(operation_data_storage_[operation_data]));
  operation_data_storage_.erase(operation_data);

  if (!crostini::CrostiniFeatures::Get()->IsExportImportUIAllowed(profile_)) {
    return std::move(callback).Run(CrostiniResult::NOT_ALLOWED);
  }

  auto status_tracker = std::move(operation_data->tracker_factory)
                            .Run(operation_data->type, path);
  status_tracker->SetStatusRunning(0);

  auto it = status_trackers_.find(operation_data->container_id);
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
              &guest_os::GuestOsSharePath::SharePath,
              base::Unretained(
                  guest_os::GuestOsSharePath::GetForProfile(profile_)),
              kCrostiniDefaultVmName, path, false,
              base::BindOnce(&CrostiniExportImport::ExportAfterSharing,
                             weak_ptr_factory_.GetWeakPtr(),
                             operation_data->container_id, path,
                             std::move(callback))));
      break;
    case ExportImportType::IMPORT:
      guest_os::GuestOsSharePath::GetForProfile(profile_)->SharePath(
          kCrostiniDefaultVmName, path, false,
          base::BindOnce(&CrostiniExportImport::ImportAfterSharing,
                         weak_ptr_factory_.GetWeakPtr(),
                         operation_data->container_id, path,
                         std::move(callback)));
      break;
  }
}

void CrostiniExportImport::ExportAfterSharing(
    const ContainerId& container_id,
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
      NOTREACHED() << container_id << " has no status_tracker to update";
    }
    return;
  }
  CrostiniManager::GetForProfile(profile_)->ExportLxdContainer(
      ContainerId::GetDefault(), container_path,
      base::BindOnce(&CrostiniExportImport::OnExportComplete,
                     weak_ptr_factory_.GetWeakPtr(), base::Time::Now(),
                     container_id, std::move(callback)));
}

void CrostiniExportImport::OnExportComplete(
    const base::Time& start,
    const ContainerId& container_id,
    CrostiniManager::CrostiniResultCallback callback,
    CrostiniResult result,
    uint64_t container_size,
    uint64_t compressed_size) {
  auto it = status_trackers_.find(container_id);
  if (it == status_trackers_.end()) {
    NOTREACHED() << container_id << " has no status_tracker to update";
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
            base::BindOnce(base::GetDeleteFileCallback(), it->second->path()));
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
          base::UmaHistogramPercentage(
              "Crostini.BackupSizeRatio",
              std::round(compressed_size * 100.0 / container_size));
        }
        RemoveTracker(it)->SetStatusDone();
        break;
      default:
        NOTREACHED();
    }
  } else if (result == CrostiniResult::CONTAINER_EXPORT_IMPORT_CANCELLED) {
    switch (it->second->status()) {
      case CrostiniExportImportStatusTracker::Status::CANCELLING: {
        // If a user requests to cancel, and the export is cancelled (|result|
        // == CONTAINER_EXPORT_IMPORT_CANCELLED), then the partially exported
        // file needs to be cleaned up.
        base::ThreadPool::PostTask(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
            base::BindOnce(base::GetDeleteFileCallback(), it->second->path()));
        RemoveTracker(it)->SetStatusCancelled();
        break;
      }
      default:
        NOTREACHED();
    }
  } else {
    LOG(ERROR) << "Error exporting " << int(result);
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(base::GetDeleteFileCallback(), it->second->path()));
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
    const ContainerId& container_id,
    ExportContainerProgressStatus status,
    int progress_percent,
    uint64_t progress_speed) {
  auto it = status_trackers_.find(container_id);
  if (it == status_trackers_.end()) {
    NOTREACHED() << container_id << " has no status_tracker to update";
    return;
  }

  switch (status) {
    // Rescale PACK:1-100 => 0-50.
    case ExportContainerProgressStatus::PACK:
      it->second->SetStatusRunning(progress_percent / 2);
      break;
    // Rescale DOWNLOAD:1-100 => 50-100.
    case ExportContainerProgressStatus::DOWNLOAD:
      it->second->SetStatusRunning(50 + progress_percent / 2);
      break;
    default:
      LOG(WARNING) << "Unknown Export progress status " << int(status);
  }
}

void CrostiniExportImport::OnExportContainerProgress(
    const ContainerId& container_id,
    const StreamingExportStatus& status) {
  auto it = status_trackers_.find(container_id);
  if (it == status_trackers_.end()) {
    NOTREACHED() << container_id << " has no status_tracker to update";
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
    const ContainerId& container_id,
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
      NOTREACHED() << container_id << " has no status_tracker to update";
    }
    return;
  }
  CrostiniManager::GetForProfile(profile_)->ImportLxdContainer(
      ContainerId::GetDefault(), container_path,
      base::BindOnce(&CrostiniExportImport::OnImportComplete,
                     weak_ptr_factory_.GetWeakPtr(), base::Time::Now(),
                     container_id, std::move(callback)));
}

void CrostiniExportImport::OnImportComplete(
    const base::Time& start,
    const ContainerId& container_id,
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
          NOTREACHED();
      }
    } else {
      NOTREACHED() << container_id << " has no status_tracker to update";
    }
  } else if (result ==
             crostini::CrostiniResult::CONTAINER_EXPORT_IMPORT_CANCELLED) {
    if (it != status_trackers_.end()) {
      switch (it->second->status()) {
        case CrostiniExportImportStatusTracker::Status::CANCELLING:
          RemoveTracker(it)->SetStatusCancelled();
          break;
        default:
          NOTREACHED();
      }
    } else {
      NOTREACHED() << container_id << " has no status_tracker to update";
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
        NOTREACHED() << container_id << " has no status_tracker to update";
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

void CrostiniExportImport::OnImportContainerProgress(
    const ContainerId& container_id,
    ImportContainerProgressStatus status,
    int progress_percent,
    uint64_t progress_speed,
    const std::string& architecture_device,
    const std::string& architecture_container,
    uint64_t available_space,
    uint64_t minimum_required_space) {
  auto it = status_trackers_.find(container_id);
  if (it == status_trackers_.end()) {
    NOTREACHED() << container_id << " has no status_tracker to update";
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
                                           ContainerId container_id) {
  auto it = status_trackers_.find(container_id);
  if (it == status_trackers_.end()) {
    NOTREACHED() << container_id << " has no status_tracker to cancel";
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
    default:
      NOTREACHED();
  }
}

bool CrostiniExportImport::GetExportImportOperationStatus() const {
  ContainerId id(kCrostiniDefaultVmName, kCrostiniDefaultContainerName);
  return status_trackers_.find(id) != status_trackers_.end();
}

base::WeakPtr<CrostiniExportImportNotificationController>
CrostiniExportImport::GetNotificationControllerForTesting(
    ContainerId container_id) {
  auto it = status_trackers_.find(container_id);
  if (it == status_trackers_.end()) {
    return nullptr;
  }
  return static_cast<CrostiniExportImportNotificationController*>(
             it->second.get())
      ->GetWeakPtr();
}

}  // namespace crostini

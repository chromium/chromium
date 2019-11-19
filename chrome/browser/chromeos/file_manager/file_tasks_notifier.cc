// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier_factory.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/network_service_instance.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace file_manager {
namespace file_tasks {
namespace {

bool IsSupportedFileSystemType(storage::FileSystemType type) {
  switch (type) {
    case storage::kFileSystemTypeNativeLocal:
    case storage::kFileSystemTypeRestrictedNativeLocal:
    case storage::kFileSystemTypeDriveFs:
      return true;
    default:
      return false;
  }
}

void ReturnQueryResults(
    std::unique_ptr<std::vector<FileTasksNotifier::FileAvailability>> results,
    base::OnceCallback<void(std::vector<FileTasksNotifier::FileAvailability>)>
        callback) {
  std::move(callback).Run(std::move(*results));
}

}  // namespace

struct FileTasksNotifier::PendingFileAvailabilityTask {
  storage::FileSystemURL url;
  FileTasksNotifier::FileAvailability* output;
  base::OnceClosure done;
};

FileTasksNotifier::FileTasksNotifier(Profile* profile)
    : profile_(profile),
      download_notifier_(content::BrowserContext::GetDownloadManager(profile_),
                         this) {}

FileTasksNotifier::~FileTasksNotifier() = default;

// static
FileTasksNotifier* FileTasksNotifier::GetForProfile(Profile* profile) {
  return FileTasksNotifierFactory::GetInstance()->GetForProfile(profile);
}

void FileTasksNotifier::AddObserver(FileTasksObserver* observer) {
  observers_.AddObserver(observer);
}

void FileTasksNotifier::RemoveObserver(FileTasksObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FileTasksNotifier::QueryFileAvailability(
    const std::vector<base::FilePath>& paths,
    base::OnceCallback<void(std::vector<FileAvailability>)> callback) {
  const auto* mount_points = storage::ExternalMountPoints::GetSystemInstance();
  std::vector<FileAvailability> results(paths.size(),
                                        FileAvailability::kUnknown);

  std::vector<PendingFileAvailabilityTask> tasks;
  for (size_t i = 0; i < paths.size(); ++i) {
    base::FilePath virtual_path;
    if (!mount_points->GetVirtualPath(paths[i], &virtual_path)) {
      continue;
    }
    auto url = mount_points->CreateCrackedFileSystemURL(
        url::Origin(), storage::kFileSystemTypeExternal, virtual_path);
    if (!url.is_valid() || !IsSupportedFileSystemType(url.type())) {
      results[i] = FileAvailability::kGone;
      continue;
    }
    tasks.push_back({url, &results[i]});
  }
  if (tasks.empty()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(results)));
    return;
  }

  auto results_owner = std::make_unique<std::vector<FileAvailability>>();
  std::swap(results, *results_owner);
  auto closure = base::BarrierClosure(
      tasks.size(),
      base::BindOnce(&ReturnQueryResults, std::move(results_owner),
                     std::move(callback)));
  for (auto& task : tasks) {
    task.done = closure;
    GetFileAvailability(std::move(task));
  }
}

void FileTasksNotifier::OnDownloadUpdated(content::DownloadManager* manager,
                                          download::DownloadItem* item) {
  if (item->IsTransient() ||
      item->GetState() != download::DownloadItem::DownloadState::COMPLETE ||
      item->GetDownloadCreationType() ==
          download::DownloadItem::DownloadCreationType::TYPE_HISTORY_IMPORT) {
    return;
  }
  NotifyObservers({item->GetTargetFilePath()},
                  FileTasksObserver::OpenType::kDownload);
}

void FileTasksNotifier::NotifyFileTasks(
    const std::vector<storage::FileSystemURL>& file_urls) {
  std::vector<base::FilePath> paths;
  for (const auto& url : file_urls) {
    if (IsSupportedFileSystemType(url.type())) {
      paths.push_back(url.path());
    }
  }
  NotifyObservers(paths, FileTasksObserver::OpenType::kLaunch);
}

void FileTasksNotifier::NotifyFileDialogSelection(
    const std::vector<ui::SelectedFileInfo>& files,
    bool for_open) {
  std::vector<base::FilePath> paths;
  for (const auto& file : files) {
    paths.push_back(file.file_path);
  }
  NotifyObservers(paths, for_open ? FileTasksObserver::OpenType::kOpen
                                  : FileTasksObserver::OpenType::kSaveAs);
}

void FileTasksNotifier::NotifyObservers(
    const std::vector<base::FilePath>& paths,
    FileTasksObserver::OpenType open_type) {
  std::vector<FileTasksObserver::FileOpenEvent> opens;
  for (const auto& path : paths) {
    if (profile_->GetPath().IsParent(path) ||
        base::FilePath("/run/arc/sdcard/write/emulated/0").IsParent(path) ||
        base::FilePath("/media/fuse").IsParent(path)) {
      opens.push_back({path, open_type});
    }
  }
  if (opens.empty()) {
    return;
  }
  for (auto& observer : observers_) {
    observer.OnFilesOpened(opens);
  }
}

void FileTasksNotifier::GetFileAvailability(PendingFileAvailabilityTask task) {
  if (task.url.type() != storage::kFileSystemTypeDriveFs) {
    base::FilePath path = std::move(task.url.path());
    base::PostTaskAndReplyWithResult(
        FROM_HERE, {base::ThreadPool(), base::MayBlock()},
        base::BindOnce(&base::PathExists, std::move(path)),
        base::BindOnce(&FileTasksNotifier::ForwardQueryResult,
                       std::move(task)));
    return;
  }
  if (!GetDriveFsInterface()) {
    *task.output = FileTasksNotifier::FileAvailability::kUnknown;
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(task.done));
    return;
  }
  base::FilePath drive_path;
  if (!GetRelativeDrivePath(task.url.path(), &drive_path)) {
    *task.output = FileTasksNotifier::FileAvailability::kGone;
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(task.done));
    return;
  }
  GetDriveFsInterface()->GetMetadata(
      drive_path,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&FileTasksNotifier::ForwardDriveFsQueryResult,
                         std::move(task), IsOffline()),
          drive::FILE_ERROR_SERVICE_UNAVAILABLE, nullptr));
}

// static
void FileTasksNotifier::ForwardQueryResult(PendingFileAvailabilityTask task,
                                           bool exists) {
  *task.output = exists ? FileTasksNotifier::FileAvailability::kOk
                        : FileTasksNotifier::FileAvailability::kGone;
  std::move(task.done).Run();
}

// static
void FileTasksNotifier::ForwardDriveFsQueryResult(
    PendingFileAvailabilityTask task,
    bool is_offline,
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  if (error == drive::FILE_ERROR_NOT_FOUND) {
    *task.output = FileTasksNotifier::FileAvailability::kGone;
  } else if (error != drive::FILE_ERROR_OK) {
    *task.output = FileTasksNotifier::FileAvailability::kUnknown;
  } else {
    *task.output =
        metadata->available_offline || !is_offline
            ? FileTasksNotifier::FileAvailability::kOk
            : FileTasksNotifier::FileAvailability::kTemporarilyUnavailable;
  }
  std::move(task.done).Run();
}

drivefs::mojom::DriveFs* FileTasksNotifier::GetDriveFsInterface() {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile_);
  if (!integration_service || !integration_service->IsMounted()) {
    return nullptr;
  }
  return integration_service->GetDriveFsInterface();
}

bool FileTasksNotifier::GetRelativeDrivePath(
    const base::FilePath& path,
    base::FilePath* drive_relative_path) {
  return drive::DriveIntegrationServiceFactory::FindForProfile(profile_)
      ->GetRelativeDrivePath(path, drive_relative_path);
}

bool FileTasksNotifier::IsOffline() {
  return content::GetNetworkConnectionTracker()->IsOffline();
}

}  // namespace file_tasks
}  // namespace file_manager

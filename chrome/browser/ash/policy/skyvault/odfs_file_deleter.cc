// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/odfs_file_deleter.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/delete_io_task.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

namespace ash::cloud_upload {

OdfsFileDeleter::OdfsFileDeleter(
    const base::FilePath& path,
    base::OnceCallback<void(bool delete_successful)> callback)
    : path_(path), callback_(std::move(callback)), weak_ptr_factory_(this) {}

OdfsFileDeleter::~OdfsFileDeleter() = default;

void OdfsFileDeleter::StartDeletion() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);

  auto* file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(profile);
  auto filesystem_url =
      FilePathToFileSystemURL(profile, file_system_context, path_);
  if (!filesystem_url.is_valid()) {
    LOG(ERROR) << "Unable to get filesystem URL for deleted file";
    std::move(callback_).Run(/*delete_successful=*/false);
    return;
  }

  std::vector<storage::FileSystemURL> file_urls;
  file_urls.push_back(filesystem_url);

  auto task = std::make_unique<file_manager::io_task::DeleteIOTask>(
      file_urls, file_system_context,
      /*show_notification=*/false);

  file_manager::VolumeManager* volume_manager =
      file_manager::VolumeManager::Get(profile);
  if (!volume_manager) {
    LOG(ERROR) << "Unable to get volume manager for deleted file";
    std::move(callback_).Run(/*delete_successful=*/false);
    return;
  }
  io_task_controller_ = volume_manager->io_task_controller();
  task_id_ = io_task_controller_->Add(std::move(task));
  io_task_controller_->AddObserver(this);
}

void OdfsFileDeleter::OnIOTaskStatus(
    const ::file_manager::io_task::ProgressStatus& status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (status.task_id != task_id_) {
    return;
  }

  // Ignore intermediate states.
  if (status.state == file_manager::io_task::State::kQueued ||
      status.state == file_manager::io_task::State::kScanning ||
      status.state == file_manager::io_task::State::kInProgress ||
      status.state == file_manager::io_task::State::kPaused) {
    return;
  }

  // Determine the delete_successful result based on the task state.
  bool delete_successful;
  switch (status.state) {
    case file_manager::io_task::State::kQueued:
    case file_manager::io_task::State::kScanning:
    case file_manager::io_task::State::kInProgress:
    case file_manager::io_task::State::kPaused:
      return;
    case file_manager::io_task::State::kSuccess:
      delete_successful = true;
      break;
    case file_manager::io_task::State::kError:
    case file_manager::io_task::State::kNeedPassword:
    case file_manager::io_task::State::kCancelled:
      delete_successful = false;
      // Handle specific error cases if needed.
      LOG(ERROR) << "OneDrive file deletion failed with state: "
                 << status.state;
      break;
  }

  std::move(callback_).Run(delete_successful);

  io_task_controller_->RemoveObserver(this);
  // Delete this object as the task is complete and callback is executed.
  delete this;
}

// Static method to initiate the deletion process.
void OdfsFileDeleter::Delete(
    const base::FilePath& path,
    base::OnceCallback<void(bool delete_successful)> callback) {
  // This static method ensures the `deleter` object lives until the IO task
  // completes and the callback is executed.
  auto* deleter = new OdfsFileDeleter(path, std::move(callback));
  deleter->StartDeletion();
}

}  // namespace ash::cloud_upload

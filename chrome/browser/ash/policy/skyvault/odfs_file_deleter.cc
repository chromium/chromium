// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/odfs_file_deleter.h"

#include "base/functional/bind.h"
#include "base/logging.h"
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
    : path_(path), callback_(std::move(callback)) {}

OdfsFileDeleter::~OdfsFileDeleter() = default;

void OdfsFileDeleter::StartDeletion() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M160);

  auto* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile, base::NotFatalUntil::M160);

  auto* file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(profile);
  auto filesystem_url =
      FilePathToFileSystemURL(profile, file_system_context, path_);
  if (!filesystem_url.is_valid()) {
    LOG(ERROR) << "Unable to get filesystem URL for deleted file";
    FinishDeletion(/*delete_successful=*/false);
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
    FinishDeletion(/*delete_successful=*/false);
    return;
  }
  io_task_controller_ = volume_manager->io_task_controller();
  if (!io_task_controller_) {
    LOG(ERROR) << "Unable to get IO task controller for deleted file";
    FinishDeletion(/*delete_successful=*/false);
    return;
  }
  task_id_ = io_task_controller_->Add(std::move(task));
  io_task_controller_->AddObserver(this);
}

void OdfsFileDeleter::FinishDeletion(bool delete_successful) {
  if (io_task_controller_) {
    io_task_controller_->RemoveObserver(this);
    io_task_controller_ = nullptr;
  }

  if (callback_) {
    std::move(callback_).Run(delete_successful);
  }

  // Delete this object as the task is complete and callback is executed.
  delete this;
}

void OdfsFileDeleter::OnIOTaskStatus(
    const ::file_manager::io_task::ProgressStatus& status) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M160);

  if (status.task_id != task_id_) {
    return;
  }

  // Determine the delete_successful result based on the task state.
  bool delete_successful = false;
  switch (status.state) {
    case file_manager::io_task::State::kQueued:
    case file_manager::io_task::State::kScanning:
    case file_manager::io_task::State::kInProgress:
    case file_manager::io_task::State::kPaused:
      // Ignore intermediate states.
      return;
    case file_manager::io_task::State::kSuccess:
      delete_successful = true;
      break;
    case file_manager::io_task::State::kError:
    case file_manager::io_task::State::kNeedPassword:
    case file_manager::io_task::State::kCancelled:
      delete_successful = false;
      LOG(ERROR) << "OneDrive file deletion failed with state: "
                 << status.state;
      break;
  }

  FinishDeletion(delete_successful);
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

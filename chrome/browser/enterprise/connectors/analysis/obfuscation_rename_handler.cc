// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/obfuscation_rename_handler.h"

#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_item.h"
#include "components/enterprise/obfuscation/core/download_obfuscator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace enterprise_obfuscation {

namespace {

storage::FileSystemURL FilePathToFileSystemURL(
    Profile* profile,
    storage::FileSystemContext* file_system_context,
    const base::FilePath& file_path) {
  GURL url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, file_path, file_manager::util::GetFileManagerURL(), &url)) {
    return storage::FileSystemURL();
  }
  return file_system_context->CrackURLInFirstPartyContext(url);
}

bool MoveAndOverwrite(const base::FilePath& from, const base::FilePath& to) {
  base::DeleteFile(to);
  if (!base::Move(from, to)) {
    if (base::CopyFile(from, to)) {
      base::DeleteFile(from);
      return true;
    }
    return false;
  }
  return true;
}

}  // namespace

// static
std::unique_ptr<ObfuscationRenameHandler>
ObfuscationRenameHandler::CreateIfNeeded(
    download::DownloadItem* download_item) {
  if (!download_item) {
    return nullptr;
  }

  auto* obfuscation_data = static_cast<DownloadObfuscationData*>(
      download_item->GetUserData(DownloadObfuscationData::kUserDataKey));
  if (!obfuscation_data || !obfuscation_data->is_obfuscated ||
      obfuscation_data->original_target_path.empty()) {
    return nullptr;
  }

  return std::make_unique<ObfuscationRenameHandler>(download_item);
}

ObfuscationRenameHandler::ObfuscationRenameHandler(
    download::DownloadItem* download_item)
    : download::DownloadItemRenameHandler(download_item) {}

ObfuscationRenameHandler::~ObfuscationRenameHandler() {
  if (io_task_controller_) {
    io_task_controller_->RemoveObserver(this);
  }
}

void ObfuscationRenameHandler::Start(ProgressCallback progress_callback,
                                     RenameCallback rename_callback) {
  auto* obfuscation_data = static_cast<DownloadObfuscationData*>(
      download_item_->GetUserData(DownloadObfuscationData::kUserDataKey));

  if (!obfuscation_data || obfuscation_data->original_target_path.empty()) {
    std::move(rename_callback)
        .Run(download::DOWNLOAD_INTERRUPT_REASON_NONE,
             download_item_->GetTargetFilePath());
    return;
  }

  destination_path_ = obfuscation_data->original_target_path;
  progress_callback_ = std::move(progress_callback);
  rename_callback_ = std::move(rename_callback);

  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(download_item_));
  if (profile && file_manager::VolumeManager::Get(profile) &&
      file_manager::VolumeManager::Get(profile)->io_task_controller()) {
    auto* file_system_context =
        file_manager::util::GetFileManagerFileSystemContext(profile);
    if (file_system_context) {
      storage::FileSystemURL source_url = FilePathToFileSystemURL(
          profile, file_system_context, download_item_->GetTargetFilePath());
      if (!source_url.is_valid()) {
        source_url = file_system_context->CreateCrackedFileSystemURL(
            blink::StorageKey(), storage::kFileSystemTypeLocal,
            download_item_->GetTargetFilePath());
      }
      storage::FileSystemURL destination_folder_url = FilePathToFileSystemURL(
          profile, file_system_context, destination_path_.DirName());

      if (source_url.is_valid() && destination_folder_url.is_valid()) {
        io_task_controller_ =
            file_manager::VolumeManager::Get(profile)->io_task_controller();
        io_task_controller_->AddObserver(this);

        auto task = std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
            file_manager::io_task::OperationType::kMove,
            std::vector<storage::FileSystemURL>{std::move(source_url)},
            std::vector<base::FilePath>{destination_path_.BaseName()},
            std::move(destination_folder_url), profile, file_system_context,
            /*show_notification=*/false);

        observed_task_id_ = io_task_controller_->Add(std::move(task));
        return;
      }
    }
  }

  // Fallback to thread pool move if IOTaskController is unavailable.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&MoveAndOverwrite, download_item_->GetTargetFilePath(),
                     destination_path_),
      base::BindOnce(&ObfuscationRenameHandler::OnThreadPoolMoveComplete,
                     weak_factory_.GetWeakPtr(), std::move(rename_callback_),
                     destination_path_));
}

bool ObfuscationRenameHandler::ShowRenameProgress() {
  return false;
}

void ObfuscationRenameHandler::OnIOTaskStatus(
    const file_manager::io_task::ProgressStatus& status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status.task_id != observed_task_id_) {
    return;
  }

  switch (status.state) {
    case file_manager::io_task::State::kInProgress:
      if (!progress_callback_.is_null() && status.bytes_transferred > 0) {
        progress_callback_.Run(status.bytes_transferred, 0);
      }
      return;
    case file_manager::io_task::State::kPaused:
      // If paused due to a destination filename conflict, automatically
      // resolve by replacing the destination file.
      if (status.pause_params.conflict_params.has_value() &&
          io_task_controller_) {
        file_manager::io_task::ConflictResumeParams conflict_params;
        conflict_params.conflict_resolve = "replace";
        conflict_params.conflict_apply_to_all = true;
        file_manager::io_task::ResumeParams resume_params;
        resume_params.conflict_params = std::move(conflict_params);
        io_task_controller_->Resume(status.task_id, std::move(resume_params));
      }
      return;
    case file_manager::io_task::State::kScanning:
    case file_manager::io_task::State::kQueued:
      return;
    case file_manager::io_task::State::kSuccess: {
      base::FilePath final_path = destination_path_;
      if (!status.outputs.empty() && status.outputs[0].url.is_valid()) {
        final_path = status.outputs[0].url.path();
      }
      if (!rename_callback_.is_null()) {
        std::move(rename_callback_)
            .Run(download::DOWNLOAD_INTERRUPT_REASON_NONE, final_path);
      }
      return;
    }
    case file_manager::io_task::State::kCancelled:
    case file_manager::io_task::State::kError:
    case file_manager::io_task::State::kNeedPassword: {
      if (!rename_callback_.is_null()) {
        std::move(rename_callback_)
            .Run(download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
                 destination_path_);
      }
      return;
    }
  }
}

void ObfuscationRenameHandler::OnThreadPoolMoveComplete(
    RenameCallback rename_callback,
    const base::FilePath& destination_path,
    bool success) {
  auto reason = success ? download::DOWNLOAD_INTERRUPT_REASON_NONE
                        : download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
  if (!rename_callback.is_null()) {
    std::move(rename_callback).Run(reason, destination_path);
  }
}

}  // namespace enterprise_obfuscation

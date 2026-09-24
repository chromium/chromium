// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/obfuscation_rename_handler.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/download/public/common/download_item.h"
#include "components/enterprise/obfuscation/core/download_obfuscator.h"

namespace enterprise_obfuscation {

namespace {

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
  // Don't check `is_obfuscated`: it is already cleared by deobfuscation, which
  // runs before a rename handler is first requested. `original_target_path` is
  // the signal that relocation is still pending.
  if (!obfuscation_data || obfuscation_data->original_target_path.empty()) {
    return nullptr;
  }

  return std::make_unique<ObfuscationRenameHandler>(download_item);
}

ObfuscationRenameHandler::ObfuscationRenameHandler(
    download::DownloadItem* download_item)
    : download::DownloadItemRenameHandler(download_item) {}

ObfuscationRenameHandler::~ObfuscationRenameHandler() = default;

void ObfuscationRenameHandler::Start(ProgressCallback /*progress_callback*/,
                                     RenameCallback rename_callback) {
  auto* obfuscation_data = static_cast<DownloadObfuscationData*>(
      download_item_->GetUserData(DownloadObfuscationData::kUserDataKey));

  if (!obfuscation_data || obfuscation_data->original_target_path.empty()) {
    std::move(rename_callback)
        .Run(download::DOWNLOAD_INTERRUPT_REASON_NONE,
             download_item_->GetTargetFilePath());
    return;
  }

  // A handler only exists for downloads that
  // ChromeDownloadManagerDelegate::DetermineLocalPath() staged in
  // base::GetTempDir(), so the source never maps to a FileSystemURL and a file
  // manager IOTask cannot service the move. A plain move suffices: the
  // destination passed IsVirtualFilesystem(), which only accepts FUSE mounts
  // under /media/fuse, and those are reachable through the local filesystem.
  const base::FilePath destination_path =
      obfuscation_data->original_target_path;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&MoveAndOverwrite, download_item_->GetTargetFilePath(),
                     destination_path),
      base::BindOnce(&ObfuscationRenameHandler::OnMoveComplete,
                     weak_factory_.GetWeakPtr(), std::move(rename_callback),
                     destination_path));
}

bool ObfuscationRenameHandler::ShowRenameProgress() {
  return false;
}

void ObfuscationRenameHandler::OnMoveComplete(
    RenameCallback rename_callback,
    const base::FilePath& destination_path,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Move of obfuscated download failed. destination_dir="
               << destination_path.DirName();
  }
  auto reason = success ? download::DOWNLOAD_INTERRUPT_REASON_NONE
                        : download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
  if (!rename_callback.is_null()) {
    std::move(rename_callback).Run(reason, destination_path);
  }
}

}  // namespace enterprise_obfuscation

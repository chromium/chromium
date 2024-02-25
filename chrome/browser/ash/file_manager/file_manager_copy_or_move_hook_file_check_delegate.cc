// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_manager_copy_or_move_hook_file_check_delegate.h"

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "google_apis/common/task_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"

namespace file_manager {

FileManagerCopyOrMoveHookFileCheckDelegate::
    FileManagerCopyOrMoveHookFileCheckDelegate(
        scoped_refptr<storage::FileSystemContext> file_system_context,
        FileCheckCallback file_check_callback)
    : CopyOrMoveHookDelegate(),
      file_system_context_(file_system_context),
      file_check_callback_(file_check_callback) {
  DCHECK(!file_check_callback.is_null());
}

FileManagerCopyOrMoveHookFileCheckDelegate::
    ~FileManagerCopyOrMoveHookFileCheckDelegate() = default;

void FileManagerCopyOrMoveHookFileCheckDelegate::OnBeginProcessFile(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    storage::FileSystemOperation::StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // On BeginProcessFile is also called for the root directory, so we check
  // whether the passed source_url is a directory or not.
  file_system_context_->operation_runner()->GetMetadata(
      source_url,
      {storage::FileSystemOperation::GetMetadataField::kIsDirectory},
      base::BindOnce(&FileManagerCopyOrMoveHookFileCheckDelegate::
                         OnBeginProcessFileGotMetadata,
                     weak_factory_.GetWeakPtr(), source_url, destination_url,
                     std::move(callback)));
}

void FileManagerCopyOrMoveHookFileCheckDelegate::OnBeginProcessFileGotMetadata(
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    storage::FileSystemOperation::StatusCallback callback,
    base::File::Error result,
    const base::File::Info& file_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (file_info.is_directory) {
    // Scanning is performed only for files, so we don't block any directory.
    // The files inside of a directory will be checked separately.
    std::move(callback).Run(base::File::FILE_OK);
    return;
  }

  // `file_check_callback` is run on the UI thread.
  // `callback` should be called on the current (IO) thread.
  file_check_callback_.Run(
      src_url, dest_url, google_apis::CreateRelayCallback(std::move(callback)));
}

}  // namespace file_manager

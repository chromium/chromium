// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_manager_copy_or_move_hook_delegate.h"

#include <ios>

#include "base/files/file.h"
#include "base/notreached.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_url.h"

namespace file_manager {

FileManagerCopyOrMoveHookDelegate::FileManagerCopyOrMoveHookDelegate(
    ProgressCallback progress_callback)
    : progress_callback_(std::move(progress_callback)) {
  DCHECK(!progress_callback_.is_null());
}

FileManagerCopyOrMoveHookDelegate::~FileManagerCopyOrMoveHookDelegate() =
    default;

void FileManagerCopyOrMoveHookDelegate::OnBeginProcessFile(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  progress_callback_.Run(ProgressType::kBegin, source_url, destination_url,
                         /*size=*/0);
  std::move(callback).Run(base::File::FILE_OK);
}

void FileManagerCopyOrMoveHookDelegate::OnBeginProcessDirectory(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  progress_callback_.Run(ProgressType::kBegin, source_url, destination_url,
                         /*size=*/0);
  std::move(callback).Run(base::File::FILE_OK);
}

void FileManagerCopyOrMoveHookDelegate::OnProgress(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    int64_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  progress_callback_.Run(ProgressType::kProgress, source_url, destination_url,
                         size);
}

void FileManagerCopyOrMoveHookDelegate::OnError(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    base::File::Error error,
    ErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  progress_callback_.Run(ProgressType::kError, source_url, destination_url,
                         /*size=*/0);
  std::move(callback).Run(ErrorAction::kDefault);
}

void FileManagerCopyOrMoveHookDelegate::OnEndCopy(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  progress_callback_.Run(ProgressType::kEndCopy, source_url, destination_url,
                         /*size=*/0);
}

void FileManagerCopyOrMoveHookDelegate::OnEndMove(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  progress_callback_.Run(ProgressType::kEndMove, source_url, destination_url,
                         /*size=*/0);
}

void FileManagerCopyOrMoveHookDelegate::OnEndRemoveSource(
    const storage::FileSystemURL& source_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  progress_callback_.Run(ProgressType::kEndRemoveSource, source_url,
                         /*destination_url=*/storage::FileSystemURL(),
                         /*size=*/0);
}

std::ostream& operator<<(
    std::ostream& out,
    const FileManagerCopyOrMoveHookDelegate::ProgressType& type) {
  out << "ProgressType::";

  using ProgressType = FileManagerCopyOrMoveHookDelegate::ProgressType;
  switch (type) {
    case ProgressType::kBegin:
      return out << "kBegin";
    case ProgressType::kProgress:
      return out << "kProgress";
    case ProgressType::kEndCopy:
      return out << "kEndCopy";
    case ProgressType::kEndMove:
      return out << "kEndMove";
    case ProgressType::kEndRemoveSource:
      return out << "kEndRemoveSource";
    case ProgressType::kError:
      return out << "kError";
  }

  NOTREACHED_IN_MIGRATION();
  return out << "Unknown type";
}

}  // namespace file_manager

// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_IO_TASK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/file_manager_copy_or_move_hook_delegate.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/speedometer.h"
#include "chrome/browser/profiles/profile.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace file_manager {
namespace io_task {

// This class represents a copy or move operation. It checks whether there is
// enough space for the copy or move to occur, and also sends the copy or move
// requests to the storage backend.
class CopyOrMoveIOTask : public IOTask {
 public:
  // |type| must be either kCopy or kMove.
  CopyOrMoveIOTask(
      OperationType type,
      std::vector<storage::FileSystemURL> source_urls,
      storage::FileSystemURL destination_folder,
      Profile* profile,
      scoped_refptr<storage::FileSystemContext> file_system_context);
  ~CopyOrMoveIOTask() override;

  // Starts the copy or move.
  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;

  void Cancel() override;

  bool IsCrossFileSystemForTesting(
      const storage::FileSystemURL& source_url,
      const storage::FileSystemURL& destination_url);

 private:
  bool IsCrossFileSystem(const storage::FileSystemURL& source_url,
                         const storage::FileSystemURL& destination_url);
  void Complete(State state);
  void GetFileSize(size_t idx);
  void GotFileSize(size_t idx,
                   base::File::Error error,
                   const base::File::Info& file_info);
  void GotFreeDiskSpace(int64_t free_space);
  void GenerateDestinationURL(size_t idx);
  void CopyOrMoveFile(
      size_t idx,
      base::FileErrorOr<storage::FileSystemURL> destination_result);
  void OnCopyOrMoveProgress(
      FileManagerCopyOrMoveHookDelegate::ProgressType type,
      const storage::FileSystemURL& source_url,
      const storage::FileSystemURL& destination_url,
      int64_t size);
  void OnCopyOrMoveComplete(size_t idx, base::File::Error error);
  void SetCurrentOperationID(
      storage::FileSystemOperationRunner::OperationID id);

  Profile* profile_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;

  // Stores the size of each source so we know what to increment the progress
  // bytes by for each copy or move completion.
  std::vector<int64_t> source_sizes_;

  // Stores the size reported by the last progress update so we can compute the
  // delta on the next progress update.
  int64_t last_progress_size_;

  // Stores the id of the copy or move operation if one is in progress. Used so
  // the transfer can be cancelled.
  absl::optional<storage::FileSystemOperationRunner::OperationID> operation_id_;

  // Speedometer for this operation, used to calculate the remaining time to
  // finish the operation.
  Speedometer speedometer_;

  ProgressCallback progress_callback_;
  CompleteCallback complete_callback_;

  base::WeakPtrFactory<CopyOrMoveIOTask> weak_ptr_factory_{this};
};

}  // namespace io_task
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_IO_TASK_H_

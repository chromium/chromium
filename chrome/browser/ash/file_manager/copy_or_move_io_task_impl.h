// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_IO_TASK_IMPL_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_IO_TASK_IMPL_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
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

namespace file_manager::io_task {

// Moving or copying a folder with multiple nested files and folders is treated
// as a single operation (moving or copying the top level folder). However,
// progress is reported at an individual level (that is, for each nested file).
// This struct lets this io_task keep track of both the aggregate progress of
// top folders and that of their individual items.
struct ItemProgress {
  ItemProgress();
  ~ItemProgress();

  base::flat_map<std::string, int64_t> individual_progress;
  int64_t aggregate_progress = 0;
};

// This class represents a copy or move operation. It checks whether there is
// enough space for the copy or move to occur, and also sends the copy or move
// requests to the storage backend.
class CopyOrMoveIOTaskImpl {
 public:
  using ProgressCallback = IOTask::ProgressCallback;
  using CompleteCallback = IOTask::CompleteCallback;

  // Use this constructor if you require the destination entries to have
  // different file names to the source entries. The size of `source_urls` and
  // `destination_file_names` must be the same.
  // |type| must be either kCopy or kMove.
  CopyOrMoveIOTaskImpl(
      OperationType type,
      ProgressStatus& progress,
      std::vector<base::FilePath> destination_file_names,
      storage::FileSystemURL destination_folder,
      Profile* profile,
      scoped_refptr<storage::FileSystemContext> file_system_context,
      bool show_notification = true);
  virtual ~CopyOrMoveIOTaskImpl();

  // Starts the copy or move.
  virtual void Execute(ProgressCallback progress_callback,
                       CompleteCallback complete_callback);

  // Cancels the copy or move.
  void Cancel();

  // Helper function for copy or move tasks that determines whether or not
  // entries identified by their URLs should be considered as being on the
  // different file systems or not. The entries are seen as being on different
  // filesystems if either:
  // - the entries are not on the same volume OR
  // - one entry is in My files, and the other one in Downloads.
  // crbug.com/1200251
  static bool IsCrossFileSystemForTesting(
      Profile* profile,
      const storage::FileSystemURL& source_url,
      const storage::FileSystemURL& destination_url);

 protected:
  // Starts the actual file transfer. Should be called after the checks of
  // `VerifyTransfer` are completed. Protected to be called from child classes.
  void StartTransfer();

  // Function that converts a progress notified from the
  // `FileManagerCopyOrMoveHookDelegate` to one understandable by
  // `progress_callback_`.
  void OnCopyOrMoveProgress(
      size_t idx,
      FileManagerCopyOrMoveHookDelegate::ProgressType type,
      const storage::FileSystemURL& source_url,
      const storage::FileSystemURL& destination_url,
      int64_t size);

  // The current progress state.
  // The reference is allowed here, as the owning object (CopyOrMoveIOTask) is
  // guaranteed to outlive the CopyOrMoveIOTaskImpl.
  ProgressStatus& progress_;

  // ProgressCallback for this operation, used to notify the UI of the current
  // progress.
  ProgressCallback progress_callback_;

 private:
  // Verifies the transfer, e.g., by using enterprise connectors for checking
  // whether a transfer is allowed.
  virtual void VerifyTransfer();
  // Returns the error behavior to be used for the copy or move operation.
  virtual storage::FileSystemOperation::ErrorBehavior GetErrorBehavior();
  // Returns the storage::CopyOrMoveHookDelegate to be used for the copy or move
  // operation.
  virtual std::unique_ptr<storage::CopyOrMoveHookDelegate> GetHookDelegate(
      size_t idx);

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

  void OnCopyOrMoveComplete(size_t idx, base::File::Error error);
  void SetCurrentOperationID(
      storage::FileSystemOperationRunner::OperationID id);

  Profile* profile_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;

  // Specifies whether the operation is already completed.
  bool completed_ = false;

  // The number of file for which the file size has been fetched.
  size_t files_preprocessed_ = 0;

  // Stores the size of each source so we know what to increment the progress
  // bytes by for each copy or move completion.
  std::vector<int64_t> source_sizes_;

  // Stores a list of file names (i.e. base::FilePath::BaseName, not full paths)
  // that will serve as the name for the source URLs in progress_.sources. These
  // names are prior to conflict resolution so in the event they conflict they
  // may be renamed to include a numbered suffix (e.g. foo.txt (1)). The
  // std::vector::size here MUST be the same as progress_.sources size.
  std::vector<base::FilePath> destination_file_names_;

  // Stores the sizes reported by the last progress update so we can compute the
  // delta on the next progress update.
  std::vector<ItemProgress> item_progresses;

  // Stores the id of the copy or move operation if one is in progress. Used so
  // the transfer can be cancelled.
  absl::optional<storage::FileSystemOperationRunner::OperationID> operation_id_;

  // Speedometer for this operation, used to calculate the remaining time to
  // finish the operation.
  Speedometer speedometer_;

  // CompleteCallback for this operation, used to notify the UI when this
  // operation is completed.
  CompleteCallback complete_callback_;

  base::WeakPtrFactory<CopyOrMoveIOTaskImpl> weak_ptr_factory_{this};
};

}  // namespace file_manager::io_task

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_IO_TASK_IMPL_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_DELETE_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_DELETE_IO_TASK_H_

#include <optional>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"

namespace file_manager {
namespace io_task {

// This class represents a delete task. A delete task attempts to delete zero or
// more files specified by their FileSystemURL objects. As ProgressStatus has
// fields for total_bytes and bytes_transferred, it uses the convention of
// setting total_bytes to the number of files that need to be deleted and
// bytes_transferred as the number of files deleted so far.
class DeleteIOTask : public IOTask {
 public:
  DeleteIOTask(std::vector<storage::FileSystemURL> file_urls,
               scoped_refptr<storage::FileSystemContext> file_system_context,
               bool show_notification = true);
  ~DeleteIOTask() override;

  // Starts the delete.
  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;

  void Cancel() override;

 private:
  void Complete(State state);
  void DeleteFile(size_t idx);
  void OnDeleteComplete(size_t idx, base::File::Error error);
  void SetCurrentOperationID(
      storage::FileSystemOperationRunner::OperationID id);

  scoped_refptr<storage::FileSystemContext> file_system_context_;

  // Stores the id of the copy operation if one is in progress. Used so the
  // delete can be cancelled.
  std::optional<storage::FileSystemOperationRunner::OperationID> operation_id_;

  ProgressCallback progress_callback_;
  CompleteCallback complete_callback_;

  base::WeakPtrFactory<DeleteIOTask> weak_ptr_factory_{this};
};

}  // namespace io_task
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_DELETE_IO_TASK_H_

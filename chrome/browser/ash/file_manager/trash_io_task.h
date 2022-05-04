// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_IO_TASK_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace file_manager {
namespace io_task {

// This class represents a trash task. A trash task attempts to trash zero or
// more files by first moving them to a .Trash/files or .Trash-{UID}/files
// folder that resides in a parent folder for:
//   - My files
//   - Downloads
//   - Crostini
//   - Drive
// A corresponding .trashinfo file will be created at .Trash/info or
// .Trash-{UID}/info that contains a timestamp of deletion and the original path
// (relative to the trash folder location).
//
// This follows the XDG specification
// https://specifications.freedesktop.org/trash-spec/trashspec-1.0.html
class TrashIOTask : public IOTask {
 public:
  TrashIOTask(std::vector<storage::FileSystemURL> file_urls,
              Profile* profile,
              scoped_refptr<storage::FileSystemContext> file_system_context);
  ~TrashIOTask() override;

  // Starts trash trask.
  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;
  void Cancel() override;

 private:
  scoped_refptr<storage::FileSystemContext> file_system_context_;

  // Stores the id of the trash operation if one is in progress. Used so the
  // trash can be cancelled.
  absl::optional<storage::FileSystemOperationRunner::OperationID> operation_id_;

  ProgressCallback progress_callback_;
  CompleteCallback complete_callback_;

  base::WeakPtrFactory<TrashIOTask> weak_ptr_factory_{this};
};

}  // namespace io_task
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_IO_TASK_H_

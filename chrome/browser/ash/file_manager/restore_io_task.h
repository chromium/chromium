// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_RESTORE_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_RESTORE_IO_TASK_H_

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

// This class represents a task restoring from trash. A restore task attempts to
// restore files from a supported Trash folder back to it's original path. If
// the path no longer exists, it will recursively create the folder structure
// until the file or folder can be moved.
//
// This follows the XDG specification
// https://specifications.freedesktop.org/trash-spec/trashspec-1.0.html
class RestoreIOTask : public IOTask {
 public:
  RestoreIOTask(std::vector<storage::FileSystemURL> file_urls,
                Profile* profile,
                scoped_refptr<storage::FileSystemContext> file_system_context,
                const base::FilePath base_path);
  ~RestoreIOTask() override;

  // Starts restore trask.
  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;
  void Cancel() override;

 private:
  // Finalises the RestoreIOTask with the `state`.
  void Complete(State state);

  // Ensure the metadata file conforms to the following:
  //   - Has a .trashinfo suffix
  // TODO(b/231830250): Implement the remaining validations:
  //   - Resides in an enabled trash directory
  //   - The file resides in the info directory
  //   - Has an identical item in the files directory with no .trashinfo suffix
  void ValidateTrashInfo(size_t idx);

  // Parse the .trashinfo file and ensure it conforms to the XDG specification
  // before restoring the file.
  // TODO(b/231830250): Finish implementation of this method.
  void ParseTrashInfoFile(size_t idx,
                          const base::FilePath& trash_info_file_path);

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  raw_ptr<Profile> profile_;

  // Represents the parent path that all the source URLs descend from. Used to
  // work around the fact `FileSystemOperationRunner` requires relative paths
  // only in testing.
  base::FilePath base_path_;

  // Stores the id of the restore operation if one is in progress. Used so the
  // restore can be cancelled.
  absl::optional<storage::FileSystemOperationRunner::OperationID> operation_id_;

  ProgressCallback progress_callback_;
  CompleteCallback complete_callback_;

  base::WeakPtrFactory<RestoreIOTask> weak_ptr_factory_{this};
};

}  // namespace io_task
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_RESTORE_IO_TASK_H_

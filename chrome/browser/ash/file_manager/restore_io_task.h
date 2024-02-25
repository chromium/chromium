// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_RESTORE_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_RESTORE_IO_TASK_H_

#include <optional>
#include <vector>

#include "base/files/file_error_or.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/ash/file_manager/trash_info_validator.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

namespace file_manager::io_task {

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
                const base::FilePath base_path,
                bool show_notification = true);
  ~RestoreIOTask() override;

  // Starts restore task.
  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;
  void Cancel() override;

 private:
  // Finalises the RestoreIOTask with the `state`.
  void Complete(State state);

  // Calls the underlying TrashInfoValidator to perform validation on the
  // supplied .trashinfo file.
  void ValidateTrashInfo(size_t idx);

  // Make sure the enclosing folder where the trashed file to be restored to
  // actually exists. In the event the file path has been removed, recreate it.
  void EnsureParentRestorePathExists(
      size_t idx,
      trash::ParsedTrashInfoDataOrError parsed_data_or_error);

  void OnParentRestorePathExists(size_t idx,
                                 const base::FilePath& trashed_file_location,
                                 const base::FilePath& absolute_restore_path,
                                 base::File::Error status);

  // Make sure the destination file name is unique before moving, creates unique
  // file names by appending (N) to the end of a file name.
  void GenerateDestinationURL(size_t idx,
                              const base::FilePath& trashed_file_location,
                              const base::FilePath& absolute_restore_path);

  // Move the item from `trashed_file_location` to `destination_result`.
  void RestoreItem(
      size_t idx,
      base::FilePath trashed_file_location,
      base::FileErrorOr<storage::FileSystemURL> destination_result);

  void OnRestoreItem(size_t idx, base::File::Error error);

  // Once a restore has completed, kick off the next restore `idx` or finish.
  void RestoreComplete(size_t idx, base::File::Error error);

  void SetCurrentOperationID(
      storage::FileSystemOperationRunner::OperationID id);

  base::FilePath MakeRelativeFromBasePath(const base::FilePath& absolute_path);

  const storage::FileSystemURL CreateFileSystemURL(
      const storage::FileSystemURL& original_url,
      const base::FilePath& path);

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  raw_ptr<Profile> profile_;

  // Represents the parent path that all the source URLs descend from. Used to
  // work around the fact `FileSystemOperationRunner` requires relative paths
  // only in testing.
  base::FilePath base_path_;

  // Stores the id of the restore operation if one is in progress. Used so the
  // restore can be cancelled.
  std::optional<storage::FileSystemOperationRunner::OperationID> operation_id_;

  // Validates and parses .trashinfo files.
  std::unique_ptr<trash::TrashInfoValidator> validator_ = nullptr;

  ProgressCallback progress_callback_;
  CompleteCallback complete_callback_;

  base::WeakPtrFactory<RestoreIOTask> weak_ptr_factory_{this};
};

}  // namespace file_manager::io_task

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_RESTORE_IO_TASK_H_

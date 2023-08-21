// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_RESTORE_TO_DESTINATION_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_RESTORE_TO_DESTINATION_IO_TASK_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/trash_info_validator.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

namespace file_manager::io_task {

// This class represents a task restoring from trash. A restore task attempts to
// restore files from a supported Trash folder to the supplied destination
// folder. The validation of the supplied .trashinfo files occurs here but the
// actual move operation is delegated to the `CopyOrMoveIOTask`.
//
// This follows the XDG specification
// https://specifications.freedesktop.org/trash-spec/trashspec-1.0.html
class RestoreToDestinationIOTask : public IOTask {
 public:
  RestoreToDestinationIOTask(
      std::vector<storage::FileSystemURL> file_urls,
      storage::FileSystemURL destination_folder,
      Profile* profile,
      scoped_refptr<storage::FileSystemContext> file_system_context,
      const base::FilePath base_path,
      bool show_notification);
  ~RestoreToDestinationIOTask() override;

  // Starts restore to destination task.
  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;

  // Passes the Pause on to the underlying `move_io_task_` if it exists.
  void Pause(PauseParams params) override;

  // Passes the Resume on to the underlying `move_io_task_` if it exists.
  void Resume(ResumeParams params) override;

  // Passes the Cancel on to the underlying `move_io_task_` if it exists.
  void Cancel() override;

  // Returns a pointer to the underlying `move_io_task_`.
  CopyOrMoveIOTask* GetMoveTaskForTesting();

 private:
  // Finalises the RestoreToDestinationIOTask with the `state`.
  // NOTE: This IOTask delegates the actual move operation to the underlying
  // CopyOrMoveIOTask, so once all .trashinfo files are parsed the callbacks are
  // passed to the next IOTask and this Complete method is no longer called.
  void Complete(State state);

  // Calls the underlying TrashInfoValidator to perform validation on the
  // supplied .trashinfo file.
  void ValidateTrashInfo(size_t idx);

  // After parsing the .trashinfo, ensure the returned data is valid. Once all
  // .trashinfo's are parsed, create and delegate the move operation to the
  // `move_io_task`
  void OnTrashInfoParsed(
      size_t idx,
      trash::ParsedTrashInfoDataOrError parsed_data_or_error);

  // Used to intercept the `move_io_task_` ProgressCallback and ensure the
  // progress is coming from the current task with the relevant task_id.
  void OnProgressCallback(const ProgressStatus& status);

  // Used to intercept the `move_io_task_` CompleteCallback and ensure the
  // progress is coming from the current task with the relevant task_id.
  void OnCompleteCallback(ProgressStatus status);

  base::FilePath MakeRelativeFromBasePath(const base::FilePath& absolute_path);

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  raw_ptr<Profile> profile_;

  // Represents the parent path that all the source URLs descend from. Used to
  // work around the fact `FileSystemOperationRunner` requires relative paths
  // only in testing.
  base::FilePath base_path_;

  // The file names are extracted from the supplied .trashinfo files and passed
  // into the `move_io_task_` to ensure the restoration uses the restore file
  // name instead of the on-disk file name.
  std::vector<base::FilePath> destination_file_names_;

  // Maintain a copy of the `source_urls_` to enable passing to the
  // `move_io_task_` without trying to extract them back out of the
  // `progress_.sources`.
  std::vector<storage::FileSystemURL> source_urls_;

  // The `CopyOrMoveIOTask` is the underlying move operation that is called once
  // the .trashinfo files are successfully parsed.
  std::unique_ptr<CopyOrMoveIOTask> move_io_task_ = nullptr;

  // Validates and parses .trashinfo files.
  std::unique_ptr<trash::TrashInfoValidator> validator_ = nullptr;

  ProgressCallback progress_callback_;
  CompleteCallback complete_callback_;

  base::WeakPtrFactory<RestoreToDestinationIOTask> weak_ptr_factory_{this};
};

}  // namespace file_manager::io_task

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_RESTORE_TO_DESTINATION_IO_TASK_H_

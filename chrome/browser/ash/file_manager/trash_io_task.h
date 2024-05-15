// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_IO_TASK_H_

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "base/files/file_error_or.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/file_manager_copy_or_move_hook_delegate.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chromeos/ash/components/file_manager/speedometer.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

namespace file_manager::io_task {

namespace {

struct TrashEntry {
  TrashEntry();
  ~TrashEntry();

  TrashEntry(TrashEntry&& other);
  TrashEntry& operator=(TrashEntry&& other);

  // The relative path (to `trash_mount_path`) where the final location of the
  // trashed file.
  base::FilePath relative_trash_path;

  // An absolute location which contains the `relative_trash_path` and combined
  // represents the final location of the trashed file.
  base::FilePath trash_mount_path;

  // The date of deletion, stored in the metadata file to help scheduled
  // cleanup.
  base::Time deletion_time;

  // The contents of the .trashinfo file containing the deletion date and
  // original path of the trashed file.
  std::string trash_info_contents;

  // The source file size.
  int64_t source_file_size;
};

}  // namespace

// This class represents a trash task. A trash task attempts to trash zero or
// more files by first moving them to a .Trash/files or .Trash-{UID}/files
// folder that resides in a parent folder for:
//   - MyFiles
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
              scoped_refptr<storage::FileSystemContext> file_system_context,
              const base::FilePath base_path,
              bool show_notification = true);
  ~TrashIOTask() override;

  // Starts trash trask.
  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;
  void Cancel() override;

 private:
  void Complete(State state);
  void UpdateTrashEntry(size_t source_idx);
  void GetFileSize(size_t source_idx);
  void GotFileSize(size_t source_idx,
                   base::File::Error error,
                   const base::File::Info& file_info);
  const storage::FileSystemURL CreateFileSystemURL(
      const storage::FileSystemURL& original_url,
      const base::FilePath& path);
  void SetCurrentOperationID(
      storage::FileSystemOperationRunner::OperationID id);
  void ValidateAndDecrementFreeSpace(
      size_t source_idx,
      const trash::TrashPathsMap::reverse_iterator& it);
  // Get the free disk space for `trash_parent_path` to know whether the
  // metadata can be written. The `folder_name` is used to differentiate between
  // .Trash and .Trash-1000 folder names on various file systems (both are valid
  // in the XDG spec).
  void GetFreeDiskSpace(size_t source_idx,
                        const trash::TrashPathsMap::reverse_iterator& it);
  void GotFreeDiskSpace(size_t source_idx,
                        const trash::TrashPathsMap::reverse_iterator& it,
                        int64_t free_space);

  // Sets up the .Trash/files and .Trash/info subdirectories specified by the
  // `trash_subdirectory` parameter. Will create the parent directories as well
  // in the instance .Trash folder does not exist.
  void SetupSubDirectory(trash::TrashPathsMap::const_iterator& it,
                         const storage::FileSystemURL trash_subdirectory);

  // During low-disk space situations, cryptohome needs a way to identify
  // folders to purge. Trash should be considered prior to the rest of the
  // users' profile.
  void SetDirectoryTracking(
      base::OnceCallback<void(base::File::Error)> on_setup_complete_callback,
      const base::FilePath& trash_subdirectory,
      base::File::Error error);
  void OnSetupSubDirectory(trash::TrashPathsMap::const_iterator& it,
                           const storage::FileSystemURL trash_subdirectory,
                           base::File::Error error);

  // After setting up directory permissions, `set_permissions_success` will have
  // true on success and false otherwise.
  void OnSetDirectoryPermissions(trash::TrashPathsMap::const_iterator& it,
                                 bool set_permissions_success);
  base::FilePath MakeRelativeFromBasePath(const base::FilePath& absolute_path);
  base::FilePath MakeRelativePathAbsoluteFromBasePath(
      const base::FilePath& relative_path);

  // Attempts to generate a unique destination filename when saving to
  // .Trash/files. Appends an increasing (N) suffix until a unique name is
  // identified.
  void GenerateDestinationURL(size_t source_idx, size_t output_idx);

  // Creates a file in .Trash/info that matches the name generated from
  // `GenerateDestinationURL`. Writes the relative restoration path as well as
  // the date time of deletion.
  void WriteMetadata(
      size_t source_idx,
      size_t output_idx,
      const storage::FileSystemURL& files_folder_location,
      base::FileErrorOr<storage::FileSystemURL> destination_result);
  void OnWriteMetadata(size_t source_idx,
                       size_t output_idx,
                       const storage::FileSystemURL& destination_url,
                       bool success);

  // Called upon either error of writing metadata or completion of moving the
  // trashed file. Ensures progress is invoked and the next file is queued.
  void TrashComplete(size_t source_idx,
                     size_t output_idx,
                     base::File::Error error);
  // Move a file from it's location to the final .Trash/files destination with
  // a unique name determined by `GenerateDestinationURL`.
  void TrashFile(size_t source_idx,
                 size_t output_idx,
                 const storage::FileSystemURL& destination_url);
  void OnMoveComplete(size_t source_idx,
                      size_t output_idx,
                      base::File::Error error);

  raw_ptr<Profile> profile_;

  scoped_refptr<storage::FileSystemContext> file_system_context_;

  // Stores the total size required for all the metadata files, keyed by the
  // trash location to ensure enough disk space to write.
  std::vector<TrashEntry> trash_entries_;

  // Maintains the free space required to write all the metadata files along
  // with the underlying locations of the .Trash/{files,info} directories.
  trash::TrashPathsMap free_space_map_;

  // Stores the size reported by the last progress update so we can compute the
  // delta on the next progress update.
  int64_t last_progress_size_;

  // Stores the last url for the most recently updated metadata file, in the
  // event of a move failure this file is removed.
  storage::FileSystemURL last_metadata_url_;

  // Speedometer for this operation, used to calculate the remaining time to
  // finish the operation.
  Speedometer speedometer_;

  // Stores the id of the operations currently behind undertaken by Trash,
  // including directory creation. Enables cancelling an inflight operation.
  std::optional<storage::FileSystemOperationRunner::OperationID> operation_id_;

  ProgressCallback progress_callback_;
  CompleteCallback complete_callback_;

  // Represents the parent path that all the source URLs descend from. Used to
  // work around the fact `FileSystemOperationRunner` requires relative paths
  // only in testing.
  const base::FilePath base_path_;

  base::WeakPtrFactory<TrashIOTask> weak_ptr_factory_{this};
};

}  // namespace file_manager::io_task

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_IO_TASK_H_

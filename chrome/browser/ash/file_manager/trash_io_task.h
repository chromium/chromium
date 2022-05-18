// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_IO_TASK_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace file_manager {
namespace io_task {

namespace {

struct TrashEntry {
  TrashEntry();
  ~TrashEntry();

  TrashEntry(TrashEntry&& other);
  TrashEntry& operator=(TrashEntry&& other);

  // The final location for the trashed file, this may not be the same as the
  // `url` in the case of naming conflicts.
  base::FilePath trash_path;

  // The date of deletion, stored in the metadata file to help scheduled
  // cleanup.
  base::Time deletion_time;

  // The contents of the .trashinfo file containing the deletion date and
  // original path of the trashed file.
  std::string trash_info_contents;

  // The source file size.
  int64_t source_file_size;
};

struct DirectoryInfo {
  explicit DirectoryInfo(int64_t free_space);
  ~DirectoryInfo();

  DirectoryInfo(DirectoryInfo&& other);
  DirectoryInfo& operator=(DirectoryInfo&& other);

  // TODO(b/231830211): Store the .Trash/{files/info} storage::FileSystemURLs to
  // enable creating the directory on the file_system_context_.

  // The free space on the underlying filesystem that .Trash is located on.
  int64_t free_space;
};

}  // namespace

// Constant representing the Trash folder name.
extern const char kTrashFolderName[];

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
              scoped_refptr<storage::FileSystemContext> file_system_context,
              const base::FilePath base_path);
  ~TrashIOTask() override;

  // Starts trash trask.
  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;
  void Cancel() override;

 private:
  void Complete(State state);
  void UpdateTrashEntry(size_t idx);
  void GetFileSize(size_t idx);
  void GotFileSize(size_t idx,
                   base::File::Error error,
                   const base::File::Info& file_info);
  using FreeSpaceMap = std::map<base::FilePath, DirectoryInfo>;
  void ValidateAndDecrementFreeSpace(size_t idx, FreeSpaceMap::iterator& it);
  void GetFreeDiskSpace(size_t idx, const base::FilePath& trash_parent_path);
  void GotFreeDiskSpace(size_t idx,
                        const base::FilePath& trash_parent_path,
                        int64_t free_space);

  raw_ptr<Profile> profile_;

  scoped_refptr<storage::FileSystemContext> file_system_context_;

  // Stores the total size required for all the metadata files, keyed by the
  // trash location to ensure enough disk space to write.
  std::vector<TrashEntry> trash_entries_;

  // Maintains the free space required to write all the metadata files along
  // with the underlying locations of the .Trash/{files,info} directories.
  FreeSpaceMap free_space_map_;

  // Stores the id of the trash operation if one is in progress. Used so the
  // trash can be cancelled.
  absl::optional<storage::FileSystemOperationRunner::OperationID> operation_id_;

  ProgressCallback progress_callback_;
  CompleteCallback complete_callback_;

  // Represents the parent path that all the source URLs descend from. Used to
  // work around the fact `FileSystemOperationRunner` requires relative paths
  // only in testing.
  const base::FilePath base_path_;

  base::WeakPtrFactory<TrashIOTask> weak_ptr_factory_{this};
};

}  // namespace io_task
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_IO_TASK_H_

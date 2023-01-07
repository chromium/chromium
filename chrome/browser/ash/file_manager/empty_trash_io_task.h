// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_EMPTY_TRASH_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_EMPTY_TRASH_IO_TASK_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace blink {
class StorageKey;
}  // namespace blink

namespace file_manager::io_task {

// This class represents a task to completely empty the trash folder. An empty
// trash task will recursively iterate over the contents of the "files" and
// "info" subdirectories and remove all the files within.
// The "Empty Trash" action happens for all enabled trash locations, not just
// for one location. Therefore no sources are supplied they are identified and
// emptied accordingly. The locations which are enabled and are emptied are
// available on the outputs of the `ProgressStatus`.
//
// This follows the XDG specification
// https://specifications.freedesktop.org/trash-spec/trashspec-1.0.html
class EmptyTrashIOTask : public IOTask {
 public:
  EmptyTrashIOTask(
      blink::StorageKey storage_key,
      Profile* profile,
      scoped_refptr<storage::FileSystemContext> file_system_context,
      base::FilePath base_path,
      bool show_notification = true);

  ~EmptyTrashIOTask() override;

  // Starts empty trash trask.
  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;
  void Cancel() override;

 private:
  // Removes the entire trash subdirectory (e.g. .Trash/files) recursively. It
  // only iterates over the enabled trash locations.
  void RemoveTrashSubDirectory(
      trash::TrashPathsMap::const_iterator& trash_location,
      const std::string& folder_name_to_remove);

  // After removing the trash directory, continue iterating until there are no
  // more enabled trash directories left.
  void OnRemoveTrashSubDirectory(trash::TrashPathsMap::const_iterator& it,
                                 const std::string& removed_folder_name,
                                 base::File::Error status);

  // Finish up and invoke the `complete_callback_`.
  void Complete(State state);

  void SetCurrentOperationID(
      storage::FileSystemOperationRunner::OperationID id);

  scoped_refptr<storage::FileSystemContext> file_system_context_;

  // Storage key used to construct the storage::FileSystemURLs that represent
  // the various trash locations when recursively removing the trash contents.
  const blink::StorageKey storage_key_;

  raw_ptr<Profile> profile_;

  // A map containing paths which are enabled for trashing.
  trash::TrashPathsMap enabled_trash_locations_;

  // Stores the id of the restore operation if one is in progress. Used to stop
  // the empty trash operation.
  absl::optional<storage::FileSystemOperationRunner::OperationID> operation_id_;

  ProgressCallback progress_callback_;
  CompleteCallback complete_callback_;

  // Represents the parent path that all the source URLs descend from. Used to
  // work around the fact `FileSystemOperationRunner` requires relative paths
  // only in testing.
  const base::FilePath base_path_;

  base::WeakPtrFactory<EmptyTrashIOTask> weak_ptr_factory_{this};
};

}  // namespace file_manager::io_task

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_EMPTY_TRASH_IO_TASK_H_

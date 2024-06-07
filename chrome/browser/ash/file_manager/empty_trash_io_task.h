// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_EMPTY_TRASH_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_EMPTY_TRASH_IO_TASK_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"

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

  // Starts emptying the trash bin.
  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;

  // Marks the whole operation as cancelled. Under the hood, the actual ongoing
  // deletion operations cannot be cancelled, and will continue until they
  // finish.
  void Cancel() override;

 private:
  // Called when a trash subdir has been removed.
  void OnRemoved(size_t i, bool ok);

  // Finish up and invoke the completion callback.
  void Complete();

  const scoped_refptr<storage::FileSystemContext> file_system_context_;

  // Storage key used to construct the storage::FileSystemURLs that represent
  // the various trash locations when recursively removing the trash contents.
  const blink::StorageKey storage_key_;

  const raw_ptr<Profile> profile_;

  // Parent path that all the source URLs descend from.
  const base::FilePath base_path_;

  // Completion callback.
  CompleteCallback complete_callback_;

  // Number of concurrent deletion operations that have been started but that
  // haven't finished yet.
  int in_flight_ = 0;

  base::WeakPtrFactory<EmptyTrashIOTask> weak_ptr_factory_{this};
};

}  // namespace file_manager::io_task

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_EMPTY_TRASH_IO_TASK_H_

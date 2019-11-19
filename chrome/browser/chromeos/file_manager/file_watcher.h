// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_WATCHER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_WATCHER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"

namespace file_manager {

// This class is used to watch changes in the given virtual path, remember
// what extensions are watching the path.
//
// For local files, the class maintains a FilePathWatcher instance and
// remembers what extensions are watching the path.
//
// For remote files (ex. files on Drive), the class just remembers what
// extensions are watching the path. The actual file watching for remote
// files is handled differently in EventRouter.
class FileWatcher {
 public:
  typedef base::OnceCallback<void(bool success)> BoolCallback;

  // Creates a FileWatcher associated with the virtual path.
  explicit FileWatcher(const base::FilePath& virtual_path);

  ~FileWatcher();

  // Remembers that the extension of |extension_id| is watching the virtual
  // path.
  //
  // If this function is called more than once with the same extension ID,
  // the class increments the counter internally, and RemoveExtension()
  // decrements the counter, and forgets the extension when the counter
  // becomes zero.
  void AddExtension(const std::string& extension_id);

  // Forgets that the extension of |extension_id| is watching the virtual path,
  // or just decrements the internal counter for the extension ID. See the
  // comment at AddExtension() for details.
  void RemoveExtension(const std::string& extension_id);

  // Returns IDs of the extensions watching virtual_path. The returned list
  // is sorted in the alphabetical order and contains no duplicates.
  std::vector<std::string> GetExtensionIds() const;

  // Returns the virtual path associated with the FileWatcher.
  const base::FilePath& virtual_path() const { return virtual_path_; }

  // Starts watching a local file at |local_path|. |file_watcher_callback|
  // will be called when changes are notified.
  //
  // |callback| will be called with true, if the file watch is started
  // successfully, or false if failed. |callback| must not be null.
  void WatchLocalFile(
      const base::FilePath& local_path,
      const base::FilePathWatcher::Callback& file_watcher_callback,
      BoolCallback callback);

 private:
  // Called when a FilePathWatcher is created and started.
  // |file_path_watcher| is NULL, if the watcher wasn't started successfully.
  void OnWatcherStarted(BoolCallback callback,
                        base::FilePathWatcher* file_path_watcher);

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  base::FilePathWatcher* local_file_watcher_;
  base::FilePath virtual_path_;
  // Map of extension-id to counter. See the comment at AddExtension() for
  // why we need to count.
  typedef std::map<std::string, int> ExtensionCountMap;
  ExtensionCountMap extensions_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileWatcher> weak_ptr_factory_{this};
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_WATCHER_H_

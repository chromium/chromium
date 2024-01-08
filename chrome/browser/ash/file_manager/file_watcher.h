// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FILE_WATCHER_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FILE_WATCHER_H_

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "url/origin.h"

namespace guest_os {
class GuestOsFileWatcher;
}

namespace file_manager {

// This class is used to watch changes in the given virtual path, remember
// what extensions are watching the path.
//
// For local files, the class maintains a FilePathWatcher instance and
// remembers what extensions are watching the path.
//
// For crostini SSHFS files (/media/fuse/crostini_...), the crostini
// FileWatcher API is used.
//
// For other remote files (ex. files on Drive), the class just remembers what
// extensions are watching the path. The actual file watching for remote
// files is handled differently in EventRouter.
class FileWatcher {
 public:
  typedef base::OnceCallback<void(bool success)> BoolCallback;

  // Creates a FileWatcher associated with the virtual path.
  explicit FileWatcher(const base::FilePath& virtual_path);

  ~FileWatcher();

  // Remembers that a |listener| is watching the virtual path.
  //
  // If this method is called with a origin more than once, it just increments
  // a counter, rather than adding additional listener. In the corresponding
  // method, RemoveListener, the listener corresponding to that origin is only
  // removed once the counter reaches 0.
  void AddListener(const url::Origin& listener);

  // Unregisters |listener| as a watcher of the |virtual_path| for which this
  // FileWatcher was created. The origin is used to decrement an internal
  // counter. If the counter reaches 0, the listener with the given origin is
  // completely removed.
  void RemoveListener(const url::Origin& listener);

  // Returns listeners of the apps watching virtual_path. The returned list
  // is sorted in the alphabetical order and contains no duplicates.
  std::vector<url::Origin> GetListeners() const;

  // Returns the virtual path associated with the FileWatcher.
  const base::FilePath& virtual_path() const { return virtual_path_; }

  // Starts watching a local file at |local_path|. |file_watcher_callback|
  // will be called when changes are notified.
  //
  // |callback| will be called with true, if the file watch is started
  // successfully, or false if failed. |callback| must not be null.
  void WatchLocalFile(
      Profile* profile,
      const base::FilePath& local_path,
      const base::FilePathWatcher::Callback& file_watcher_callback,
      BoolCallback callback);

 private:
  // Called when a FilePathWatcher is created and started.
  // |file_path_watcher| is NULL, if the watcher wasn't started successfully.
  void OnWatcherStarted(BoolCallback callback,
                        base::FilePathWatcher* file_path_watcher);

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  raw_ptr<base::FilePathWatcher, DanglingUntriaged> local_file_watcher_ =
      nullptr;
  std::unique_ptr<guest_os::GuestOsFileWatcher> crostini_file_watcher_;
  base::FilePath virtual_path_;
  // Map of origin to counter. See the comment at AddListener() for
  // why we need to count.
  typedef std::map<url::Origin, int> OriginCountMap;
  OriginCountMap origins_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileWatcher> weak_ptr_factory_{this};
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FILE_WATCHER_H_

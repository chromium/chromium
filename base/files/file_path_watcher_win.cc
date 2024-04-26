// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path_watcher.h"

#include <windows.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/win/object_watcher.h"

namespace base {

namespace {

class FilePathWatcherImpl : public FilePathWatcher::PlatformDelegate,
                            public base::win::ObjectWatcher::Delegate {
 public:
  FilePathWatcherImpl() = default;
  FilePathWatcherImpl(const FilePathWatcherImpl&) = delete;
  FilePathWatcherImpl& operator=(const FilePathWatcherImpl&) = delete;
  ~FilePathWatcherImpl() override;

  // FilePathWatcher::PlatformDelegate implementation:
  bool Watch(const FilePath& path,
             Type type,
             const FilePathWatcher::Callback& callback) override;
  void Cancel() override;

  // base::win::ObjectWatcher::Delegate implementation:
  void OnObjectSignaled(HANDLE object) override;

 private:
  // Setup a watch handle for directory `dir`. Set `recursive` to true to watch
  // the directory sub trees. Returns true if no fatal error occurs. `handle`
  // will receive the handle value if `dir` is watchable, otherwise
  // INVALID_HANDLE_VALUE.
  [[nodiscard]] static bool SetupWatchHandle(const FilePath& dir,
                                             bool recursive,
                                             HANDLE& handle);

  // Sets up a watch handle in `watched_handle_` for either `target_` or one of
  // its ancestors. Returns true on success.
  [[nodiscard]] bool SetupWatchHandleForTarget();

  void CloseWatchHandle();

  // Callback to notify upon changes.
  FilePathWatcher::Callback callback_;

  // Path we're supposed to watch (passed to callback).
  FilePath target_;

  // Handle for FindFirstChangeNotification.
  HANDLE watched_handle_ = INVALID_HANDLE_VALUE;

  // ObjectWatcher to watch handle_ for events.
  base::win::ObjectWatcher watcher_;

  // The type of watch requested.
  Type type_ = Type::kNonRecursive;

  // Keep track of the last modified time of the file.  We use nulltime to
  // represent the file not existing.
  Time last_modified_;

  // The time at which we processed the first notification with the
  // `last_modified_` time stamp.
  Time first_notification_;

  WeakPtrFactory<FilePathWatcherImpl> weak_factory_{this};
};

FilePathWatcherImpl::~FilePathWatcherImpl() {
  DCHECK(!task_runner() || task_runner()->RunsTasksInCurrentSequence());
}

bool FilePathWatcherImpl::Watch(const FilePath& path,
                                Type type,
                                const FilePathWatcher::Callback& callback) {
  DCHECK(target_.empty());  // Can only watch one path.

  set_task_runner(SequencedTaskRunner::GetCurrentDefault());
  callback_ = callback;
  target_ = path;
  type_ = type;

  File::Info file_info;
  if (GetFileInfo(target_, &file_info)) {
    last_modified_ = file_info.last_modified;
    first_notification_ = Time::Now();
  }

  if (!SetupWatchHandleForTarget()) {
    return false;
  }

  watcher_.StartWatchingOnce(watched_handle_, this);

  return true;
}

void FilePathWatcherImpl::Cancel() {
  set_cancelled();

  if (callback_.is_null()) {
    // Watch was never called, or the `task_runner_` has already quit.
    return;
  }

  DCHECK(task_runner()->RunsTasksInCurrentSequence());

  CloseWatchHandle();

  callback_.Reset();
}

void FilePathWatcherImpl::OnObjectSignaled(HANDLE object) {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  DCHECK_EQ(object, watched_handle_);

  auto self = weak_factory_.GetWeakPtr();

  if (!SetupWatchHandleForTarget()) {
    // `this` may be deleted after `callback_` is run.
    callback_.Run(target_, /*error=*/true);
    return;
  }

  // Check whether the event applies to `target_` and notify the callback.
  File::Info file_info;
  bool file_exists = false;
  {
    ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
    file_exists = GetFileInfo(target_, &file_info);
  }
  if (type_ == Type::kRecursive) {
    // Only the mtime of `target_` is tracked but in a recursive watch, some
    // other file or directory may have changed so all notifications are passed
    // through. It is possible to figure out which file changed using
    // ReadDirectoryChangesW() instead of FindFirstChangeNotification(), but
    // that function is quite complicated:
    // http://qualapps.blogspot.com/2010/05/understanding-readdirectorychangesw.html

    // `this` may be deleted after `callback_` is run.
    callback_.Run(target_, /*error=*/false);
  } else if (file_exists && (last_modified_.is_null() ||
                             last_modified_ != file_info.last_modified)) {
    last_modified_ = file_info.last_modified;
    first_notification_ = Time::Now();

    // `this` may be deleted after `callback_` is run.
    callback_.Run(target_, /*error=*/false);
  } else if (file_exists && last_modified_ == file_info.last_modified &&
             !first_notification_.is_null()) {
    // The target's last modification time is equal to what's on record. This
    // means that either an unrelated event occurred, or the target changed
    // again (file modification times only have a resolution of 1s). Comparing
    // file modification times against the wall clock is not reliable to find
    // out whether the change is recent, since this code might just run too
    // late. Moreover, there's no guarantee that file modification time and wall
    // clock times come from the same source.
    //
    // Instead, the time at which the first notification carrying the current
    // `last_notified_` time stamp is recorded. Later notifications that find
    // the same file modification time only need to be forwarded until wall
    // clock has advanced one second from the initial notification. After that
    // interval, client code is guaranteed to having seen the current revision
    // of the file.
    if (Time::Now() - first_notification_ > Seconds(1)) {
      // Stop further notifications for this `last_modification_` time stamp.
      first_notification_ = Time();
    }

    // `this` may be deleted after `callback_` is run.
    callback_.Run(target_, /*error=*/false);
  } else if (!file_exists && !last_modified_.is_null()) {
    last_modified_ = Time();

    // `this` may be deleted after `callback_` is run.
    callback_.Run(target_, /*error=*/false);
  }

  // The watch may have been cancelled by the callback.
  if (self) {
    watcher_.StartWatchingOnce(watched_handle_, this);
  }
}

// static
bool FilePathWatcherImpl::SetupWatchHandle(const FilePath& dir,
                                           bool recursive,
                                           HANDLE& handle) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  handle = FindFirstChangeNotification(
      dir.value().c_str(), recursive,
      FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE |
          FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_DIR_NAME |
          FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SECURITY);
  if (handle != INVALID_HANDLE_VALUE) {
    // Make sure the handle we got points to an existing directory. It seems
    // that windows sometimes hands out watches to directories that are about to
    // go away, but doesn't send notifications if that happens.
    if (!DirectoryExists(dir)) {
      FindCloseChangeNotification(handle);
      handle = INVALID_HANDLE_VALUE;
    }
    return true;
  }

  // If FindFirstChangeNotification failed because the target directory doesn't
  // exist, access is denied (happens if the file is already gone but there are
  // still handles open), or the target is not a directory, try the immediate
  // parent directory instead.
  DWORD error_code = GetLastError();
  if (error_code != ERROR_FILE_NOT_FOUND &&
      error_code != ERROR_PATH_NOT_FOUND && error_code != ERROR_ACCESS_DENIED &&
      error_code != ERROR_SHARING_VIOLATION && error_code != ERROR_DIRECTORY) {
    DPLOG(ERROR) << "FindFirstChangeNotification failed for " << dir.value();
    return false;
  }

  return true;
}

bool FilePathWatcherImpl::SetupWatchHandleForTarget() {
  CloseWatchHandle();

  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  // Start at the target and walk up the directory chain until we successfully
  // create a watch handle in `watched_handle_`. `child_dirs` keeps a stack of
  // child directories stripped from target, in reverse order.
  std::vector<FilePath> child_dirs;
  FilePath path_to_watch(target_);
  while (true) {
    if (!SetupWatchHandle(path_to_watch, type_ == Type::kRecursive,
                          watched_handle_)) {
      return false;
    }

    // Break if a valid handle is returned. Try the parent directory otherwise.
    if (watched_handle_ != INVALID_HANDLE_VALUE) {
      break;
    }

    // Abort if we hit the root directory.
    child_dirs.push_back(path_to_watch.BaseName());
    FilePath parent(path_to_watch.DirName());
    if (parent == path_to_watch) {
      DLOG(ERROR) << "Reached the root directory";
      return false;
    }
    path_to_watch = parent;
  }

  // At this point, `watched_handle_` is valid. However, the bottom-up search
  // that the above code performs races against directory creation. So try to
  // walk back down and see whether any children appeared in the mean time.
  while (!child_dirs.empty()) {
    path_to_watch = path_to_watch.Append(child_dirs.back());
    child_dirs.pop_back();
    HANDLE temp_handle = INVALID_HANDLE_VALUE;
    if (!SetupWatchHandle(path_to_watch, type_ == Type::kRecursive,
                          temp_handle)) {
      return false;
    }
    if (temp_handle == INVALID_HANDLE_VALUE) {
      break;
    }
    FindCloseChangeNotification(watched_handle_);
    watched_handle_ = temp_handle;
  }

  return true;
}

void FilePathWatcherImpl::CloseWatchHandle() {
  if (watched_handle_ != INVALID_HANDLE_VALUE) {
    watcher_.StopWatching();

    ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
    FindCloseChangeNotification(watched_handle_);
    watched_handle_ = INVALID_HANDLE_VALUE;
  }
}

}  // namespace

FilePathWatcher::FilePathWatcher()
    : FilePathWatcher(std::make_unique<FilePathWatcherImpl>()) {}

}  // namespace base

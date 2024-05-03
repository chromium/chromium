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
#include "base/types/expected.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"

namespace base {
namespace {
class NotificationHandleTraits {
 public:
  using Handle = HANDLE;

  NotificationHandleTraits() = delete;
  NotificationHandleTraits(const NotificationHandleTraits&) = delete;
  NotificationHandleTraits& operator=(const NotificationHandleTraits&) = delete;

  static bool CloseHandle(HANDLE handle) {
    return FindCloseChangeNotification(handle) != 0;
  }
  static bool IsHandleValid(HANDLE handle) {
    return handle != INVALID_HANDLE_VALUE;
  }
  static HANDLE NullHandle() { return INVALID_HANDLE_VALUE; }
};

using ScopedNotificationHandle =
    win::GenericScopedHandle<NotificationHandleTraits,
                             win::DummyVerifierTraits>;

enum class CreateFileHandleError {
  // When watching a path, the path (or some of its ancestor directories) might
  // not exist yet. Failure to create a watcher because the path doesn't exist
  // (or is not a directory) should not be considered fatal, since the watcher
  // implementation can simply try again one directory level above.
  kNonFatal,
  kFatal,
};

base::expected<ScopedNotificationHandle, CreateFileHandleError>
CreateNotificationHandle(const FilePath& dir, FilePathWatcher::Type type) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  ScopedNotificationHandle handle(FindFirstChangeNotification(
      dir.value().c_str(), type == FilePathWatcher::Type::kRecursive,
      FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE |
          FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_DIR_NAME |
          FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SECURITY));

  if (handle.is_valid()) {
    // Make sure the handle we got points to an existing directory. It seems
    // that windows sometimes hands out watches to directories that are about to
    // go away, but doesn't send notifications if that happens.
    if (!DirectoryExists(dir)) {
      return base::unexpected(CreateFileHandleError::kNonFatal);
    }

    return handle;
  }

  switch (GetLastError()) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_DIRECTORY:
      // Failure to create the handle is ok if the target directory doesn't
      // exist, access is denied (happens if the file is already gone but there
      // are still handles open), or the target is not a directory.
      return base::unexpected(CreateFileHandleError::kNonFatal);
    default:
      DPLOG(ERROR) << "CreateFileW failed for " << dir.value();
      return base::unexpected(CreateFileHandleError::kFatal);
  }
}

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
  // Sets up a watch handle in `watched_handle_` for either `target_` or one of
  // its ancestors. Returns true on success.
  [[nodiscard]] bool SetupWatchHandleForTarget();

  void CloseWatchHandle();

  // Callback to notify upon changes.
  FilePathWatcher::Callback callback_;

  // Path we're supposed to watch (passed to callback).
  FilePath target_;

  // Handle for FindFirstChangeNotification.
  ScopedNotificationHandle watched_handle_;

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

  watcher_.StartWatchingOnce(watched_handle_.get(), this);

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
  DCHECK_EQ(object, watched_handle_.get());

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
    watcher_.StartWatchingOnce(watched_handle_.get(), this);
  }
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
    auto result = CreateNotificationHandle(path_to_watch, type_);

    // Break if a valid handle is returned.
    if (result.has_value()) {
      watched_handle_ = std::move(result.value());
      break;
    }

    // We're in an unknown state if `CreateNotificationHandle` returns an
    // `kFatal` error, so return failure.
    if (result.error() == CreateFileHandleError::kFatal) {
      return false;
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
    auto result = CreateNotificationHandle(path_to_watch, type_);
    if (!result.has_value()) {
      // We're in an unknown state if `CreateNotificationHandle` returns an
      // `kFatal` error, so return failure.
      if (result.error() == CreateFileHandleError::kFatal) {
        return false;
      }
      // Otherwise go with the current `watched_handle`.
      break;
    }
    watched_handle_ = std::move(result.value());
  }

  return true;
}

void FilePathWatcherImpl::CloseWatchHandle() {
  if (watched_handle_.is_valid()) {
    watcher_.StopWatching();

    ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
    watched_handle_.Close();
  }
}

}  // namespace

FilePathWatcher::FilePathWatcher()
    : FilePathWatcher(std::make_unique<FilePathWatcherImpl>()) {}

}  // namespace base

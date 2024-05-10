// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path_watcher.h"

#include <windows.h>

#include <winnt.h>

#include <map>
#include <memory>
#include <tuple>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/id_type.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"

namespace base {
namespace {

enum class CreateFileHandleError {
  // When watching a path, the path (or some of its ancestor directories) might
  // not exist yet. Failure to create a watcher because the path doesn't exist
  // (or is not a directory) should not be considered fatal, since the watcher
  // implementation can simply try again one directory level above.
  kNonFatal,
  kFatal,
};

base::expected<base::win::ScopedHandle, CreateFileHandleError>
CreateDirectoryHandle(const FilePath& dir) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  base::win::ScopedHandle handle(::CreateFileW(
      dir.value().c_str(), FILE_LIST_DIRECTORY,
      FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
      nullptr));

  if (handle.is_valid()) {
    File::Info file_info;
    if (!GetFileInfo(dir, &file_info)) {
      // Windows sometimes hands out handles to files that are about to go away.
      return base::unexpected(CreateFileHandleError::kNonFatal);
    }

    // Only return the handle if its a directory.
    if (!file_info.is_directory) {
      return base::unexpected(CreateFileHandleError::kNonFatal);
    }

    return handle;
  }

  switch (::GetLastError()) {
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

class FilePathWatcherImpl;

class CompletionIOPortThread final : public PlatformThread::Delegate {
 public:
  using WatcherEntryId = base::IdTypeU64<class WatcherEntryIdTag>;

  CompletionIOPortThread(const CompletionIOPortThread&) = delete;
  CompletionIOPortThread& operator=(const CompletionIOPortThread&) = delete;

  static CompletionIOPortThread* Get() {
    static NoDestructor<CompletionIOPortThread> io_thread;
    return io_thread.get();
  }

  // Thread safe.
  std::optional<WatcherEntryId> AddWatcher(
      FilePathWatcherImpl& watcher,
      base::win::ScopedHandle watched_handle);

  // Thread safe.
  void RemoveWatcher(WatcherEntryId watcher_id);

 private:
  friend NoDestructor<CompletionIOPortThread>;

  // Choose something small since we won't actually be processing the buffer.
  static constexpr size_t kWatchBufferSizeBytes = sizeof(DWORD);

  // Must be DWORD aligned.
  static_assert(kWatchBufferSizeBytes % sizeof(DWORD) == 0);
  // Must be less than the max network packet size for network drives. See
  // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw#remarks.
  static_assert(kWatchBufferSizeBytes <= 64 * 1024);

  struct WatcherEntry {
    WatcherEntry(base::WeakPtr<FilePathWatcherImpl> watcher_weak_ptr,
                 scoped_refptr<SequencedTaskRunner> task_runner,
                 base::win::ScopedHandle watched_handle)
        : watcher_weak_ptr(std::move(watcher_weak_ptr)),
          task_runner(std::move(task_runner)),
          watched_handle(std::move(watched_handle)) {}
    ~WatcherEntry() = default;

    // Delete copy and move constructors since `buffer` should not be copied or
    // moved.
    WatcherEntry(const WatcherEntry&) = delete;
    WatcherEntry& operator=(const WatcherEntry&) = delete;
    WatcherEntry(WatcherEntry&&) = delete;
    WatcherEntry& operator=(WatcherEntry&&) = delete;

    base::WeakPtr<FilePathWatcherImpl> watcher_weak_ptr;
    scoped_refptr<SequencedTaskRunner> task_runner;

    base::win::ScopedHandle watched_handle;

    alignas(DWORD) uint8_t buffer[kWatchBufferSizeBytes];
  };

  OVERLAPPED overlapped = {};

  CompletionIOPortThread();

  ~CompletionIOPortThread() override = default;

  void ThreadMain() override;

  [[nodiscard]] DWORD SetupWatch(WatcherEntry& watcher_entry);

  Lock watchers_lock_;

  WatcherEntryId::Generator watcher_id_generator_ GUARDED_BY(watchers_lock_);

  std::map<WatcherEntryId, WatcherEntry> watcher_entries_
      GUARDED_BY(watchers_lock_);

  // It is safe to access `io_completion_port_` on any thread without locks
  // since:
  //   - Windows Handles are thread safe
  //   - `io_completion_port_` is set once in the constructor of this class
  //   - This class is never destroyed.
  win::ScopedHandle io_completion_port_{
      ::CreateIoCompletionPort(INVALID_HANDLE_VALUE,
                               nullptr,
                               reinterpret_cast<ULONG_PTR>(nullptr),
                               1)};
};

class FilePathWatcherImpl : public FilePathWatcher::PlatformDelegate {
 public:
  FilePathWatcherImpl() = default;
  FilePathWatcherImpl(const FilePathWatcherImpl&) = delete;
  FilePathWatcherImpl& operator=(const FilePathWatcherImpl&) = delete;
  ~FilePathWatcherImpl() override;

  // FilePathWatcher::PlatformDelegate implementation:
  bool Watch(const FilePath& path,
             Type type,
             const FilePathWatcher::Callback& callback) override;

  // FilePathWatcher::PlatformDelegate implementation:
  bool WatchWithOptions(const FilePath& path,
                        const WatchOptions& flags,
                        const FilePathWatcher::Callback& callback) override;

  // FilePathWatcher::PlatformDelegate implementation:
  bool WatchWithChangeInfo(
      const FilePath& path,
      const WatchOptions& options,
      const FilePathWatcher::CallbackWithChangeInfo& callback) override;

  void Cancel() override;

 private:
  friend CompletionIOPortThread;

  // Sets up a watch handle for either `target_` or one of its ancestors.
  // Returns true on success.
  [[nodiscard]] bool SetupWatchHandleForTarget();

  void CloseWatchHandle();

  void OnObjectSignaled();

  // Callback to notify upon changes.
  FilePathWatcher::CallbackWithChangeInfo callback_;

  // Path we're supposed to watch (passed to callback).
  FilePath target_;

  std::optional<CompletionIOPortThread::WatcherEntryId> watcher_id_;

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

CompletionIOPortThread::CompletionIOPortThread() {
  PlatformThread::CreateNonJoinable(0, this);
}

DWORD CompletionIOPortThread::SetupWatch(WatcherEntry& watcher_entry) {
  bool success = ::ReadDirectoryChangesW(
      watcher_entry.watched_handle.get(), &watcher_entry.buffer,
      static_cast<DWORD>(kWatchBufferSizeBytes), /*bWatchSubtree =*/true,
      FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE |
          FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_DIR_NAME |
          FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SECURITY,
      nullptr, &overlapped, nullptr);
  if (!success) {
    return ::GetLastError();
  }
  return ERROR_SUCCESS;
}

std::optional<CompletionIOPortThread::WatcherEntryId>
CompletionIOPortThread::AddWatcher(FilePathWatcherImpl& watcher,
                                   base::win::ScopedHandle watched_handle) {
  AutoLock auto_lock(watchers_lock_);

  WatcherEntryId watcher_id = watcher_id_generator_.GenerateNextId();
  HANDLE port = ::CreateIoCompletionPort(
      watched_handle.get(), io_completion_port_.get(),
      static_cast<ULONG_PTR>(watcher_id.GetUnsafeValue()), 1);
  if (port == nullptr) {
    return std::nullopt;
  }

  auto [it, inserted] = watcher_entries_.emplace(
      std::piecewise_construct, std::forward_as_tuple(watcher_id),
      std::forward_as_tuple(watcher.weak_factory_.GetWeakPtr(),
                            watcher.task_runner(), std::move(watched_handle)));

  CHECK(inserted);

  DWORD result = SetupWatch(it->second);

  if (result != ERROR_SUCCESS) {
    watcher_entries_.erase(it);
    return std::nullopt;
  }

  return watcher_id;
}

void CompletionIOPortThread::RemoveWatcher(WatcherEntryId watcher_id) {
  HANDLE raw_watched_handle;
  {
    AutoLock auto_lock(watchers_lock_);

    auto it = watcher_entries_.find(watcher_id);
    CHECK(it != watcher_entries_.end());

    auto& watched_handle = it->second.watched_handle;
    CHECK(watched_handle.is_valid());
    raw_watched_handle = watched_handle.release();
  }

  {
    ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

    // `raw_watched_handle` being closed indicates to `ThreadMain` that this
    // entry needs to be removed from `watcher_entries_` once the kernel
    // indicates it is safe too.
    ::CloseHandle(raw_watched_handle);
  }
}

void CompletionIOPortThread::ThreadMain() {
  while (true) {
    DWORD bytes_transferred;
    ULONG_PTR key = reinterpret_cast<ULONG_PTR>(nullptr);
    OVERLAPPED* overlapped_out = nullptr;

    BOOL io_port_result = ::GetQueuedCompletionStatus(
        io_completion_port_.get(), &bytes_transferred, &key, &overlapped_out,
        INFINITE);
    CHECK(&overlapped == overlapped_out);

    if (io_port_result == FALSE) {
      DWORD io_port_error = ::GetLastError();
      // `ERROR_ACCESS_DENIED` should be the only error we can receive.
      DCHECK_EQ(io_port_error, static_cast<DWORD>(ERROR_ACCESS_DENIED));
    }

    AutoLock auto_lock(watchers_lock_);

    WatcherEntryId watcher_id = WatcherEntryId::FromUnsafeValue(key);

    auto watcher_entry_it = watcher_entries_.find(watcher_id);

    if (watcher_entry_it == watcher_entries_.end()) {
      NOTREACHED() << "!watcher_entries_.contains(watcher_id)";
      continue;
    }

    auto& watcher_entry = watcher_entry_it->second;
    auto& [watcher_weak_ptr, task_runner, watched_handle, buffer] =
        watcher_entry;

    if (!watched_handle.is_valid()) {
      // After the handle has been closed, a final notification will be sent
      // with `bytes_transferred` equal to 0. It is safe to destroy the watcher
      // now.
      if (bytes_transferred == 0) {
        // `watcher_entry` and all the local refs to its members will be
        // dangling after this call.
        watcher_entries_.erase(watcher_entry_it);
      }
      continue;
    }

    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(&FilePathWatcherImpl::OnObjectSignaled,
                                         watcher_weak_ptr));
  }
}

FilePathWatcherImpl::~FilePathWatcherImpl() {
  DCHECK(!task_runner() || task_runner()->RunsTasksInCurrentSequence());
}

bool FilePathWatcherImpl::Watch(const FilePath& path,
                                Type type,
                                const FilePathWatcher::Callback& callback) {
  return WatchWithChangeInfo(
      path, WatchOptions{.type = type},
      base::IgnoreArgs<const FilePathWatcher::ChangeInfo&>(
          base::BindRepeating(std::move(callback))));
}

bool FilePathWatcherImpl::WatchWithOptions(
    const FilePath& path,
    const WatchOptions& options,
    const FilePathWatcher::Callback& callback) {
  return WatchWithChangeInfo(
      path, options,
      base::IgnoreArgs<const FilePathWatcher::ChangeInfo&>(
          base::BindRepeating(std::move(callback))));
}

bool FilePathWatcherImpl::WatchWithChangeInfo(
    const FilePath& path,
    const WatchOptions& options,
    const FilePathWatcher::CallbackWithChangeInfo& callback) {
  DCHECK(target_.empty());  // Can only watch one path.

  set_task_runner(SequencedTaskRunner::GetCurrentDefault());
  callback_ = callback;
  target_ = path;
  type_ = options.type;

  File::Info file_info;
  if (GetFileInfo(target_, &file_info)) {
    last_modified_ = file_info.last_modified;
    first_notification_ = Time::Now();
  }

  return SetupWatchHandleForTarget();
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

void FilePathWatcherImpl::OnObjectSignaled() {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());

  auto self = weak_factory_.GetWeakPtr();

  if (!SetupWatchHandleForTarget()) {
    // `this` may be deleted after `callback_` is run.
    callback_.Run(FilePathWatcher::ChangeInfo(), target_, /*error=*/true);
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
    callback_.Run(FilePathWatcher::ChangeInfo(), target_, /*error=*/false);
  } else if (file_exists && (last_modified_.is_null() ||
                             last_modified_ != file_info.last_modified)) {
    last_modified_ = file_info.last_modified;
    first_notification_ = Time::Now();

    // `this` may be deleted after `callback_` is run.
    callback_.Run(FilePathWatcher::ChangeInfo(), target_, /*error=*/false);
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
    callback_.Run(FilePathWatcher::ChangeInfo(), target_, /*error=*/false);
  } else if (!file_exists && !last_modified_.is_null()) {
    last_modified_ = Time();

    // `this` may be deleted after `callback_` is run.
    callback_.Run(FilePathWatcher::ChangeInfo(), target_, /*error=*/false);
  }
}

bool FilePathWatcherImpl::SetupWatchHandleForTarget() {
  CloseWatchHandle();

  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  // Start at the target and walk up the directory chain until we successfully
  // create a file handle in `watched_handle_`. `child_dirs` keeps a stack of
  // child directories stripped from target, in reverse order.
  std::vector<FilePath> child_dirs;
  FilePath path_to_watch(target_);
  base::win::ScopedHandle watched_handle;
  while (true) {
    auto result = CreateDirectoryHandle(path_to_watch);

    // Break if a valid handle is returned.
    if (result.has_value()) {
      watched_handle = std::move(result.value());
      break;
    }

    // We're in an unknown state if `CreateDirectoryHandle` returns an `kFatal`
    // error, so return failure.
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

  // At this point, `watched_handle` is valid. However, the bottom-up search
  // that the above code performs races against directory creation. So try to
  // walk back down and see whether any children appeared in the mean time.
  while (!child_dirs.empty()) {
    path_to_watch = path_to_watch.Append(child_dirs.back());
    child_dirs.pop_back();
    auto result = CreateDirectoryHandle(path_to_watch);
    if (!result.has_value()) {
      // We're in an unknown state if `CreateDirectoryHandle` returns an
      // `kFatal` error, so return failure.
      if (result.error() == CreateFileHandleError::kFatal) {
        return false;
      }
      // Otherwise go with the current `watched_handle`.
      break;
    }
    watched_handle = std::move(result.value());
  }

  watcher_id_ = CompletionIOPortThread::Get()->AddWatcher(
      *this, std::move(watched_handle));

  return watcher_id_.has_value();
}

void FilePathWatcherImpl::CloseWatchHandle() {
  if (watcher_id_.has_value()) {
    CompletionIOPortThread::Get()->RemoveWatcher(watcher_id_.value());
    watcher_id_.reset();
  }
}

}  // namespace

FilePathWatcher::FilePathWatcher()
    : FilePathWatcher(std::make_unique<FilePathWatcherImpl>()) {}

}  // namespace base

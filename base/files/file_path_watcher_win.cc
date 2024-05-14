// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path_watcher.h"

#include <windows.h>

#include <winnt.h>

#include <cstdint>
#include <map>
#include <memory>
#include <tuple>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
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
      base::win::ScopedHandle watched_handle,
      base::FilePath watched_path);

  // Thread safe.
  void RemoveWatcher(WatcherEntryId watcher_id);

  Lock& GetLockForTest();  // IN-TEST

 private:
  friend NoDestructor<CompletionIOPortThread>;

  // The max size of a file notification assuming that long paths aren't
  // enabled.
  static constexpr size_t kMaxFileNotifySize =
      sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH;

  // Choose a decent number of notifications to support that isn't too large.
  // Whatever we choose will be doubled by the kernel's copy of the buffer.
  static constexpr int kBufferNotificationCount = 20;
  static constexpr size_t kWatchBufferSizeBytes =
      kBufferNotificationCount * kMaxFileNotifySize;

  // Must be DWORD aligned.
  static_assert(kWatchBufferSizeBytes % sizeof(DWORD) == 0);
  // Must be less than the max network packet size for network drives. See
  // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw#remarks.
  static_assert(kWatchBufferSizeBytes <= 64 * 1024);

  struct WatcherEntry {
    WatcherEntry(base::WeakPtr<FilePathWatcherImpl> watcher_weak_ptr,
                 scoped_refptr<SequencedTaskRunner> task_runner,
                 base::win::ScopedHandle watched_handle,
                 base::FilePath watched_path)
        : watcher_weak_ptr(std::move(watcher_weak_ptr)),
          task_runner(std::move(task_runner)),
          watched_handle(std::move(watched_handle)),
          watched_path(std::move(watched_path)) {}
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
    base::FilePath watched_path;

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

  Lock& GetWatchThreadLockForTest() override;  // IN-TEST

 private:
  friend CompletionIOPortThread;

  // Sets up a watch handle for either `target_` or one of its ancestors.
  // Returns true on success.
  [[nodiscard]] bool SetupWatchHandleForTarget();

  void CloseWatchHandle();

  void BufferOverflowed();

  void WatchedDirectoryDeleted(base::FilePath watched_path,
                               base::HeapArray<uint8_t> notification_batch);

  void ProcessNotificationBatch(base::FilePath watched_path,
                                base::HeapArray<uint8_t> notification_batch);

  // Callback to notify upon changes.
  FilePathWatcher::CallbackWithChangeInfo callback_;

  // Path we're supposed to watch (passed to callback).
  FilePath target_;

  std::optional<CompletionIOPortThread::WatcherEntryId> watcher_id_;

  // The type of watch requested.
  Type type_ = Type::kNonRecursive;

  bool target_exists_ = false;

  WeakPtrFactory<FilePathWatcherImpl> weak_factory_{this};
};

CompletionIOPortThread::CompletionIOPortThread() {
  PlatformThread::CreateNonJoinable(0, this);
}

DWORD CompletionIOPortThread::SetupWatch(WatcherEntry& watcher_entry) {
  bool success = ReadDirectoryChangesW(
      watcher_entry.watched_handle.get(), &watcher_entry.buffer,
      kWatchBufferSizeBytes, /*bWatchSubtree=*/true,
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
                                   base::win::ScopedHandle watched_handle,
                                   base::FilePath watched_path) {
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
                            watcher.task_runner(), std::move(watched_handle),
                            std::move(watched_path)));

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

Lock& CompletionIOPortThread::GetLockForTest() {
  return watchers_lock_;
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

    DWORD io_port_error = ERROR_SUCCESS;
    if (io_port_result == FALSE) {
      io_port_error = ::GetLastError();
      // `ERROR_ACCESS_DENIED` should be the only error we can receive.
      CHECK_EQ(io_port_error, static_cast<DWORD>(ERROR_ACCESS_DENIED));
    }

    AutoLock auto_lock(watchers_lock_);

    WatcherEntryId watcher_id = WatcherEntryId::FromUnsafeValue(key);

    auto watcher_entry_it = watcher_entries_.find(watcher_id);

    CHECK(watcher_entry_it != watcher_entries_.end())
        << "WatcherEntryId not in map";

    auto& watcher_entry = watcher_entry_it->second;
    auto& [watcher_weak_ptr, task_runner, watched_handle, watched_path,
           buffer] = watcher_entry;

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

    // `GetQueuedCompletionStatus` can fail with `ERROR_ACCESS_DENIED` when the
    // watched directory is deleted.
    if (io_port_result == FALSE) {
      CHECK(bytes_transferred == 0);

      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&FilePathWatcherImpl::WatchedDirectoryDeleted,
                         watcher_weak_ptr, watched_path,
                         base::HeapArray<uint8_t>()));
      continue;
    }

    base::HeapArray<uint8_t> notification_batch;
    if (bytes_transferred > 0) {
      notification_batch = base::HeapArray<uint8_t>::CopiedFrom(
          base::span<uint8_t>(buffer).first(bytes_transferred));
    }

    // Let the kernel know that we're ready to receive change events again in
    // the `watcher_entry`'s `buffer`.
    //
    // We do this as soon as possible, so that not too many events are received
    // in the next batch. Too many events can cause a buffer overflow.
    DWORD result = SetupWatch(watcher_entry);

    // `SetupWatch` can fail if the watched directory was deleted before
    // `SetupWatch` was called but after `GetQueuedCompletionStatus` returned.
    if (result != ERROR_SUCCESS) {
      CHECK_EQ(result, static_cast<DWORD>(ERROR_ACCESS_DENIED));
      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&FilePathWatcherImpl::WatchedDirectoryDeleted,
                         watcher_weak_ptr, watched_path,
                         std::move(notification_batch)));
      continue;
    }

    // `GetQueuedCompletionStatus` succeeds with zero bytes transferred if there
    // is a buffer overflow.
    if (bytes_transferred == 0) {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(&FilePathWatcherImpl::BufferOverflowed,
                                    watcher_weak_ptr));
      continue;
    }

    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&FilePathWatcherImpl::ProcessNotificationBatch,
                       watcher_weak_ptr, watched_path,
                       std::move(notification_batch)));
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
  target_exists_ = GetFileInfo(target_, &file_info);

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

Lock& FilePathWatcherImpl::GetWatchThreadLockForTest() {
  return CompletionIOPortThread::Get()->GetLockForTest();  // IN-TEST
}

void FilePathWatcherImpl::BufferOverflowed() {
  // `this` may be deleted after `callback_` is run.
  callback_.Run(FilePathWatcher::ChangeInfo(), target_, /*error=*/false);
}

void FilePathWatcherImpl::WatchedDirectoryDeleted(
    base::FilePath watched_path,
    base::HeapArray<uint8_t> notification_batch) {
  if (!SetupWatchHandleForTarget()) {
    // `this` may be deleted after `callback_` is run.
    callback_.Run(FilePathWatcher::ChangeInfo(), target_, /*error=*/true);
    return;
  }

  if (!notification_batch.empty()) {
    auto self = weak_factory_.GetWeakPtr();
    // `ProcessNotificationBatch` may delete `this`.
    ProcessNotificationBatch(std::move(watched_path),
                             std::move(notification_batch));
    if (!self) {
      return;
    }
  }

  bool target_was_deleted = target_exists_ || watched_path == target_;
  if (target_was_deleted) {
    // `this` may be deleted after `callback_` is run.
    callback_.Run(FilePathWatcher::ChangeInfo(), target_, /*error=*/false);
  }
}

void FilePathWatcherImpl::ProcessNotificationBatch(
    base::FilePath watched_path,
    base::HeapArray<uint8_t> notification_batch) {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  CHECK(!notification_batch.empty());

  auto self = weak_factory_.GetWeakPtr();

  // Check whether the event applies to `target_` and notify the callback.
  File::Info target_info;
  bool target_exists_after_batch = GetFileInfo(target_, &target_info);

  bool target_created_or_deleted = target_exists_after_batch != target_exists_;
  target_exists_ = target_exists_after_batch;

  // This keeps track of whether we just notified for a
  // `FILE_ACTION_RENAMED_OLD_NAME`.
  bool last_event_notified_for_old_name = false;

  auto sub_span = notification_batch.as_span();
  bool has_next_entry = true;

  while (has_next_entry) {
    const auto& file_notify_info =
        *reinterpret_cast<FILE_NOTIFY_INFORMATION*>(sub_span.data());

    has_next_entry = file_notify_info.NextEntryOffset != 0;
    if (has_next_entry) {
      sub_span = sub_span.subspan(file_notify_info.NextEntryOffset);
    }

    DWORD change_type = file_notify_info.Action;

    // A rename will generate two move events, but we only report it as one move
    // event. So continue if we just reported a `FILE_ACTION_RENAMED_OLD_NAME`.
    if (last_event_notified_for_old_name &&
        change_type == FILE_ACTION_RENAMED_NEW_NAME) {
      last_event_notified_for_old_name = false;
      continue;
    }
    last_event_notified_for_old_name = false;

    FilePath change_path = watched_path.Append(std::basic_string_view<wchar_t>(
        file_notify_info.FileName,
        file_notify_info.FileNameLength / sizeof(wchar_t)));

    // Ancestors of the `target_` are outside the watch scope.
    if (change_path.IsParent(target_)) {
      // Only report move events where the target was created or deleted.
      if ((change_type != FILE_ACTION_RENAMED_NEW_NAME &&
           change_type != FILE_ACTION_RENAMED_OLD_NAME) ||
          !target_created_or_deleted) {
        continue;
      }
    } else if (type_ == FilePathWatcher::Type::kNonRecursive &&
               change_path != target_ && change_path.DirName() != target_) {
      // For non recursive watches, only report events for the target or its
      // direct children.
      continue;
    }

    if (change_type == FILE_ACTION_MODIFIED) {
      // Don't report modified events for directories.
      File::Info file_info;
      if (GetFileInfo(change_path, &file_info) && file_info.is_directory) {
        continue;
      }
    }

    last_event_notified_for_old_name =
        change_type == FILE_ACTION_RENAMED_OLD_NAME;

    // `this` may be deleted after `callback_` is run.
    callback_.Run(FilePathWatcher::ChangeInfo(), target_, /*error=*/false);
    if (!self) {
      return;
    }
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
  FilePath watched_path;
  while (true) {
    auto result = CreateDirectoryHandle(path_to_watch);

    // Break if a valid handle is returned.
    if (result.has_value()) {
      watched_handle = std::move(result.value());
      watched_path = path_to_watch;
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
    path_to_watch = std::move(parent);
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
    watched_path = path_to_watch;
  }

  watcher_id_ = CompletionIOPortThread::Get()->AddWatcher(
      *this, std::move(watched_handle), std::move(watched_path));

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

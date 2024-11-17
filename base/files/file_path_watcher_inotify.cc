// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path_watcher_inotify.h"

#include <errno.h>
#include <poll.h>
#include <stddef.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

namespace base {

namespace {

#if !BUILDFLAG(IS_FUCHSIA)

// The /proc path to max_user_watches.
constexpr char kInotifyMaxUserWatchesPath[] =
    "/proc/sys/fs/inotify/max_user_watches";

// This is a soft limit. If there are more than |kExpectedFilePathWatches|
// FilePathWatchers for a user, than they might affect each other's inotify
// watchers limit.
constexpr size_t kExpectedFilePathWatchers = 16u;

// The default max inotify watchers limit per user, if reading
// /proc/sys/fs/inotify/max_user_watches fails.
constexpr size_t kDefaultInotifyMaxUserWatches = 8192u;

#endif  // !BUILDFLAG(IS_FUCHSIA)

class FilePathWatcherImpl;
class InotifyReader;

// Used by test to override inotify watcher limit.
size_t g_override_max_inotify_watches = 0u;

FilePathWatcher::ChangeType ToChangeType(const inotify_event* const event) {
  // Greedily select the most specific change type. It's possible that multiple
  // types may apply, so this is ordered by specificity (e.g. "created" may also
  // imply "modified", but the former is more useful).
  if (event->mask & (IN_MOVED_FROM | IN_MOVED_TO)) {
    return FilePathWatcher::ChangeType::kMoved;
  } else if (event->mask & IN_CREATE) {
    return FilePathWatcher::ChangeType::kCreated;
  } else if (event->mask & IN_DELETE) {
    return FilePathWatcher::ChangeType::kDeleted;
  } else {
    return FilePathWatcher::ChangeType::kModified;
  }
}

class InotifyReaderThreadDelegate final : public PlatformThread::Delegate {
 public:
  explicit InotifyReaderThreadDelegate(int inotify_fd)
      : inotify_fd_(inotify_fd) {}
  InotifyReaderThreadDelegate(const InotifyReaderThreadDelegate&) = delete;
  InotifyReaderThreadDelegate& operator=(const InotifyReaderThreadDelegate&) =
      delete;
  ~InotifyReaderThreadDelegate() override = default;

 private:
  void ThreadMain() override;

  const int inotify_fd_;
};

// Singleton to manage all inotify watches.
// TODO(tony): It would be nice if this wasn't a singleton.
// http://crbug.com/38174
class InotifyReader {
 public:
  // Watch descriptor used by AddWatch() and RemoveWatch().
#if BUILDFLAG(IS_ANDROID)
  using Watch = uint32_t;
#else
  using Watch = int;
#endif

  // Record of watchers tracked for watch descriptors.
  struct WatcherEntry {
    scoped_refptr<SequencedTaskRunner> task_runner;
    WeakPtr<FilePathWatcherImpl> watcher;
  };

  static constexpr Watch kInvalidWatch = static_cast<Watch>(-1);
  static constexpr Watch kWatchLimitExceeded = static_cast<Watch>(-2);

  InotifyReader(const InotifyReader&) = delete;
  InotifyReader& operator=(const InotifyReader&) = delete;

  // Watch directory |path| for changes. |watcher| will be notified on each
  // change. Returns |kInvalidWatch| on failure.
  Watch AddWatch(const FilePath& path, FilePathWatcherImpl* watcher);

  // Remove |watch| if it's valid.
  void RemoveWatch(Watch watch, FilePathWatcherImpl* watcher);

  // Invoked on "inotify_reader" thread to notify relevant watchers.
  void OnInotifyEvent(const inotify_event* event);

  // Returns true if any paths are actively being watched.
  bool HasWatches();

 private:
  friend struct LazyInstanceTraitsBase<InotifyReader>;

  InotifyReader();
  // There is no destructor because |g_inotify_reader| is a
  // base::LazyInstace::Leaky object. Having a destructor causes build
  // issues with GCC 6 (http://crbug.com/636346).

  // Returns true on successful thread creation.
  bool StartThread();

  Lock lock_;

  // Tracks which FilePathWatcherImpls to be notified on which watches.
  // The tracked FilePathWatcherImpl is keyed by raw pointers for fast look up
  // and mapped to a WatchEntry that is used to safely post a notification.
  std::unordered_map<Watch, std::map<FilePathWatcherImpl*, WatcherEntry>>
      watchers_ GUARDED_BY(lock_);

  // File descriptor returned by inotify_init.
  const int inotify_fd_;

  // Thread delegate for the Inotify thread.
  InotifyReaderThreadDelegate thread_delegate_;

  // Flag set to true when startup was successful.
  bool valid_ = false;
};

class FilePathWatcherImpl : public FilePathWatcher::PlatformDelegate {
 public:
  FilePathWatcherImpl();
  FilePathWatcherImpl(const FilePathWatcherImpl&) = delete;
  FilePathWatcherImpl& operator=(const FilePathWatcherImpl&) = delete;
  ~FilePathWatcherImpl() override;

  // Called for each event coming from the watch on the original thread.
  // |fired_watch| identifies the watch that fired, |child| indicates what has
  // changed, and is relative to the currently watched path for |fired_watch|.
  //
  // |change_info| includes information about the change.
  // |created| is true if the object appears.
  // |deleted| is true if the object disappears.
  void OnFilePathChanged(InotifyReader::Watch fired_watch,
                         const FilePath::StringType& child,
                         FilePathWatcher::ChangeInfo change_info,
                         bool created,
                         bool deleted);

  // Returns whether the number of inotify watches of this FilePathWatcherImpl
  // would exceed the limit if adding one more.
  bool WouldExceedWatchLimit() const;

  // Returns a WatcherEntry for this, must be called on the original sequence.
  InotifyReader::WatcherEntry GetWatcherEntry();

 private:
  // Start watching |path| for changes and notify |delegate| on each change.
  // Returns true if watch for |path| has been added successfully.
  bool Watch(const FilePath& path,
             Type type,
             const FilePathWatcher::Callback& callback) override;

  // A generalized version. It extends |Type|.
  bool WatchWithOptions(const FilePath& path,
                        const WatchOptions& flags,
                        const FilePathWatcher::Callback& callback) override;

  bool WatchWithChangeInfo(
      const FilePath& path,
      const WatchOptions& options,
      const FilePathWatcher::CallbackWithChangeInfo& callback) override;

  // Cancel the watch. This unregisters the instance with InotifyReader.
  void Cancel() override;

  // Inotify watches are installed for all directory components of |target_|.
  // A WatchEntry instance holds:
  // - |watch|: the watch descriptor for a component.
  // - |subdir|: the subdirectory that identifies the next component.
  //   - For the last component, there is no next component, so it is empty.
  // - |linkname|: the target of the symlink.
  //   - Only if the target being watched is a symbolic link.
  struct WatchEntry {
    explicit WatchEntry(const FilePath::StringType& dirname)
        : watch(InotifyReader::kInvalidWatch), subdir(dirname) {}

    InotifyReader::Watch watch;
    FilePath::StringType subdir;
    FilePath::StringType linkname;
  };

  // Reconfigure to watch for the most specific parent directory of |target_|
  // that exists. Also calls UpdateRecursiveWatches() below. Returns true if
  // watch limit is not hit. Otherwise, returns false.
  [[nodiscard]] bool UpdateWatches();

  // Reconfigure to recursively watch |target_| and all its sub-directories.
  // - This is a no-op if the watch is not recursive.
  // - If |target_| does not exist, then clear all the recursive watches.
  // - Assuming |target_| exists, passing kInvalidWatch as |fired_watch| forces
  //   addition of recursive watches for |target_|.
  // - Otherwise, only the directory associated with |fired_watch| and its
  //   sub-directories will be reconfigured.
  // Returns true if watch limit is not hit. Otherwise, returns false.
  [[nodiscard]] bool UpdateRecursiveWatches(InotifyReader::Watch fired_watch,
                                            bool is_dir);

  // Enumerate recursively through |path| and add / update watches.
  // Returns true if watch limit is not hit. Otherwise, returns false.
  [[nodiscard]] bool UpdateRecursiveWatchesForPath(const FilePath& path);

  // Do internal bookkeeping to update mappings between |watch| and its
  // associated full path |path|.
  void TrackWatchForRecursion(InotifyReader::Watch watch, const FilePath& path);

  // Remove all the recursive watches.
  void RemoveRecursiveWatches();

  // |path| is a symlink to a non-existent target. Attempt to add a watch to
  // the link target's parent directory. Update |watch_entry| on success.
  // Returns true if watch limit is not hit. Otherwise, returns false.
  [[nodiscard]] bool AddWatchForBrokenSymlink(const FilePath& path,
                                              WatchEntry* watch_entry);

  bool HasValidWatchVector() const;

  // Callback to notify upon changes.
  FilePathWatcher::CallbackWithChangeInfo callback_;

  // The file or directory we're supposed to watch.
  FilePath target_;

  Type type_ = Type::kNonRecursive;
  bool report_modified_path_ = false;

  // The vector of watches and next component names for all path components,
  // starting at the root directory. The last entry corresponds to the watch for
  // |target_| and always stores an empty next component name in |subdir|.
  std::vector<WatchEntry> watches_;

  std::unordered_map<InotifyReader::Watch, FilePath> recursive_paths_by_watch_;
  std::map<FilePath, InotifyReader::Watch> recursive_watches_by_path_;

  WeakPtrFactory<FilePathWatcherImpl> weak_factory_{this};
};

LazyInstance<InotifyReader>::Leaky g_inotify_reader = LAZY_INSTANCE_INITIALIZER;

void InotifyReaderThreadDelegate::ThreadMain() {
  PlatformThread::SetName("inotify_reader");

  std::array<pollfd, 1> fdarray{{{inotify_fd_, POLLIN, 0}}};

  while (true) {
    // Wait until some inotify events are available.
    int poll_result = HANDLE_EINTR(poll(fdarray.data(), fdarray.size(), -1));
    if (poll_result < 0) {
      DPLOG(WARNING) << "poll failed";
      return;
    }

    // Adjust buffer size to current event queue size.
    int buffer_size;
    int ioctl_result = HANDLE_EINTR(ioctl(inotify_fd_, FIONREAD, &buffer_size));

    if (ioctl_result != 0 || buffer_size < 0) {
      DPLOG(WARNING) << "ioctl failed";
      return;
    }

    std::vector<char> buffer(static_cast<size_t>(buffer_size));

    ssize_t bytes_read = HANDLE_EINTR(
        read(inotify_fd_, buffer.data(), static_cast<size_t>(buffer_size)));

    if (bytes_read < 0) {
      DPLOG(WARNING) << "read from inotify fd failed";
      return;
    }

    for (size_t i = 0; i < static_cast<size_t>(bytes_read);) {
      inotify_event* event = reinterpret_cast<inotify_event*>(&buffer[i]);
      size_t event_size = sizeof(inotify_event) + event->len;
      DUMP_WILL_BE_CHECK_LE(i + event_size, static_cast<size_t>(bytes_read));
      g_inotify_reader.Get().OnInotifyEvent(event);
      i += event_size;
    }
  }
}

InotifyReader::InotifyReader()
    : inotify_fd_(inotify_init()), thread_delegate_(inotify_fd_) {
  if (inotify_fd_ < 0) {
    PLOG(ERROR) << "inotify_init() failed";
    return;
  }

  if (!StartThread())
    return;

  valid_ = true;
}

bool InotifyReader::StartThread() {
  // This object is LazyInstance::Leaky, so thread_delegate_ will outlive the
  // thread.
  return PlatformThread::CreateNonJoinable(0, &thread_delegate_);
}

InotifyReader::Watch InotifyReader::AddWatch(const FilePath& path,
                                             FilePathWatcherImpl* watcher) {
  if (!valid_)
    return kInvalidWatch;

  if (watcher->WouldExceedWatchLimit())
    return kWatchLimitExceeded;

  AutoLock auto_lock(lock_);

  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::WILL_BLOCK);
  const int watch_int =
      inotify_add_watch(inotify_fd_, path.value().c_str(),
                        IN_ATTRIB | IN_CREATE | IN_DELETE | IN_CLOSE_WRITE |
                            IN_MOVE | IN_ONLYDIR);
  if (watch_int == -1)
    return kInvalidWatch;
  const Watch watch = static_cast<Watch>(watch_int);

  watchers_[watch].emplace(std::make_pair(watcher, watcher->GetWatcherEntry()));

  return watch;
}

void InotifyReader::RemoveWatch(Watch watch, FilePathWatcherImpl* watcher) {
  if (!valid_ || (watch == kInvalidWatch))
    return;

  AutoLock auto_lock(lock_);

  auto watchers_it = watchers_.find(watch);
  if (watchers_it == watchers_.end())
    return;

  auto& watcher_map = watchers_it->second;
  watcher_map.erase(watcher);

  if (watcher_map.empty()) {
    watchers_.erase(watchers_it);

    ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                            BlockingType::WILL_BLOCK);
    inotify_rm_watch(inotify_fd_, watch);
  }
}

void InotifyReader::OnInotifyEvent(const inotify_event* event) {
  if (event->mask & IN_IGNORED)
    return;

  FilePath::StringType child(event->len ? event->name : FILE_PATH_LITERAL(""));
  AutoLock auto_lock(lock_);

  // In racing conditions, RemoveWatch() could grab `lock_` first and remove
  // the entry for `event->wd`.
  auto watchers_it = watchers_.find(static_cast<Watch>(event->wd));
  if (watchers_it == watchers_.end())
    return;

  auto& watcher_map = watchers_it->second;
  for (const auto& entry : watcher_map) {
    auto& watcher_entry = entry.second;

    FilePathWatcher::ChangeInfo change_info{
        .file_path_type = event->mask & IN_ISDIR
                              ? FilePathWatcher::FilePathType::kDirectory
                              : FilePathWatcher::FilePathType::kFile,
        .change_type = ToChangeType(event),
        .cookie =
            event->cookie ? std::make_optional(event->cookie) : std::nullopt,
    };
    bool created = event->mask & (IN_CREATE | IN_MOVED_TO);
    bool deleted = event->mask & (IN_DELETE | IN_MOVED_FROM);
    watcher_entry.task_runner->PostTask(
        FROM_HERE,
        BindOnce(&FilePathWatcherImpl::OnFilePathChanged, watcher_entry.watcher,
                 static_cast<Watch>(event->wd), child, std::move(change_info),
                 created, deleted));
  }
}

bool InotifyReader::HasWatches() {
  AutoLock auto_lock(lock_);

  return !watchers_.empty();
}

FilePathWatcherImpl::FilePathWatcherImpl() = default;

FilePathWatcherImpl::~FilePathWatcherImpl() {
  DUMP_WILL_BE_CHECK(!task_runner() ||
                     task_runner()->RunsTasksInCurrentSequence());
}

void FilePathWatcherImpl::OnFilePathChanged(
    InotifyReader::Watch fired_watch,
    const FilePath::StringType& child,
    FilePathWatcher::ChangeInfo change_info,
    bool created,
    bool deleted) {
  DUMP_WILL_BE_CHECK(task_runner()->RunsTasksInCurrentSequence());

  // Check to see if Cancel() has already been called.
  if (watches_.empty()) {
    return;
  }

  DUMP_WILL_BE_CHECK(HasValidWatchVector());

  // Used below to avoid multiple recursive updates.
  bool did_update = false;

  // Whether kWatchLimitExceeded is encountered during update.
  bool exceeded_limit = false;

  // Find the entries in |watches_| that correspond to |fired_watch|.
  for (size_t i = 0; i < watches_.size(); ++i) {
    const WatchEntry& watch_entry = watches_[i];
    if (fired_watch != watch_entry.watch)
      continue;

    // Check whether a path component of |target_| changed.
    bool change_on_target_path = child.empty() ||
                                 (child == watch_entry.linkname) ||
                                 (child == watch_entry.subdir);

    // Check if the change references |target_| or a direct child of |target_|.
    bool target_changed;
    if (watch_entry.subdir.empty()) {
      // The fired watch is for a WatchEntry without a subdir. Thus for a given
      // |target_| = "/path/to/foo", this is for "foo". Here, check either:
      // - the target has no symlink: it is the target and it changed.
      // - the target has a symlink, and it matches |child|.
      target_changed =
          (watch_entry.linkname.empty() || child == watch_entry.linkname);
    } else {
      // The fired watch is for a WatchEntry with a subdir. Thus for a given
      // |target_| = "/path/to/foo", this is for {"/", "/path", "/path/to"}.
      // So we can safely access the next WatchEntry since we have not reached
      // the end yet. Check |watch_entry| is for "/path/to", i.e. the next
      // element is "foo".
      bool next_watch_may_be_for_target = watches_[i + 1].subdir.empty();
      if (next_watch_may_be_for_target) {
        // The current |watch_entry| is for "/path/to", so check if the |child|
        // that changed is "foo".
        target_changed = watch_entry.subdir == child;
      } else {
        // The current |watch_entry| is not for "/path/to", so the next entry
        // cannot be "foo". Thus |target_| has not changed.
        target_changed = false;
      }
    }

    // Update watches if a directory component of the |target_| path
    // (dis)appears. Note that we don't add the additional restriction of
    // checking the event mask to see if it is for a directory here as changes
    // to symlinks on the target path will not have IN_ISDIR set in the event
    // masks. As a result we may sometimes call UpdateWatches() unnecessarily.
    if (change_on_target_path && (created || deleted) && !did_update) {
      if (!UpdateWatches()) {
        exceeded_limit = true;
        break;
      }
      did_update = true;
    }

    // Report the following events:
    //  - The target or a direct child of the target got changed (in case the
    //    watched path refers to a directory).
    //  - One of the parent directories got moved or deleted, since the target
    //    disappears in this case.
    //  - One of the parent directories appears. The event corresponding to
    //    the target appearing might have been missed in this case, so recheck.
    if (target_changed || (change_on_target_path && deleted) ||
        (change_on_target_path && created && PathExists(target_))) {
      if (!did_update) {
        if (!UpdateRecursiveWatches(
                fired_watch, change_info.file_path_type ==
                                 FilePathWatcher::FilePathType::kDirectory)) {
          exceeded_limit = true;
          break;
        }
        did_update = true;
      }
      FilePath modified_path = report_modified_path_ && !change_on_target_path
                                   ? target_.Append(child)
                                   : target_;
      callback_.Run(std::move(change_info), modified_path,
                    /*error=*/false);  // `this` may be deleted.
      return;
    }
  }

  if (!exceeded_limit && Contains(recursive_paths_by_watch_, fired_watch)) {
    if (!did_update) {
      if (!UpdateRecursiveWatches(
              fired_watch, change_info.file_path_type ==
                               FilePathWatcher::FilePathType::kDirectory)) {
        exceeded_limit = true;
      }
    }
    if (!exceeded_limit) {
      FilePath modified_path =
          report_modified_path_
              ? recursive_paths_by_watch_[fired_watch].Append(child)
              : target_;
      callback_.Run(std::move(change_info), modified_path,
                    /*error=*/false);  // `this` may be deleted.
      return;
    }
  }

  if (exceeded_limit) {
    // Cancels all in-flight events from inotify thread.
    weak_factory_.InvalidateWeakPtrs();

    // Reset states and cancels all watches.
    auto callback = callback_;
    Cancel();

    // Fires the error callback. `this` may be deleted as a result of this call.
    callback.Run(FilePathWatcher::ChangeInfo(), target_, /*error=*/true);
  }
}

bool FilePathWatcherImpl::WouldExceedWatchLimit() const {
  DUMP_WILL_BE_CHECK(task_runner()->RunsTasksInCurrentSequence());

  // `watches_` contains inotify watches of all dir components of `target_`.
  // `recursive_paths_by_watch_` contains inotify watches for sub dirs under
  // `target_` of a Type::kRecursive watcher and keyed by inotify watches.
  // All inotify watches used by this FilePathWatcherImpl are either in
  // `watches_` or as a key in `recursive_paths_by_watch_`. As a result, the
  // two provide a good estimate on the number of inofiy watches used by this
  // FilePathWatcherImpl.
  const size_t number_of_inotify_watches =
      watches_.size() + recursive_paths_by_watch_.size();
  return number_of_inotify_watches >= GetMaxNumberOfInotifyWatches();
}

InotifyReader::WatcherEntry FilePathWatcherImpl::GetWatcherEntry() {
  DUMP_WILL_BE_CHECK(task_runner()->RunsTasksInCurrentSequence());
  return {task_runner(), weak_factory_.GetWeakPtr()};
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
  DUMP_WILL_BE_CHECK(target_.empty());

  set_task_runner(SequencedTaskRunner::GetCurrentDefault());
  callback_ = callback;
  target_ = path;
  type_ = options.type;
  report_modified_path_ = options.report_modified_path;

  std::vector<FilePath::StringType> comps = target_.GetComponents();
  DUMP_WILL_BE_CHECK(!comps.empty());
  for (size_t i = 1; i < comps.size(); ++i) {
    watches_.emplace_back(comps[i]);
  }
  watches_.emplace_back(FilePath::StringType());

  if (!UpdateWatches()) {
    Cancel();
    // Note `callback` is not invoked since false is returned.
    return false;
  }

  return true;
}

void FilePathWatcherImpl::Cancel() {
  if (!callback_) {
    // Watch() was never called.
    set_cancelled();
    return;
  }

  DUMP_WILL_BE_CHECK(task_runner()->RunsTasksInCurrentSequence());
  DUMP_WILL_BE_CHECK(!is_cancelled());

  set_cancelled();
  callback_.Reset();

  for (const auto& watch : watches_)
    g_inotify_reader.Get().RemoveWatch(watch.watch, this);
  watches_.clear();
  target_.clear();
  RemoveRecursiveWatches();
}

bool FilePathWatcherImpl::UpdateWatches() {
  // Ensure this runs on the task_runner() exclusively in order to avoid
  // concurrency issues.
  DUMP_WILL_BE_CHECK(task_runner()->RunsTasksInCurrentSequence());
  DUMP_WILL_BE_CHECK(HasValidWatchVector());

  // Walk the list of watches and update them as we go.
  FilePath path(FILE_PATH_LITERAL("/"));
  for (WatchEntry& watch_entry : watches_) {
    InotifyReader::Watch old_watch = watch_entry.watch;
    watch_entry.watch = InotifyReader::kInvalidWatch;
    watch_entry.linkname.clear();
    watch_entry.watch = g_inotify_reader.Get().AddWatch(path, this);
    if (watch_entry.watch == InotifyReader::kWatchLimitExceeded)
      return false;
    if (watch_entry.watch == InotifyReader::kInvalidWatch) {
      // Ignore the error code (beyond symlink handling) to attempt to add
      // watches on accessible children of unreadable directories. Note that
      // this is a best-effort attempt; we may not catch events in this
      // scenario.
      if (IsLink(path)) {
        if (!AddWatchForBrokenSymlink(path, &watch_entry))
          return false;
      }
    }
    if (old_watch != watch_entry.watch)
      g_inotify_reader.Get().RemoveWatch(old_watch, this);
    path = path.Append(watch_entry.subdir);
  }

  return UpdateRecursiveWatches(InotifyReader::kInvalidWatch, /*is_dir=*/false);
}

bool FilePathWatcherImpl::UpdateRecursiveWatches(
    InotifyReader::Watch fired_watch,
    bool is_dir) {
  DUMP_WILL_BE_CHECK(HasValidWatchVector());

  if (type_ != Type::kRecursive)
    return true;

  if (!DirectoryExists(target_)) {
    RemoveRecursiveWatches();
    return true;
  }

  // Check to see if this is a forced update or if some component of |target_|
  // has changed. For these cases, redo the watches for |target_| and below.
  if (!Contains(recursive_paths_by_watch_, fired_watch) &&
      fired_watch != watches_.back().watch) {
    return UpdateRecursiveWatchesForPath(target_);
  }

  // Underneath |target_|, only directory changes trigger watch updates.
  if (!is_dir)
    return true;

  const FilePath& changed_dir = Contains(recursive_paths_by_watch_, fired_watch)
                                    ? recursive_paths_by_watch_[fired_watch]
                                    : target_;

  auto start_it = recursive_watches_by_path_.upper_bound(changed_dir);
  auto end_it = start_it;
  for (; end_it != recursive_watches_by_path_.end(); ++end_it) {
    const FilePath& cur_path = end_it->first;
    if (!changed_dir.IsParent(cur_path))
      break;

    // There could be a race when another process is changing contents under
    // `changed_dir` while chrome is watching (e.g. an Android app updating
    // a dir with Chrome OS file manager open for the dir). In such case,
    // `cur_dir` under `changed_dir` could exist in this loop but not in
    // the FileEnumerator loop in the upcoming UpdateRecursiveWatchesForPath(),
    // As a result, `g_inotify_reader` would have an entry in its `watchers_`
    // pointing to `this` but `this` is no longer aware of that. Crash in
    // http://crbug/990004 could happen later.
    //
    // Remove the watcher of `cur_path` regardless of whether it exists
    // or not to keep `this` and `g_inotify_reader` consistent even when the
    // race happens. The watcher will be added back if `cur_path` exists in
    // the FileEnumerator loop in UpdateRecursiveWatchesForPath().
    g_inotify_reader.Get().RemoveWatch(end_it->second, this);

    // Keep it in sync with |recursive_watches_by_path_| crbug.com/995196.
    recursive_paths_by_watch_.erase(end_it->second);
  }
  recursive_watches_by_path_.erase(start_it, end_it);

  // If `changed_dir` does not exist anymore, then there is no need to call
  // UpdateRecursiveWatchesForPath().
  if (!DirectoryExists(changed_dir)) {
    return true;
  }

  return UpdateRecursiveWatchesForPath(changed_dir);
}

bool FilePathWatcherImpl::UpdateRecursiveWatchesForPath(const FilePath& path) {
  DUMP_WILL_BE_CHECK_EQ(type_, Type::kRecursive);
  DUMP_WILL_BE_CHECK(!path.empty());
  DUMP_WILL_BE_CHECK(DirectoryExists(path));

  // Note: SHOW_SYM_LINKS exposes symlinks as symlinks, so they are ignored
  // rather than followed. Following symlinks can easily lead to the undesirable
  // situation where the entire file system is being watched.
  FileEnumerator enumerator(
      path, true /* recursive enumeration */,
      FileEnumerator::DIRECTORIES | FileEnumerator::SHOW_SYM_LINKS);
  for (FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    DUMP_WILL_BE_CHECK(enumerator.GetInfo().IsDirectory());

    // Check `recursive_watches_by_path_` as a heuristic to determine if this
    // needs to be an add or update operation.
    if (!Contains(recursive_watches_by_path_, current)) {
      // Try to add new watches.
      InotifyReader::Watch watch =
          g_inotify_reader.Get().AddWatch(current, this);
      if (watch == InotifyReader::kWatchLimitExceeded)
        return false;

      // The `watch` returned by inotify already exists. This is actually an
      // update operation.
      auto it = recursive_paths_by_watch_.find(watch);
      if (it != recursive_paths_by_watch_.end()) {
        recursive_watches_by_path_.erase(it->second);
        recursive_paths_by_watch_.erase(it);
      }
      TrackWatchForRecursion(watch, current);
    } else {
      // Update existing watches.
      InotifyReader::Watch old_watch = recursive_watches_by_path_[current];
      DUMP_WILL_BE_CHECK_NE(InotifyReader::kInvalidWatch, old_watch);
      InotifyReader::Watch watch =
          g_inotify_reader.Get().AddWatch(current, this);
      if (watch == InotifyReader::kWatchLimitExceeded)
        return false;
      if (watch != old_watch) {
        g_inotify_reader.Get().RemoveWatch(old_watch, this);
        recursive_paths_by_watch_.erase(old_watch);
        recursive_watches_by_path_.erase(current);
        TrackWatchForRecursion(watch, current);
      }
    }
  }
  return true;
}

void FilePathWatcherImpl::TrackWatchForRecursion(InotifyReader::Watch watch,
                                                 const FilePath& path) {
  DUMP_WILL_BE_CHECK_EQ(type_, Type::kRecursive);
  DUMP_WILL_BE_CHECK(!path.empty());
  DUMP_WILL_BE_CHECK(target_.IsParent(path));

  if (watch == InotifyReader::kInvalidWatch)
    return;

  DUMP_WILL_BE_CHECK(!Contains(recursive_paths_by_watch_, watch));
  DUMP_WILL_BE_CHECK(!Contains(recursive_watches_by_path_, path));
  recursive_paths_by_watch_[watch] = path;
  recursive_watches_by_path_[path] = watch;
}

void FilePathWatcherImpl::RemoveRecursiveWatches() {
  if (type_ != Type::kRecursive)
    return;

  for (const auto& it : recursive_paths_by_watch_)
    g_inotify_reader.Get().RemoveWatch(it.first, this);

  recursive_paths_by_watch_.clear();
  recursive_watches_by_path_.clear();
}

bool FilePathWatcherImpl::AddWatchForBrokenSymlink(const FilePath& path,
                                                   WatchEntry* watch_entry) {
#if BUILDFLAG(IS_FUCHSIA)
  // Fuchsia does not support symbolic links.
  return false;
#else   // BUILDFLAG(IS_FUCHSIA)
  DUMP_WILL_BE_CHECK_EQ(InotifyReader::kInvalidWatch, watch_entry->watch);
  std::optional<FilePath> link = ReadSymbolicLinkAbsolute(path);
  if (!link) {
    return true;
  }
  DUMP_WILL_BE_CHECK(link->IsAbsolute());

  // Try watching symlink target directory. If the link target is "/", then we
  // shouldn't get here in normal situations and if we do, we'd watch "/" for
  // changes to a component "/" which is harmless so no special treatment of
  // this case is required.
  InotifyReader::Watch watch =
      g_inotify_reader.Get().AddWatch(link->DirName(), this);
  if (watch == InotifyReader::kWatchLimitExceeded)
    return false;
  if (watch == InotifyReader::kInvalidWatch) {
    // TODO(craig) Symlinks only work if the parent directory for the target
    // exist. Ideally we should make sure we've watched all the components of
    // the symlink path for changes. See crbug.com/91561 for details.
    DPLOG(WARNING) << "Watch failed for " << link->DirName().value();
    return true;
  }
  watch_entry->watch = watch;
  watch_entry->linkname = link->BaseName().value();
  return true;
#endif  // BUILDFLAG(IS_FUCHSIA)
}

bool FilePathWatcherImpl::HasValidWatchVector() const {
  if (watches_.empty())
    return false;
  for (size_t i = 0; i < watches_.size() - 1; ++i) {
    if (watches_[i].subdir.empty())
      return false;
  }
  return watches_.back().subdir.empty();
}

}  // namespace

size_t GetMaxNumberOfInotifyWatches() {
#if BUILDFLAG(IS_FUCHSIA)
  // Fuchsia has no limit on the number of watches.
  return std::numeric_limits<int>::max();
#else
  static const size_t max = [] {
    size_t max_number_of_inotify_watches = 0u;

    std::ifstream in(kInotifyMaxUserWatchesPath);
    if (!in.is_open() || !(in >> max_number_of_inotify_watches)) {
      LOG(ERROR) << "Failed to read " << kInotifyMaxUserWatchesPath;
      return kDefaultInotifyMaxUserWatches / kExpectedFilePathWatchers;
    }

    return max_number_of_inotify_watches / kExpectedFilePathWatchers;
  }();
  return g_override_max_inotify_watches ? g_override_max_inotify_watches : max;
#endif  // if BUILDFLAG(IS_FUCHSIA)
}

ScopedMaxNumberOfInotifyWatchesOverrideForTest::
    ScopedMaxNumberOfInotifyWatchesOverrideForTest(size_t override_max) {
  DUMP_WILL_BE_CHECK_EQ(g_override_max_inotify_watches, 0u);
  g_override_max_inotify_watches = override_max;
}

ScopedMaxNumberOfInotifyWatchesOverrideForTest::
    ~ScopedMaxNumberOfInotifyWatchesOverrideForTest() {
  g_override_max_inotify_watches = 0u;
}

FilePathWatcher::FilePathWatcher()
    : FilePathWatcher(std::make_unique<FilePathWatcherImpl>()) {}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Put inside "BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)" because Android
// includes file_path_watcher_linux.cc.

// static
bool FilePathWatcher::HasWatchesForTest() {
  return g_inotify_reader.Get().HasWatches();
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace base

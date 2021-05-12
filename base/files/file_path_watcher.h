// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module provides a way to monitor a file or directory for changes.

#ifndef BASE_FILES_FILE_PATH_WATCHER_H_
#define BASE_FILES_FILE_PATH_WATCHER_H_

#include <memory>
#include <utility>

#include "base/base_export.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "build/build_config.h"

namespace base {

class FilePath;

// This class lets you register interest in changes on a FilePath.
// The callback will get called whenever the file or directory referenced by the
// FilePath is changed, including created or deleted. Due to limitations in the
// underlying OS APIs, FilePathWatcher has slightly different semantics on OS X
// than on Windows or Linux. FilePathWatcher on Linux and Windows will detect
// modifications to files in a watched directory. FilePathWatcher on Mac will
// detect the creation and deletion of files in a watched directory, but will
// not detect modifications to those files. See file_path_watcher_kqueue.cc for
// details.
//
// Must be destroyed on the sequence that invokes Watch().
class BASE_EXPORT FilePathWatcher {
 public:
  enum class Type {
    // Indicates that the watcher should watch the given path and its
    // ancestors for changes. If the path does not exist, its ancestors will
    // be watched in anticipation of it appearing later. If the path names a
    // directory, changes within the directory are not watched.
    kNonRecursive,

    // Indicates that the watcher should watch the given path, its ancestors,
    // and its descendants for changes. If the path names a directory, changes
    // within the directory are watched.
    kRecursive,

#if defined(OS_MAC)
    // Indicates that the watcher should watch the given path only (neither
    // ancestors nor descendants). The watch fails if the path does not exist.
    kTrivial,
#endif  // defined(OS_MAC)
  };

  // Callback type for Watch(). |path| points to the file that was updated,
  // and |error| is true if the platform specific code detected an error. In
  // that case, the callback won't be invoked again.
  using Callback =
      base::RepeatingCallback<void(const FilePath& path, bool error)>;

  // Used internally to encapsulate different members on different platforms.
  class PlatformDelegate {
   public:
    using Type = FilePathWatcher::Type;

    PlatformDelegate();
    PlatformDelegate(const PlatformDelegate&) = delete;
    PlatformDelegate& operator=(const PlatformDelegate&) = delete;
    virtual ~PlatformDelegate();

    // Start watching for the given |path| and notify |delegate| about changes.
    virtual bool Watch(const FilePath& path,
                       Type type,
                       const Callback& callback) WARN_UNUSED_RESULT = 0;

    // Stop watching. This is called from FilePathWatcher's dtor in order to
    // allow to shut down properly while the object is still alive.
    virtual void Cancel() = 0;

   protected:
    friend class FilePathWatcher;

    scoped_refptr<SequencedTaskRunner> task_runner() const {
      return task_runner_;
    }

    void set_task_runner(scoped_refptr<SequencedTaskRunner> runner) {
      task_runner_ = std::move(runner);
    }

    // Must be called before the PlatformDelegate is deleted.
    void set_cancelled() {
      cancelled_ = true;
    }

    bool is_cancelled() const {
      return cancelled_;
    }

   private:
    scoped_refptr<SequencedTaskRunner> task_runner_;
    bool cancelled_ = false;
  };

  FilePathWatcher();
  FilePathWatcher(const FilePathWatcher&) = delete;
  FilePathWatcher& operator=(const FilePathWatcher&) = delete;
  ~FilePathWatcher();

  // Returns true if the platform and OS version support recursive watches.
  static bool RecursiveWatchAvailable();

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Whether there are outstanding inotify watches.
  static bool HasWatchesForTest();
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

  // Starts watching |path| (and its descendants if |type| is kRecursive) for
  // changes. |callback| will be run on the caller's sequence to report such
  // changes. Returns true if the watch was started successfully and |callback|
  // may one day be run, or false in case of failure (e.g., a recursive watch on
  // platforms that do not support such).
  //
  // On POSIX, this must be called from a thread that supports
  // FileDescriptorWatcher.
  bool Watch(const FilePath& path, Type type, const Callback& callback);

 private:
  std::unique_ptr<PlatformDelegate> impl_;

  SequenceChecker sequence_checker_;
};

}  // namespace base

#endif  // BASE_FILES_FILE_PATH_WATCHER_H_

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exists for systems for which Chromium does not support watching
// file paths. This includes Unix systems that don't have the inotify headers
// and thus cannot build file_watcher_inotify.cc.

#include "base/files/file_path_watcher.h"

#include <memory>

#include "base/check.h"
#include "base/notimplemented.h"
#include "base/task/sequenced_task_runner.h"

namespace base {

namespace {

class FilePathWatcherImpl : public FilePathWatcher::PlatformDelegate {
 public:
  FilePathWatcherImpl() = default;
  FilePathWatcherImpl(const FilePathWatcherImpl&) = delete;
  FilePathWatcherImpl& operator=(const FilePathWatcherImpl&) = delete;
  ~FilePathWatcherImpl() override = default;

  // FilePathWatcher::PlatformDelegate:
  bool Watch(const FilePath& path,
             Type type,
             const FilePathWatcher::Callback& callback) override {
    DCHECK(!callback.is_null());

    NOTIMPLEMENTED_LOG_ONCE();
    return false;
  }
  void Cancel() override { set_cancelled(); }
};

}  // namespace

FilePathWatcher::FilePathWatcher()
    : FilePathWatcher(std::make_unique<FilePathWatcherImpl>()) {}

}  // namespace base

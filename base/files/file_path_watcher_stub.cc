// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exists for Unix systems which don't have the inotify headers, and
// thus cannot build file_watcher_inotify.cc

#include "base/files/file_path_watcher.h"

#include "base/memory/ptr_util.h"

namespace base {

namespace {

class FilePathWatcherImpl : public FilePathWatcher::PlatformDelegate {
 public:
  FilePathWatcherImpl() = default;
  FilePathWatcherImpl(const FilePathWatcherImpl&) = delete;
  FilePathWatcherImpl& operator=(const FilePathWatcherImpl&) = delete;
  ~FilePathWatcherImpl() override = default;

  bool Watch(const FilePath& path,
             Type type,
             const FilePathWatcher::Callback& callback) override {
    return false;
  }

  void Cancel() override {}
};

}  // namespace

FilePathWatcher::FilePathWatcher() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  impl_ = std::make_unique<FilePathWatcherImpl>();
}

}  // namespace base

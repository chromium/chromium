// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path_watcher.h"

#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/threading/sequenced_task_runner_handle.h"

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
             const FilePathWatcher::Callback& callback) override;
  void Cancel() override;
};

bool FilePathWatcherImpl::Watch(const FilePath& path,
                                Type type,
                                const FilePathWatcher::Callback& callback) {
  DCHECK(!callback.is_null());

  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void FilePathWatcherImpl::Cancel() {
  set_cancelled();
}

}  // namespace

FilePathWatcher::FilePathWatcher() {
  sequence_checker_.DetachFromSequence();
  impl_ = std::make_unique<FilePathWatcherImpl>();
}

}  // namespace base

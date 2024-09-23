// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Cross platform methods for FilePathWatcher. See the various platform
// specific implementation files, too.

#include "base/files/file_path_watcher.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "build/build_config.h"

namespace base {

FilePathWatcher::~FilePathWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  impl_->Cancel();
}

// static
bool FilePathWatcher::RecursiveWatchAvailable() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) ||        \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_AIX) || \
    BUILDFLAG(IS_FUCHSIA)
  return true;
#else
  // FSEvents isn't available on iOS.
  return false;
#endif
}

FilePathWatcher::PlatformDelegate::PlatformDelegate() = default;

FilePathWatcher::PlatformDelegate::~PlatformDelegate() {
  DCHECK(is_cancelled());
}

bool FilePathWatcher::Watch(const FilePath& path,
                            Type type,
                            const Callback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(path.IsAbsolute());
  return impl_->Watch(path, type, callback);
}

bool FilePathWatcher::WatchWithOptions(const FilePath& path,
                                       const WatchOptions& options,
                                       const Callback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(path.IsAbsolute());
  return impl_->WatchWithOptions(path, options, callback);
}

bool FilePathWatcher::WatchWithChangeInfo(
    const FilePath& path,
    const WatchOptions& options,
    const CallbackWithChangeInfo& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(path.IsAbsolute());
  return impl_->WatchWithChangeInfo(path, options, callback);
}

bool FilePathWatcher::PlatformDelegate::WatchWithOptions(
    const FilePath& path,
    const WatchOptions& options,
    const Callback& callback) {
  return Watch(path, options.type, callback);
}

bool FilePathWatcher::PlatformDelegate::WatchWithChangeInfo(
    const FilePath& path,
    const WatchOptions& options,
    const CallbackWithChangeInfo& callback) {
  return Watch(path, options.type, base::BindRepeating(callback, ChangeInfo()));
}

FilePathWatcher::FilePathWatcher(std::unique_ptr<PlatformDelegate> delegate) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  impl_ = std::move(delegate);
}

#if BUILDFLAG(IS_WIN)
Lock& FilePathWatcher::GetWatchThreadLockForTest() {
  return impl_->GetWatchThreadLockForTest();  // IN-TEST
}
#endif

}  // namespace base

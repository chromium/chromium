// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_file_util.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>

#include <optional>
#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/notimplemented.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace base {

namespace {

// Deny |permission| on the file |path|.
bool DenyFilePermission(const FilePath& path, mode_t permission) {
  stat_wrapper_t stat_buf;
  if (File::Stat(path, &stat_buf) != 0) {
    return false;
  }
  stat_buf.st_mode &= ~permission;

  int rv = HANDLE_EINTR(chmod(path.value().c_str(), stat_buf.st_mode));
  return rv == 0;
}

// Gets a mode_t indicating the permission information for `path`.
// Returns an empty value on failure.
std::optional<mode_t> GetPermissionInfo(const FilePath& path) {
  stat_wrapper_t stat_buf;
  if (File::Stat(path, &stat_buf) != 0) {
    return std::nullopt;
  }

  return stat_buf.st_mode & ~S_IFMT;  // Filter out file/path kind.
}

}  // namespace

bool DieFileDie(const FilePath& file, bool recurse) {
  // There is no need to workaround Windows problems on POSIX.
  // Just pass-through.
  if (recurse) {
    return DeletePathRecursively(file);
  }
  return DeleteFile(file);
}

void SyncPageCacheToDisk() {
  // On Linux (and Android) the sync(2) call waits for I/O completions.
  sync();
}

#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_APPLE) && \
    !BUILDFLAG(IS_ANDROID)
bool EvictFileFromSystemCache(const FilePath& file) {
  // There doesn't seem to be a POSIX way to cool the disk cache.
  NOTIMPLEMENTED();
  return false;
}
#endif

bool MakeFileUnreadable(const FilePath& path) {
  return DenyFilePermission(path, S_IRUSR | S_IRGRP | S_IROTH);
}

bool MakeFileUnwritable(const FilePath& path) {
  return DenyFilePermission(path, S_IWUSR | S_IWGRP | S_IWOTH);
}

struct FilePermissionRestorer::SavedFilePermissions {
  explicit SavedFilePermissions(mode_t mode) : mode_(mode) {}
  mode_t mode_;
};

FilePermissionRestorer::FilePermissionRestorer(const FilePath& path)
    : path_(path) {
  auto mode = GetPermissionInfo(path);
  CHECK(mode);
  permissions_ = std::make_unique<SavedFilePermissions>(*mode);
}

FilePermissionRestorer::~FilePermissionRestorer() {
  CHECK(permissions_);
  CHECK_EQ(HANDLE_EINTR(chmod(path_.value().c_str(), permissions_->mode_)), 0);
}

}  // namespace base

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_file_util.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>

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

// Gets a blob indicating the permission information for |path|.
// |length| is the length of the blob.  Zero on failure.
// Returns the blob pointer, or NULL on failure.
void* GetPermissionInfo(const FilePath& path, size_t* length) {
  DCHECK(length);
  *length = 0;

  stat_wrapper_t stat_buf;
  if (File::Stat(path, &stat_buf) != 0) {
    return nullptr;
  }

  *length = sizeof(mode_t);
  mode_t* mode = new mode_t;
  *mode = stat_buf.st_mode & ~S_IFMT;  // Filter out file/path kind.

  return mode;
}

// Restores the permission information for |path|, given the blob retrieved
// using |GetPermissionInfo()|.
// |info| is the pointer to the blob.
// |length| is the length of the blob.
// Either |info| or |length| may be NULL/0, in which case nothing happens.
bool RestorePermissionInfo(const FilePath& path, void* info, size_t length) {
  if (!info || (length == 0))
    return false;

  DCHECK_EQ(sizeof(mode_t), length);
  mode_t* mode = reinterpret_cast<mode_t*>(info);

  int rv = HANDLE_EINTR(chmod(path.value().c_str(), *mode));

  delete mode;

  return rv == 0;
}

}  // namespace

bool DieFileDie(const FilePath& file, bool recurse) {
  // There is no need to workaround Windows problems on POSIX.
  // Just pass-through.
  if (recurse)
    return DeletePathRecursively(file);
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

FilePermissionRestorer::FilePermissionRestorer(const FilePath& path)
    : path_(path), info_(nullptr), length_(0) {
  info_ = GetPermissionInfo(path_, &length_);
  DCHECK(info_ != nullptr);
  DCHECK_NE(0u, length_);
}

FilePermissionRestorer::~FilePermissionRestorer() {
  const bool success = RestorePermissionInfo(path_, info_, length_);
  CHECK(success);
}

}  // namespace base

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/cache_file_context.h"

#include "base/logging.h"

namespace ash::file_system_provider {

CacheFileContext::CacheFileContext(const std::string& version_tag,
                                   int64_t bytes_on_disk,
                                   int64_t id,
                                   const base::FilePath& path_on_disk)
    : bytes_on_disk_(bytes_on_disk),
      version_tag_(version_tag),
      id_(id),
      path_on_disk_(path_on_disk) {}

CacheFileContext::CacheFileContext(CacheFileContext&&) = default;

CacheFileContext::~CacheFileContext() = default;

bool CacheFileContext::CanGetLocalFD(const OpenedCloudFile& file) const {
  if (open_fds_.contains(file.request_id)) {
    // Already has access to the cached file.
    return true;
  }

  if (path_on_disk_.empty()) {
    VLOG(2) << "No cached file on disk yet";
    return false;
  }

  if (file.version_tag != version_tag_) {
    VLOG(2) << "File version does not match cached file version";
    return false;
  }

  if (evicted_) {
    VLOG(2) << "No new accesses allowed on evicted files";
    return false;
  }

  return true;
}

LocalFD& CacheFileContext::GetLocalFD(
    const OpenedCloudFile& file,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  CHECK(CanGetLocalFD(file));

  // Either get the existing LocalFD for the request_id or create a new one.
  auto [it, inserted] =
      open_fds_.try_emplace(file.request_id, path_on_disk_, io_task_runner);
  VLOG_IF(1, !inserted) << "Re-using cached file descriptor {request_id = '"
                        << file.request_id << "', path = '" << path_on_disk_
                        << "'}";
  return it->second;
}

}  // namespace ash::file_system_provider

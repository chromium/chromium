// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/cache_file_context.h"

#include "base/logging.h"

namespace ash::file_system_provider {

CacheFileContext::CacheFileContext(const std::string& version_tag,
                                   int64_t bytes_on_disk,
                                   int64_t id)
    : bytes_on_disk_(bytes_on_disk), version_tag_(version_tag), id_(id) {}

CacheFileContext::CacheFileContext(CacheFileContext&&) = default;

CacheFileContext::~CacheFileContext() = default;

LocalFD& CacheFileContext::GetOrCreateLocalFD(
    int request_id,
    base::FilePath path_on_disk,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  auto [it, inserted] =
      open_fds_.try_emplace(request_id, path_on_disk, io_task_runner);
  VLOG_IF(1, !inserted) << "Re-using cached file descriptor {request_id = '"
                        << request_id << "', path = '" << path_on_disk << "'}";
  return it->second;
}

}  // namespace ash::file_system_provider

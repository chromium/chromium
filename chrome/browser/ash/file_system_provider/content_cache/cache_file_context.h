// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_FILE_CONTEXT_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_FILE_CONTEXT_H_

#include <functional>
#include <map>
#include <utility>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_system_provider/content_cache/local_fd.h"

namespace ash::file_system_provider {

// Helper to explain the "-1" that is used to denote an unknown ID.
inline constexpr int kUnknownId = -1;

// Context relating to a file that is cached on disk. Used to make decisions
// around evicting files from the cache (e.g. until N bytes have been evicted)
// and guard against multiple writers to a single file.
class CacheFileContext {
 public:
  // When repopulating the context on session startup with the file information
  // that is already cached on disk, use the `bytes_on_disk` and `id` fields.
  // Otherwise leave the default values.
  explicit CacheFileContext(const std::string& version_tag,
                            int64_t bytes_on_disk = 0,
                            int64_t id = kUnknownId);

  CacheFileContext(CacheFileContext&&);
  CacheFileContext(const CacheFileContext&) = delete;
  CacheFileContext& operator=(const CacheFileContext&) = delete;

  ~CacheFileContext();

  bool HasLocalFD(int request_id) { return open_fds_.contains(request_id); }
  LocalFD& GetOrCreateLocalFD(
      int request_id,
      base::FilePath path_on_disk,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  bool CloseLocalFD(int request_id) { return open_fds_.erase(request_id) == 1; }

  int64_t bytes_on_disk() const { return bytes_on_disk_; }
  void set_bytes_on_disk(int64_t bytes_on_disk) {
    bytes_on_disk_ = bytes_on_disk;
  }

  base::Time accessed_time() const { return accessed_time_; }
  void set_accessed_time(base::Time accessed_time) {
    accessed_time_ = accessed_time;
  }

  const std::string& version_tag() const { return version_tag_; }

  int64_t id() const { return id_; }
  void set_id(int64_t id) { id_ = id; }

  bool has_writer() const { return has_writer_; }
  void set_has_writer(bool has_writer) { has_writer_ = has_writer; }

  bool pending_removal() const { return pending_removal_; }
  void set_pending_removal(bool pending_removal) {
    pending_removal_ = pending_removal;
  }

 private:
  // The number of contiguous bytes that are written to this file currently. If
  // a file write is in progress, this might not represent the entire size of
  // the file on disk.
  int64_t bytes_on_disk_;

  // The latest access time, cryptohome is mounted with MS_NOATIME so we need to
  // keep track of this separately.
  base::Time accessed_time_ = base::Time::Now();

  // The version tag for this specific file. This is read-only as the version
  // can never be changed without deleting the context for the file and
  // replacing it with an updated version.
  const std::string version_tag_;

  // A unique ID associated with this file that is used to write the file on
  // disk.
  int64_t id_;

  // True if there is an open writer to this file, multiple writers at
  // disjoint offset ranges is currently not supported.
  bool has_writer_ = false;

  // Evicted items are scheduled to be removed from disk and the database, so
  // any further use should be disallowed.
  bool pending_removal_ = false;

  // A map (keyed by request ID) that represents any open file descriptors for
  // this specific file.
  std::map<int, LocalFD> open_fds_;
};

using PathContextPair = std::pair<base::FilePath, CacheFileContext>;

}  // namespace ash::file_system_provider

template <>
struct std::hash<ash::file_system_provider::CacheFileContext> {
  // Enables the key to be retrieved from the std::pair that represents the
  // key-value pair in the LRU cache.
  constexpr const base::FilePath& operator()(
      const ash::file_system_provider::PathContextPair& pair) {
    return pair.first;
  }

  // Returns a hash for the std::pair to enable O(1) lookup in the
  // base::HashingLRUCache (hashes the std::string representation of the
  // base::FilePath key).
  constexpr size_t operator()(
      const ash::file_system_provider::PathContextPair& pair) const {
    return std::hash<std::string>{}(pair.first.value());
  }
};

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_FILE_CONTEXT_H_

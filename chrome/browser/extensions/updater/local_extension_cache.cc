// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/local_extension_cache.h"

#include <string>
#include <string_view>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/version.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {
namespace {

// File name extension for CRX files (not case sensitive).
const char kCRXFileExtension[] = ".crx";

// Delay between checks for flag file presence when waiting for the cache to
// become ready.
constexpr base::TimeDelta kCacheStatusPollingDelay = base::Seconds(1);

constexpr std::string_view kExtensionIdDelimiter = "\n";

}  // namespace

const char LocalExtensionCache::kCacheReadyFlagFileName[] = ".initialized";
const char LocalExtensionCache::kInvalidCacheIdsFileName[] = ".invalid_cache";

LocalExtensionCache::LocalExtensionCache(
    const base::FilePath& cache_dir,
    uint64_t max_cache_size,
    const base::TimeDelta& max_cache_age,
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner)
    : cache_dir_(cache_dir),
      max_cache_size_(max_cache_size),
      min_cache_age_(base::Time::Now() - max_cache_age),
      backend_task_runner_(backend_task_runner),
      state_(kUninitialized),
      cache_status_polling_delay_(kCacheStatusPollingDelay) {}

LocalExtensionCache::~LocalExtensionCache() {
  if (state_ == kReady)
    CleanUp();
}

void LocalExtensionCache::Init(bool wait_for_cache_initialization,
                               base::OnceClosure callback) {
  DCHECK_EQ(state_, kUninitialized);

  state_ = kWaitInitialization;
  if (wait_for_cache_initialization)
    CheckCacheStatus(std::move(callback));
  else
    CheckCacheContents(std::move(callback));
}

void LocalExtensionCache::Shutdown(base::OnceClosure callback) {
  DCHECK_NE(state_, kShutdown);
  if (state_ == kReady)
    CleanUp();
  state_ = kShutdown;
  backend_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                         std::move(callback));
}

// static
LocalExtensionCache::CacheMap::iterator LocalExtensionCache::FindExtension(
    CacheMap& cache,
    const std::string& id,
    const std::string& expected_hash) {
  CacheHit hit = cache.equal_range(id);
  CacheMap::iterator empty_hash = cache.end();
  std::string hash = base::ToLowerASCII(expected_hash);
  for (CacheMap::iterator it = hit.first; it != hit.second; ++it) {
    if (expected_hash.empty() || it->second.expected_hash == hash) {
      return it;
    }
    if (it->second.expected_hash.empty()) {
      empty_hash = it;
    }
  }
  return empty_hash;
}

bool LocalExtensionCache::GetExtension(const std::string& id,
                                       const std::string& expected_hash,
                                       base::FilePath* file_path,
                                       std::string* version) {
  if (state_ != kReady)
    return false;

  CacheMap::iterator it = FindExtension(cached_extensions_, id, expected_hash);
  if (it == cached_extensions_.end())
    return false;

  if (file_path) {
    *file_path = it->second.file_path;

    // If caller is not interested in file_path, extension is not used.
    base::Time now = base::Time::Now();
    backend_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&LocalExtensionCache::BackendMarkFileUsed,
                                  it->second.file_path, now));
    it->second.last_used = now;
  }

  if (version)
    *version = it->second.version.GetString();

  return true;
}

bool LocalExtensionCache::ShouldRetryDownload(
    const std::string& id,
    const std::string& expected_hash) {
  if (state_ != kReady)
    return false;

  // Should retry download only if in the previous attempt the extension was
  // present in the cache and the installer process failed. After the removal,
  // the extension is freshly downloaded.
  CacheMap::iterator it = FindExtension(cached_extensions_, id, expected_hash);
  if (it == cached_extensions_.end())
    return false;

  return true;
}

// static
bool LocalExtensionCache::NewerOrSame(const CacheMap::iterator& entry,
                                      const base::Version& version,
                                      const std::string& expected_hash,
                                      int* compare) {
  const base::Version& prev_version = entry->second.version;
  int cmp = version.CompareTo(prev_version);

  if (compare)
    *compare = cmp;

  // Cache entry is newer if its version is greater or same, and in the latter
  // case we will prefer the existing one if we are trying to add an
  // unhashed file, or we already have a hashed file in cache.
  return (cmp < 0 || (cmp == 0 && (expected_hash.empty() ||
                                   !entry->second.expected_hash.empty())));
}

void LocalExtensionCache::PutExtension(const std::string& id,
                                       const std::string& expected_hash,
                                       const base::FilePath& file_path,
                                       const base::Version& version,
                                       PutExtensionCallback callback) {
  if (state_ != kReady) {
    std::move(callback).Run(file_path, true);
    return;
  }

  if (!version.IsValid()) {
    LOG(ERROR) << "Extension " << id << " has bad version " << version;
    std::move(callback).Run(file_path, true);
    return;
  }

  CacheMap::iterator it = FindExtension(cached_extensions_, id, expected_hash);
  if (it != cached_extensions_.end() &&
      NewerOrSame(it, version, expected_hash, nullptr)) {
    LOG(WARNING) << "Cache contains newer or the same version "
                 << it->second.version << " for extension " << id << " version "
                 << version;
    std::move(callback).Run(file_path, true);
    return;
  }

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LocalExtensionCache::BackendInstallCacheEntry,
                     weak_ptr_factory_.GetWeakPtr(), cache_dir_, id,
                     expected_hash, file_path, version, std::move(callback)));
}

bool LocalExtensionCache::RemoveExtensionAt(const CacheMap::iterator& it,
                                            bool match_hash) {
  if (state_ != kReady || it == cached_extensions_.end())
    return false;
  std::string hash = match_hash ? it->second.expected_hash : std::string();
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LocalExtensionCache::BackendRemoveCacheEntry,
                                cache_dir_, it->first, hash));
  cached_extensions_.erase(it);
  return true;
}

bool LocalExtensionCache::RemoveExtension(const std::string& id,
                                          const std::string& expected_hash) {
  if (state_ != kReady)
    return false;

  CacheMap::iterator it = FindExtension(cached_extensions_, id, expected_hash);
  if (it == cached_extensions_.end())
    return false;

  while (it != cached_extensions_.end()) {
    RemoveExtensionAt(it, !expected_hash.empty());

    // For empty |expected_hash| this will iteratively return any cached file.
    // For any specific |expected_hash| this will only be able to find the
    // matching entry once.
    it = FindExtension(cached_extensions_, id, expected_hash);
  }

  return true;
}

bool LocalExtensionCache::RemoveOnNextInit(const std::string& id) {
  if (state_ != kReady) {
    return false;
  }

  if (base::Contains(invalid_cache_ids_, id)) {
    return true;
  }

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LocalExtensionCache::BackendMarkCacheInvalid,
                     weak_ptr_factory_.GetWeakPtr(), cache_dir_, id));
  invalid_cache_ids_.insert(id);
  return true;
}

bool LocalExtensionCache::GetStatistics(uint64_t* cache_size,
                                        size_t* extensions_count) {
  if (state_ != kReady)
    return false;

  *cache_size = 0;
  for (CacheMap::iterator it = cached_extensions_.begin();
       it != cached_extensions_.end(); ++it) {
    *cache_size += it->second.size;
  }
  *extensions_count = cached_extensions_.size();

  return true;
}

void LocalExtensionCache::SetCacheStatusPollingDelayForTests(
    const base::TimeDelta& delay) {
  cache_status_polling_delay_ = delay;
}

void LocalExtensionCache::CheckCacheStatus(base::OnceClosure callback) {
  if (state_ == kShutdown) {
    std::move(callback).Run();
    return;
  }

  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LocalExtensionCache::BackendCheckCacheStatus,
                                weak_ptr_factory_.GetWeakPtr(), cache_dir_,
                                std::move(callback)));
}

// static
void LocalExtensionCache::BackendCheckCacheStatus(
    base::WeakPtr<LocalExtensionCache> local_cache,
    const base::FilePath& cache_dir,
    base::OnceClosure callback) {
  base::FilePath ready_flag_file =
      cache_dir.AppendASCII(kCacheReadyFlagFileName);
  bool exists = base::PathExists(ready_flag_file);

  static bool already_warned = false;
  if (!exists && !base::SysInfo::IsRunningOnChromeOS()) {
    // This is a developer build. Automatically create the directory.
    if (base::CreateDirectory(cache_dir)) {
      base::File file(ready_flag_file, base::File::FLAG_OPEN_ALWAYS);
      if (file.IsValid()) {
        exists = true;
      } else if (!already_warned) {
        LOG(WARNING) << "Could not create cache file "
                     << ready_flag_file.value()
                     << "; extensions cannot be installed from update URLs.";
        already_warned = true;
      }
    } else if (!already_warned) {
      LOG(WARNING) << "Could not create cache directory " << cache_dir.value()
                   << "; extensions cannot be installed from update URLs.";
      already_warned = true;
    }
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&LocalExtensionCache::OnCacheStatusChecked,
                                local_cache, exists, std::move(callback)));
}

void LocalExtensionCache::OnCacheStatusChecked(bool ready,
                                               base::OnceClosure callback) {
  if (state_ == kShutdown) {
    std::move(callback).Run();
    return;
  }

  if (ready) {
    CheckCacheContents(std::move(callback));
  } else {
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&LocalExtensionCache::CheckCacheStatus,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        cache_status_polling_delay_);
  }
}

void LocalExtensionCache::CheckCacheContents(base::OnceClosure callback) {
  DCHECK_EQ(state_, kWaitInitialization);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LocalExtensionCache::BackendCheckCacheContents,
                                weak_ptr_factory_.GetWeakPtr(), cache_dir_,
                                std::move(callback)));
}

// static
void LocalExtensionCache::BackendCheckCacheContents(
    base::WeakPtr<LocalExtensionCache> local_cache,
    const base::FilePath& cache_dir,
    base::OnceClosure callback) {
  std::unique_ptr<CacheMap> cache_content(new CacheMap);
  BackendCheckCacheContentsInternal(cache_dir, cache_content.get());
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&LocalExtensionCache::OnCacheContentsChecked, local_cache,
                     std::move(cache_content), std::move(callback)));
}

// static
LocalExtensionCache::CacheMap::iterator LocalExtensionCache::InsertCacheEntry(
    CacheMap& cache,
    const std::string& id,
    const CacheItemInfo& info,
    const bool delete_files) {
  bool keep = true;
  std::string any_hash;
  // FindExtension with empty hash will always return the first one
  CacheMap::iterator it = FindExtension(cache, id, any_hash);
  if (it != cache.end()) {
    // |cache_content| already has some version for this ID. Remove older ones.
    // If we loook at the first cache entry, it may be:
    // 1. an older version (in which case we should remove all its instances)
    // 2. a newer version (in which case we should skip current file)
    // 3. the same version without hash (skip if our hash is empty,
    // 4. remove if our hash in not empty),
    // 5. the same version with hash (skip if our hash is empty,
    // 6. skip if there is already an entry with the same hash,
    // otherwise add a new entry).

    int cmp = 0;
    if (!NewerOrSame(it, info.version, info.expected_hash, &cmp)) {
      // Case #1 or #4, remove all instances from cache.
      while ((it != cache.end()) && (it->first == id)) {
        if (delete_files) {
          base::DeletePathRecursively(base::FilePath(it->second.file_path));
          VLOG(1) << "Remove older version " << it->second.version
                  << " for extension id " << id;
        }
        it = cache.erase(it);
      }
    } else if ((cmp < 0) || (cmp == 0 && info.expected_hash.empty())) {
      // Case #2, #3 or #5
      keep = false;
    } else if (cmp == 0) {
      // Same version, both hashes are not empty, try to find the same hash.
      while (keep && (it != cache.end()) && (it->first == id)) {
        if (it->second.expected_hash == info.expected_hash) {
          // Case #6
          keep = false;
        }
        ++it;
      }
    }
  }

  if (keep) {
    it = cache.insert(std::make_pair(id, info));
  } else {
    if (delete_files) {
      base::DeletePathRecursively(info.file_path);
      VLOG(1) << "Remove older version " << info.version << " for extension id "
              << id;
    }
    it = cache.end();
  }

  return it;
}

// static
void LocalExtensionCache::BackendCheckCacheContentsInternal(
    const base::FilePath& cache_dir,
    CacheMap* cache_content) {
  // Start by verifying that the cache_dir exists.
  if (!base::DirectoryExists(cache_dir)) {
    // Create it now.
    if (!base::CreateDirectory(cache_dir)) {
      LOG(ERROR) << "Failed to create cache directory at "
                 << cache_dir.value();
    }

    // Nothing else to do. Cache is empty.
    return;
  }

  std::set<std::string> invalid_cache = BackendGetInvalidCache(cache_dir);
  // Enumerate all the files in the cache |cache_dir|, including directories
  // and symlinks. Each unrecognized file will be erased.
  int types = base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES;
  base::FileEnumerator enumerator(cache_dir, false /* recursive */, types);
  for (base::FilePath path = enumerator.Next();
       !path.empty(); path = enumerator.Next()) {
    base::FileEnumerator::FileInfo info = enumerator.GetInfo();
    std::string basename = path.BaseName().value();

    if (info.IsDirectory() || base::IsLink(info.GetName())) {
      LOG(ERROR) << "Erasing bad file in cache directory: " << basename;
      base::DeletePathRecursively(path);
      continue;
    }

    // Skip flag file that indicates that cache is ready.
    if (basename == kCacheReadyFlagFileName)
      continue;
    // Skip file with extension ids of invalidated cache.
    if (basename == kInvalidCacheIdsFileName) {
      continue;
    }

    // crx files in the cache are named
    // <extension-id>-<version>[-<expected_hash>].crx.
    std::string id;
    std::string version;
    std::string expected_hash;
    if (base::EndsWith(basename, kCRXFileExtension,
                       base::CompareCase::INSENSITIVE_ASCII)) {
      size_t n = basename.find('-');
      if (n != std::string::npos && n + 1 < basename.size() - 4) {
        id = basename.substr(0, n);
        // Size of |version| = total size - "<id>" - "-" - ".crx"
        version = basename.substr(n + 1, basename.size() - 5 - id.size());

        n = version.find('-');
        if (n != std::string::npos && n + 1 < version.size()) {
          expected_hash = version.substr(n + 1, version.size() - n - 1);
          version.resize(n);
        }
      }
    }

    // Enforce a lower-case id.
    id = base::ToLowerASCII(id);
    if (!crx_file::id_util::IdIsValid(id)) {
      LOG(ERROR) << "Bad extension id in cache: " << id;
      id.clear();
    }

    if (!base::Version(version).IsValid()) {
      LOG(ERROR) << "Bad extension version in cache: " << version;
      version.clear();
    }

    if (id.empty() || version.empty()) {
      LOG(ERROR) << "Invalid file in cache, erasing: " << basename;
      base::DeletePathRecursively(path);
      continue;
    }

    if (base::Contains(invalid_cache, id)) {
      base::DeleteFile(path);
      continue;
    }

    VLOG(1) << "Found cached version " << version
            << " for extension id " << id;

    InsertCacheEntry(
        *cache_content, id,
        CacheItemInfo(base::Version(version), expected_hash,
                      info.GetLastModifiedTime(), info.GetSize(), path),
        true);
  }

  // Delete the invalid cache file.
  base::FilePath invalid_cache_file =
      cache_dir.AppendASCII(kInvalidCacheIdsFileName);
  if (!base::DeleteFile(invalid_cache_file)) {
    LOG(WARNING) << "Failed to delete cache invalidation file "
                 << invalid_cache_file;
  }
}

void LocalExtensionCache::OnCacheContentsChecked(
    std::unique_ptr<CacheMap> cache_content,
    base::OnceClosure callback) {
  cache_content->swap(cached_extensions_);
  state_ = kReady;
  std::move(callback).Run();
}

// static
void LocalExtensionCache::BackendMarkFileUsed(const base::FilePath& file_path,
                                              const base::Time& time) {
  base::TouchFile(file_path, time, time);
}

// static
std::string LocalExtensionCache::ExtensionFileName(
    const std::string& id,
    const std::string& version,
    const std::string& expected_hash) {
  std::string filename = id + "-" + version;
  if (!expected_hash.empty())
    filename += "-" + base::ToLowerASCII(expected_hash);
  filename += kCRXFileExtension;
  return filename;
}

// static
void LocalExtensionCache::BackendInstallCacheEntry(
    base::WeakPtr<LocalExtensionCache> local_cache,
    const base::FilePath& cache_dir,
    const std::string& id,
    const std::string& expected_hash,
    const base::FilePath& file_path,
    const base::Version& version,
    PutExtensionCallback callback) {
  std::string basename =
      ExtensionFileName(id, version.GetString(), expected_hash);
  base::FilePath cached_crx_path = cache_dir.AppendASCII(basename);

  bool was_error = false;
  if (base::PathExists(cached_crx_path)) {
    LOG(ERROR) << "File already exists " << file_path.value();
    cached_crx_path = file_path;
    was_error = true;
  }

  base::File::Info info;
  if (!was_error) {
    if (!base::Move(file_path, cached_crx_path)) {
      LOG(ERROR) << "Failed to copy from " << file_path.value()
                 << " to " << cached_crx_path.value();
      cached_crx_path = file_path;
      was_error = true;
    } else {
      was_error = !base::GetFileInfo(cached_crx_path, &info);
      VLOG(1) << "Cache entry installed for extension id " << id
              << " version " << version;
    }
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&LocalExtensionCache::OnCacheEntryInstalled, local_cache,
                     id,
                     CacheItemInfo(version, expected_hash, info.last_modified,
                                   info.size, cached_crx_path),
                     was_error, std::move(callback)));
}

void LocalExtensionCache::OnCacheEntryInstalled(const std::string& id,
                                                const CacheItemInfo& info,
                                                bool was_error,
                                                PutExtensionCallback callback) {
  if (state_ == kShutdown || was_error) {
    // If |was_error| is true, it means that the |info.file_path| refers to the
    // original downloaded file, otherwise it refers to a file in cache, which
    // should not be deleted by CrxInstaller.
    std::move(callback).Run(info.file_path, was_error);
    return;
  }

  CacheMap::iterator it = InsertCacheEntry(cached_extensions_, id, info, false);
  if (it == cached_extensions_.end()) {
    LOG(WARNING) << "Cache contains newer or the same version for extension "
                 << id << " version " << info.version;
    std::move(callback).Run(info.file_path, true);
    return;
  }

  // Time from file system can have lower precision so use precise "now".
  it->second.last_used = base::Time::Now();

  std::move(callback).Run(info.file_path, false);
}

// static
void LocalExtensionCache::BackendRemoveCacheEntry(
    const base::FilePath& cache_dir,
    const std::string& id,
    const std::string& expected_hash) {
  std::string file_pattern = ExtensionFileName(id, "*", expected_hash);
  base::FileEnumerator enumerator(cache_dir,
                                  false /* not recursive */,
                                  base::FileEnumerator::FILES,
                                  file_pattern);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    base::DeleteFile(path);
    VLOG(1) << "Removed cached file " << path.value();
  }
}

// static
void LocalExtensionCache::BackendMarkCacheInvalid(
    base::WeakPtr<LocalExtensionCache> local_cache,
    const base::FilePath& cache_dir,
    const std::string& extension_id) {
  base::FilePath invalid_cache_file =
      cache_dir.AppendASCII(kInvalidCacheIdsFileName);
  std::string contents = base::ToString(extension_id, kExtensionIdDelimiter);
  bool success = false;
  if (!base::PathExists(invalid_cache_file)) {
    success = base::WriteFile(invalid_cache_file, contents);
  } else {
    success = base::AppendToFile(invalid_cache_file, contents);
  }

  if (!success) {
    static bool already_warned = false;
    if (!already_warned) {
      LOG(WARNING) << "Failed writing obsolete cache extension id "
                   << extension_id << " to file " << invalid_cache_file;
      already_warned = true;
    }
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&LocalExtensionCache::OnMarkCacheInvalidFailed,
                       local_cache, extension_id));
    return;
  }
}

void LocalExtensionCache::OnMarkCacheInvalidFailed(
    const std::string& extension_id) {
  // Erase extension id from invalid ids since it was not marked invalid
  // successfully.
  invalid_cache_ids_.erase(extension_id);
}

// static
std::set<std::string> LocalExtensionCache::BackendGetInvalidCache(
    const base::FilePath& cache_dir) {
  base::FilePath file = cache_dir.AppendASCII(kInvalidCacheIdsFileName);
  std::string contents;
  base::ReadFileToString(file, &contents);

  auto extension_ids =
      base::SplitString(contents, kExtensionIdDelimiter, base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  return {std::make_move_iterator(extension_ids.begin()),
          std::make_move_iterator(extension_ids.end())};
}

// static
bool LocalExtensionCache::CompareCacheItemsAge(const CacheMap::iterator& lhs,
                                               const CacheMap::iterator& rhs) {
  return lhs->second.last_used < rhs->second.last_used;
}

void LocalExtensionCache::CleanUp() {
  DCHECK_EQ(state_, kReady);

  std::vector<CacheMap::iterator> items;
  items.reserve(cached_extensions_.size());
  uint64_t total_size = 0;
  for (CacheMap::iterator it = cached_extensions_.begin();
       it != cached_extensions_.end(); ++it) {
    items.push_back(it);
    total_size += it->second.size;
  }
  std::sort(items.begin(), items.end(), CompareCacheItemsAge);

  for (std::vector<CacheMap::iterator>::iterator it = items.begin();
       it != items.end(); ++it) {
    if ((*it)->second.last_used < min_cache_age_ ||
        (max_cache_size_ && total_size > max_cache_size_)) {
      total_size -= (*it)->second.size;
      VLOG(1) << "Clean up cached extension id " << (*it)->first;
      RemoveExtensionAt(*it, true);
    }
  }
}

LocalExtensionCache::CacheItemInfo::CacheItemInfo(
    const base::Version& version,
    const std::string& expected_hash,
    const base::Time& last_used,
    uint64_t size,
    const base::FilePath& file_path)
    : version(version),
      expected_hash(base::ToLowerASCII(expected_hash)),
      last_used(last_used),
      size(size),
      file_path(file_path) {}

LocalExtensionCache::CacheItemInfo::CacheItemInfo(const CacheItemInfo& other) =
    default;

LocalExtensionCache::CacheItemInfo::~CacheItemInfo() {
}

}  // namespace extensions

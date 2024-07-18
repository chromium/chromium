// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_LOCAL_EXTENSION_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_LOCAL_EXTENSION_CACHE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/version.h"

namespace base {
class SequencedTaskRunner;
}

namespace extensions {

// Cache .crx files in some local dir for future use. Cache keeps only latest
// version of the extensions. Only one instance of LocalExtensionCache can work
// with the same directory. But LocalExtensionCache instance can be shared
// between multiple clients. Public interface can be used only from UI thread.
class LocalExtensionCache {
 public:
  // Callback invoked on UI thread when PutExtension is completed.
  using PutExtensionCallback =
      base::OnceCallback<void(const base::FilePath& file_path,
                              bool file_ownership_passed)>;

  // |cache_dir| - directory that will be used for caching CRX files.
  // |max_cache_size| - maximum disk space that cache can use, 0 means no limit.
  // |max_cache_age| - maximum age that unused item can be kept in cache, 0 age
  // means that all unused cache items will be removed on Shutdown.
  // All file I/O is done via the |backend_task_runner|.
  LocalExtensionCache(
      const base::FilePath& cache_dir,
      uint64_t max_cache_size,
      const base::TimeDelta& max_cache_age,
      const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner);

  LocalExtensionCache(const LocalExtensionCache&) = delete;
  LocalExtensionCache& operator=(const LocalExtensionCache&) = delete;

  ~LocalExtensionCache();

  // Name of flag file that indicates that cache is ready (import finished).
  static const char kCacheReadyFlagFileName[];
  // Name of the file that lists invalid cache that should be removed.
  static const char kInvalidCacheIdsFileName[];

  // Initialize cache. If |wait_for_cache_initialization| is |true|, the cache
  // contents will not be read until a flag file appears in the cache directory,
  // signaling that the cache is ready. The |callback| is called when cache is
  // ready and cache dir content was already checked.
  void Init(bool wait_for_cache_initialization, base::OnceClosure callback);

  // Shut down the cache. The |callback| will be invoked when the cache has shut
  // down completely and there are no more pending file I/O operations.
  void Shutdown(base::OnceClosure callback);

  // If extension with |id| and |expected_hash| exists in the cache (or there
  // is an extension with the same |id|, but without expected hash sum),
  // returns |true|, |file_path| and |version| for the found extension.
  // If |file_path| was requested, then extension will be marked as used with
  // current timestamp.
  bool GetExtension(const std::string& id,
                    const std::string& expected_hash,
                    base::FilePath* file_path,
                    std::string* version);

  // Returns |true| if there is a file with |id| and |expected_hash| in the
  // cache, and its hash sum is actually empty. After removing it from cache and
  // re-downloading, the new entry will have some non-empty hash sum.
  bool ShouldRetryDownload(const std::string& id,
                           const std::string& expected_hash);

  // Put extension with |id|, |version| and |expected_hash| into local cache.
  // Older version in the cache will be deleted on next run so it can be safely
  // used. Extension will be marked as used with current timestamp. The file
  // will be available via GetExtension when |callback| is called. PutExtension
  // may get ownership of |file_path| or return it back via |callback|.
  void PutExtension(const std::string& id,
                    const std::string& expected_hash,
                    const base::FilePath& file_path,
                    const base::Version& version,
                    PutExtensionCallback callback);

  // Remove extension with |id| and |expected_hash| from local cache,
  // corresponding crx file will be removed from disk too. If |expected_hash| is
  // empty, all files corresponding to that |id| will be removed.
  bool RemoveExtension(const std::string& id, const std::string& expected_hash);

  // Mark extension with |id| as obsolete and schedule removal for the next
  // initialization. Returns true if removal was scheduled successfully.
  bool RemoveOnNextInit(const std::string& id);

  // Return cache statistics. Returns |false| if cache is not ready.
  bool GetStatistics(uint64_t* cache_size, size_t* extensions_count);

  // Outputs properly formatted extension file name, as it will be stored in
  // cache. If |expected_hash| is empty, it will be <id>-<version>.crx,
  // otherwise the name format is <id>-<version>-<hash>.crx.
  static std::string ExtensionFileName(const std::string& id,
                                       const std::string& version,
                                       const std::string& expected_hash);

  bool is_ready() const { return state_ == kReady; }
  bool is_uninitialized() const { return state_ == kUninitialized; }
  bool is_shutdown() const { return state_ == kShutdown; }

  // For tests only!
  void SetCacheStatusPollingDelayForTests(const base::TimeDelta& delay);

 private:
  struct CacheItemInfo {
    base::Version version;
    std::string expected_hash;
    base::Time last_used;
    uint64_t size;
    base::FilePath file_path;

    CacheItemInfo(const base::Version& version,
                  const std::string& expected_hash,
                  const base::Time& last_used,
                  uint64_t size,
                  const base::FilePath& file_path);
    CacheItemInfo(const CacheItemInfo& other);
    ~CacheItemInfo();
  };
  typedef std::multimap<std::string, CacheItemInfo> CacheMap;
  typedef std::pair<CacheMap::iterator, CacheMap::iterator> CacheHit;

  enum State {
    kUninitialized,
    kWaitInitialization,
    kReady,
    kShutdown
  };

  // Helper function that searches the cache map for an extension with the
  // specified |id| and |expected_hash|. If there is an extension with empty
  // hash in the map, it will be returned. If |expected_hash| is empty, returns
  // the first extension with the same |id|.
  static CacheMap::iterator FindExtension(CacheMap& cache,
                                          const std::string& id,
                                          const std::string& expected_hash);

  // Helper function that compares a cache entry (typically returned from
  // FindExtension) with an incoming |version| and |expected_hash|. Comparison
  // is based on the version number (newer is better) and hash sum (it is
  // better to have a file with an expected hash sum than without it).
  // Return value of this function is |true| if we already have a 'better'
  // entry in cache (considering both version number and hash sum), and the
  // value of |compare| is set to the version number comparison result (as
  // returned by Version::CompareTo).
  static bool NewerOrSame(const CacheMap::iterator& entry,
                          const base::Version& version,
                          const std::string& expected_hash,
                          int* compare);

  // Helper function that checks if there is already a newer version of the
  // extension we want to add to the cache, or if there is already a file with a
  // hash sum (and we are trying to add one without it), or vice versa. Keeps
  // the invariant of having only one version of each extension, and either only
  // unhashed (single) or only hashed (multiple) variants of that version.
  // |delete_files| specifies if this function is called on startup (in which
  // case we will clean up files we don't need), or on extension install.
  // Returns cache.end() if the extension is already cached, or an iterator to
  // the inserted cache entry otherwise.
  static CacheMap::iterator InsertCacheEntry(CacheMap& cache,
                                             const std::string& id,
                                             const CacheItemInfo& info,
                                             const bool delete_files);

  // Remove extension at a specified iterator. This is necessary because
  // removing an extension by |id| and |expected_hash| taken by reference from
  // an iterator leads to use-after-free. On the other hand, when passing the
  // iterator itself we avoid lookup as such, at all.
  // For external calls from RemoveExtension without expected hash we will
  // ignore the hash in iterator by setting |match_hash| to false.
  bool RemoveExtensionAt(const CacheMap::iterator& it, bool match_hash);

  // Sends BackendCheckCacheStatus task on backend thread.
  void CheckCacheStatus(base::OnceClosure callback);

  // Checks whether a flag file exists in the |cache_dir|, indicating that the
  // cache is ready. This method is invoked via the |backend_task_runner_| and
  // posts its result back to the |local_cache| on the UI thread.
  static void BackendCheckCacheStatus(
      base::WeakPtr<LocalExtensionCache> local_cache,
      const base::FilePath& cache_dir,
      base::OnceClosure callback);

  // Invoked on the UI thread after checking whether the cache is ready. If the
  // cache is not ready yet, posts a delayed task that will repeat the check,
  // thus polling for cache readiness.
  void OnCacheStatusChecked(bool ready, base::OnceClosure callback);

  // Checks the cache contents. This is a helper that invokes the actual check
  // by posting to the |backend_task_runner_|.
  void CheckCacheContents(base::OnceClosure callback);

  // Checks the cache contents. This method is invoked via the
  // |backend_task_runner_| and posts back a list of cache entries to the
  // |local_cache| on the UI thread.
  static void BackendCheckCacheContents(
      base::WeakPtr<LocalExtensionCache> local_cache,
      const base::FilePath& cache_dir,
      base::OnceClosure callback);

  // Helper for BackendCheckCacheContents() that updates |cache_content|.
  static void BackendCheckCacheContentsInternal(
      const base::FilePath& cache_dir,
      CacheMap* cache_content);

  // Invoked when the cache content on disk has been checked. |cache_content|
  // contains all the currently valid crx files in the cache.
  void OnCacheContentsChecked(std::unique_ptr<CacheMap> cache_content,
                              base::OnceClosure callback);

  // Update timestamp for the file to mark it as "used". This method is invoked
  // via the |backend_task_runner_|.
  static void BackendMarkFileUsed(const base::FilePath& file_path,
                                  const base::Time& time);

  // Installs the downloaded crx file at |path| in the |cache_dir|. This method
  // is invoked via the |backend_task_runner_|.
  static void BackendInstallCacheEntry(
      base::WeakPtr<LocalExtensionCache> local_cache,
      const base::FilePath& cache_dir,
      const std::string& id,
      const std::string& expected_hash,
      const base::FilePath& file_path,
      const base::Version& version,
      PutExtensionCallback callback);

  // Invoked on the UI thread when a new entry has been installed in the cache.
  void OnCacheEntryInstalled(const std::string& id,
                             const CacheItemInfo& info,
                             bool was_error,
                             PutExtensionCallback callback);

  // Remove cached crx files(all versions) under |cached_dir| for extension with
  // |id|. This method is invoked via the |backend_task_runner_|.
  static void BackendRemoveCacheEntry(const base::FilePath& cache_dir,
                                      const std::string& expected_hash,
                                      const std::string& id);

  // Appends the extension id to |kInvalidCacheIdsFileName| file so that it is
  // deleted on the next startup and a new version is retrieved.
  static void BackendMarkCacheInvalid(
      base::WeakPtr<LocalExtensionCache> local_cache,
      const base::FilePath& cache_dir,
      const std::string& extension_id);

  // Invoked on the UI thread if BackendMarkCacheInvalid() fails.
  void OnMarkCacheInvalidFailed(const std::string& extension_id);

  // Gets extension ids from |kInvalidCacheIdsFileName| file.
  static std::set<std::string> BackendGetInvalidCache(
      const base::FilePath& cache_dir);

  // Compare two cache items returns true if first item is older.
  static bool CompareCacheItemsAge(const CacheMap::iterator& lhs,
                                   const CacheMap::iterator& rhs);

  // Calculate which files need to be deleted and schedule files deletion.
  void CleanUp();

  // Path to the directory where the extension cache is stored.
  base::FilePath cache_dir_;

  // Maximum size of cache dir on disk.
  uint64_t max_cache_size_;

  // Minimal age of unused item in cache, items prior to this age will be
  // deleted on shutdown.
  base::Time min_cache_age_;

  // Task runner for executing file I/O tasks.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Track state of the instance.
  State state_;

  // This contains info about all cached extensions.
  CacheMap cached_extensions_;

  // This contains extension ids of invalid cache that should be removed on the
  // next initialization.
  std::set<std::string> invalid_cache_ids_;

  // Delay between polling cache status.
  base::TimeDelta cache_status_polling_delay_;

  // Weak factory for callbacks from the backend and delayed tasks.
  base::WeakPtrFactory<LocalExtensionCache> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_LOCAL_EXTENSION_CACHE_H_

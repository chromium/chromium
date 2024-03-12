// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_MANAGER_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_MANAGER_H_

#include <set>

#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/file_system_provider/abort_callback.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_cache.h"

namespace ash::file_system_provider {

// Callback type used when an FSP has been intiialized.
using FileErrorOrContentCacheCallback =
    base::OnceCallback<void(base::FileErrorOr<std::unique_ptr<ContentCache>>)>;

// The root directory name that houses all FSP content caches.
inline constexpr char kFspContentCacheDirName[] = "FspContentCache";

// A singleton that is the hub for all FileSystemProvider extensions that are
// enabled with a content cache. Currently this is just an experiment hidden
// behind both the `FileSystemProviderCloudFileSystem` and
// `FileSystemProviderContentCache` flags and only enabled on ODFS when the
// flags are toggled on.
// Supply the flag `in_memory_only` to avoid creating the FSP cache directory.
class CacheManager {
 public:
  // Observer class to be notified about changes happening in the CacheManager.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the initialization for a ContentCache for a
    // FileSystemProvider is complete.
    virtual void OnContentCacheInitializeComplete(base::FilePath mount_path,
                                                  base::File::Error result) {}
  };

  explicit CacheManager(const base::FilePath& profile_path,
                        bool in_memory_only = false);

  CacheManager(const CacheManager&) = delete;
  CacheManager& operator=(const CacheManager&) = delete;

  ~CacheManager();

  // Setup the cache directory for the specific FSP.
  void InitializeForProvider(const base::FilePath& provider_mount_path,
                             FileErrorOrContentCacheCallback callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Responds to the FSP with the a `ContentCache` instance if directory
  // creation was successful (or `in_memory_only` is true).
  void OnInitializeForProvider(FileErrorOrContentCacheCallback callback,
                               base::FilePath mount_path,
                               base::File::Error result);

  const base::FilePath profile_path_;
  bool in_memory_only_ = false;
  std::set<base::FilePath> initialized_providers_;
  base::ObserverList<Observer> observers_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_{
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})};

  base::WeakPtrFactory<CacheManager> weak_ptr_factory_{this};
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_MANAGER_H_

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
using FileErrorOrContentCache =
    base::FileErrorOr<std::unique_ptr<ContentCache>>;
using FileErrorOrContentCacheCallback =
    base::OnceCallback<void(FileErrorOrContentCache)>;

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
    // Called when the initialization for a provider is complete.
    virtual void OnProviderInitializationComplete(
        base::FilePath base64_encoded_provider_folder_name,
        base::File::Error result) {}

    // Called when a provider has been uninitialized.
    virtual void OnProviderUninitialized(
        base::FilePath base64_encoded_provider_folder_name,
        base::File::Error result) {}
  };

  virtual ~CacheManager() = default;

  // Setup the cache directory for the specific FSP.
  virtual void InitializeForProvider(
      const ProvidedFileSystemInfo& file_system_info,
      FileErrorOrContentCacheCallback callback) = 0;

  // Destruction of the cache directory for the specific FSP.
  virtual void UninitializeForProvider(
      const ProvidedFileSystemInfo& file_system_info) = 0;

  virtual bool IsProviderInitialized(
      const ProvidedFileSystemInfo& file_system_info) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_MANAGER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_MANAGER_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_MANAGER_H_

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/file_system_provider/abort_callback.h"

namespace ash::file_system_provider {

// Callback type used when an FSP has been intiialized.
using FileErrorCallback = base::OnceCallback<void(base::File::Error)>;

// The root directory name that houses all FSP content caches.
inline constexpr char kFspContentCacheDirName[] = "FspContentCache";

// A singleton that is the hub for all FileSystemProvider extensions that are
// enabled with a content cache. Currently this is just an experiment hidden
// behind both the `FileSystemProviderCloudFileSystem` and
// `FileSystemProviderContentCache` flags and only enabled on ODFS when the
// flags are toggled on.
class CacheManager {
 public:
  explicit CacheManager(const base::FilePath& profile_path);

  CacheManager(const CacheManager&) = delete;
  CacheManager& operator=(const CacheManager&) = delete;

  ~CacheManager();

  // Setup the cache directory for the specific FSP.
  void InitializeForProvider(const base::FilePath& provider_mount_path,
                             FileErrorCallback callback);

 private:
  void OnInitialized(FileErrorCallback callback,
                     base::FilePath mount_path,
                     base::File::Error result);

  const base::FilePath profile_path_;
  std::set<base::FilePath> initialized_providers_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_{
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})};

  base::WeakPtrFactory<CacheManager> weak_ptr_factory_{this};
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_MANAGER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_MANAGER_IMPL_H_

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
#include "chrome/browser/ash/file_system_provider/content_cache/cache_manager.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_cache.h"

namespace ash::file_system_provider {

class CacheManagerImpl : public CacheManager {
 public:
  explicit CacheManagerImpl(const base::FilePath& profile_path,
                            bool in_memory_only = false);

  CacheManagerImpl(const CacheManagerImpl&) = delete;
  CacheManagerImpl& operator=(const CacheManagerImpl&) = delete;

  ~CacheManagerImpl() override;

  static std::unique_ptr<CacheManager> Create(
      const base::FilePath& profile_path,
      bool in_memory_only = false);

  // Setup the cache directory for the specific FSP.
  void InitializeForProvider(const ProvidedFileSystemInfo& file_system_info,
                             FileErrorOrContentCacheCallback callback) override;

  void UninitializeForProvider(
      const ProvidedFileSystemInfo& file_system_info) override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  // Responds to the FSP with the a `ContentCache` instance if directory
  // creation was successful (or `in_memory_only` is true).
  void OnInitializeForProvider(FileErrorOrContentCacheCallback callback,
                               base::FilePath cache_directory_path,
                               base::File::Error result);

  // Called once the deletion of the cache directory has been attempted.
  void OnUninitializeForProvider(
      const base::FilePath& base64_encoded_provider_folder_name,
      base::File::Error result);

  const base::FilePath GetCacheDirectoryPath(
      const ProvidedFileSystemInfo& file_system_info);

  const base::FilePath root_content_cache_directory_;
  bool in_memory_only_ = false;
  std::set<base::FilePath> initialized_providers_;
  base::ObserverList<Observer> observers_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_{
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})};

  base::WeakPtrFactory<CacheManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_MANAGER_IMPL_H_

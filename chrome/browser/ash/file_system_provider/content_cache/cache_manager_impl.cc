// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/cache_manager_impl.h"

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/file_system_provider/content_cache/cache_manager.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_cache_impl.h"
#include "chrome/browser/ash/file_system_provider/content_cache/context_database.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"

namespace ash::file_system_provider {

namespace {

base::File::Error CreateProviderDirectory(const base::FilePath& path) {
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  if (!base::CreateDirectoryAndGetError(path, &error)) {
    return error;
  }

  return base::File::FILE_OK;
}

OptionalContextDatabase InitializeContextDatabase(
    const base::FilePath& db_path) {
  std::unique_ptr<ContextDatabase> context_db =
      std::make_unique<ContextDatabase>(db_path);
  if (context_db->Initialize()) {
    return context_db;
  }
  return std::nullopt;
}

}  // namespace

CacheManagerImpl::CacheManagerImpl(const base::FilePath& profile_path)
    : root_content_cache_directory_(
          profile_path.Append(kFspContentCacheDirName)) {}

CacheManagerImpl::~CacheManagerImpl() = default;

std::unique_ptr<CacheManager> CacheManagerImpl::Create(
    const base::FilePath& profile_path) {
  return std::make_unique<CacheManagerImpl>(profile_path);
}

void CacheManagerImpl::InitializeForProvider(
    const ProvidedFileSystemInfo& file_system_info,
    FileErrorOrContentCacheCallback callback) {
  const base::FilePath cache_directory_path =
      GetCacheDirectoryPath(file_system_info);
  if (cache_directory_path.empty()) {
    std::move(callback).Run(
        base::unexpected(base::File::FILE_ERROR_INVALID_URL));
    return;
  }

  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&CreateProviderDirectory, cache_directory_path),
      base::BindOnce(&CacheManagerImpl::OnProviderDirectoryCreationComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     cache_directory_path));
}

void CacheManagerImpl::UninitializeForProvider(
    const ProvidedFileSystemInfo& file_system_info) {
  const base::FilePath cache_directory_path =
      GetCacheDirectoryPath(file_system_info);
  if (cache_directory_path.empty()) {
    return;
  }
  const base::FilePath base64_encoded_provider_folder_name =
      cache_directory_path.BaseName();
  if (!initialized_caches_.contains(base64_encoded_provider_folder_name)) {
    OnUninitializeForProvider(base64_encoded_provider_folder_name,
                              base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  // Remove the provider from the map.
  initialized_caches_.erase(base64_encoded_provider_folder_name);

  // Attempt to delete the cache directory to ensure dead files don't remain
  // on the user's disk as the logic changes in this experimental design phase.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& cache_directory_path) {
            return base::DeletePathRecursively(cache_directory_path)
                       ? base::File::FILE_OK
                       : base::File::FILE_ERROR_FAILED;
          },
          std::move(cache_directory_path)),
      base::BindOnce(&CacheManagerImpl::OnUninitializeForProvider,
                     weak_ptr_factory_.GetWeakPtr(),
                     base64_encoded_provider_folder_name));
}

bool CacheManagerImpl::IsProviderInitialized(
    const ProvidedFileSystemInfo& file_system_info) {
  const base::FilePath cache_directory_path =
      GetCacheDirectoryPath(file_system_info);
  if (cache_directory_path.empty()) {
    return false;
  }
  const base::FilePath base64_encoded_provider_folder_name =
      cache_directory_path.BaseName();
  return initialized_caches_.contains(base64_encoded_provider_folder_name);
}

void CacheManagerImpl::AddObserver(CacheManager::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void CacheManagerImpl::RemoveObserver(CacheManager::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void CacheManagerImpl::OnProviderDirectoryCreationComplete(
    FileErrorOrContentCacheCallback callback,
    base::FilePath cache_directory_path,
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    OnProviderInitializationComplete(cache_directory_path.BaseName(),
                                     std::move(callback),
                                     base::unexpected(result));
    return;
  }

  // Initialize the database task runner, the `ContextDatabase` will be created
  // on this task runner and bound to the task runner on return.
  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  db_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&InitializeContextDatabase,
                     cache_directory_path.Append("context.db")),
      base::BindOnce(&CacheManagerImpl::OnProviderContextDatabaseSetup,
                     weak_ptr_factory_.GetWeakPtr(), cache_directory_path,
                     std::move(callback), db_task_runner));
}

void CacheManagerImpl::OnProviderContextDatabaseSetup(
    const base::FilePath& cache_directory_path,
    FileErrorOrContentCacheCallback callback,
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    OptionalContextDatabase optional_context_db) {
  const base::FilePath base64_encoded_provider_folder_name =
      cache_directory_path.BaseName();

  if (!optional_context_db.has_value()) {
    OnProviderInitializationComplete(
        base64_encoded_provider_folder_name, std::move(callback),
        base::unexpected(base::File::FILE_ERROR_FAILED));
    return;
  }

  // Bind the `ContextDatabase` to the database task runner to ensure
  // sequenced access via the `ContentCache` instance.
  BoundContextDatabase context_db(db_task_runner,
                                  std::move(optional_context_db.value()));
  std::unique_ptr<ContentCache> content_cache =
      ContentCacheImpl::Create(cache_directory_path, std::move(context_db));
  content_cache->LoadFromDisk(
      base::BindOnce(&CacheManagerImpl::OnProviderFilesLoadedFromDisk,
                     weak_ptr_factory_.GetWeakPtr(), std::move(content_cache),
                     base64_encoded_provider_folder_name, std::move(callback)));
}

void CacheManagerImpl::OnProviderFilesLoadedFromDisk(
    std::unique_ptr<ContentCache> content_cache,
    const base::FilePath& base64_encoded_provider_folder_name,
    FileErrorOrContentCacheCallback callback) {
  SpacedClient* const spaced = ash::SpacedClient::Get();
  initialized_caches_.emplace(base64_encoded_provider_folder_name,
                              content_cache->GetWeakPtr());

  GetFreeSpaceAndResizeInitializedCaches();
  // If we're not already observing spaced then start so the disk space can be
  // retrieved.
  if (!spaced_client_.IsObserving() && spaced && spaced->IsConnected()) {
    spaced_client_.Observe(spaced);
  }
  OnProviderInitializationComplete(base64_encoded_provider_folder_name,
                                   std::move(callback),
                                   std::move(content_cache));
}

void CacheManagerImpl::OnUninitializeForProvider(
    const base::FilePath& base64_encoded_provider_folder_name,
    base::File::Error result) {
  LOG_IF(ERROR, result != base::File::FILE_OK)
      << "Failed to uninitialize provider";
  // In the instance that the provider has uninitialized, the remaining caches
  // should be resized to take up the free space and if not more caches are
  // initialized after this one, stop observing the free space.
  if (initialized_caches_.size() > 0) {
    GetFreeSpaceAndResizeInitializedCaches();
  } else {
    SpacedClient* const spaced = ash::SpacedClient::Get();
    if (spaced_client_.IsObserving() && spaced && spaced->IsConnected()) {
      spaced_client_.Reset();
    }
  }
  // Notify all observers.
  for (auto& observer : observers_) {
    observer.OnProviderUninitialized(base64_encoded_provider_folder_name,
                                     result);
  }
}

const base::FilePath CacheManagerImpl::GetCacheDirectoryPath(
    const ProvidedFileSystemInfo& file_system_info) {
  const base::FilePath& provider_folder_name =
      file_system_info.mount_path().BaseName();
  if (provider_folder_name.empty()) {
    return base::FilePath();
  }
  // The provider folder name takes the form
  // {provider-id}:{file-system-id}:{user-hash} with the {file-system-id} being
  // escaped but ultimately provided by the extension, so let's convert it to
  // base64 before creating a directory.
  const base::FilePath base64_encoded_provider_folder_name(
      base::Base64Encode(provider_folder_name.value()));

  const base::FilePath cache_directory_path(
      root_content_cache_directory_.Append(
          base64_encoded_provider_folder_name));

  return cache_directory_path;
}

void CacheManagerImpl::OnProviderInitializationComplete(
    const base::FilePath& base64_encoded_provider_folder_name,
    FileErrorOrContentCacheCallback callback,
    FileErrorOrContentCache error_or_content_cache) {
  base::File::Error result =
      error_or_content_cache.error_or(base::File::FILE_OK);
  std::move(callback).Run(std::move(error_or_content_cache));

  for (CacheManager::Observer& observer : observers_) {
    observer.OnProviderInitializationComplete(
        base64_encoded_provider_folder_name, result);
  }
}

void CacheManagerImpl::GetFreeSpaceAndResizeInitializedCaches() {
  ash::SpacedClient::Get()->GetFreeDiskSpace(
      root_content_cache_directory_.value(),
      base::BindOnce(
          [](base::WeakPtr<CacheManagerImpl> weak_ptr,
             std::optional<int64_t> free_space_in_bytes) {
            LOG_IF(ERROR, !free_space_in_bytes.has_value())
                << "Failed getting free space";
            if (weak_ptr && free_space_in_bytes.has_value()) {
              weak_ptr->OnFreeSpace(free_space_in_bytes.value());
            }
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void CacheManagerImpl::OnFreeSpace(int64_t free_space_in_bytes) {
  if (initialized_caches_.empty()) {
    return;
  }
  VLOG(1) << "Free space: " << free_space_in_bytes;

  // TODO(b/339540246): The logic below currently just divides the space up by
  // the number of initialized caches. For now, as ODFS is the only expected
  // content cache, this is ok. However, this should be smarter and allow for
  // variable size caches.
  int64_t total_bytes_of_all_caches = 0;
  for (auto it = initialized_caches_.begin(); it != initialized_caches_.end();
       ++it) {
    base::WeakPtr<ContentCache> cache_weak_ptr = it->second;
    if (!cache_weak_ptr) {
      initialized_caches_.erase(it);
      continue;
    }
    total_bytes_of_all_caches += cache_weak_ptr->GetSize().total_bytes_on_disk;
  }

  // The max size of all caches should not exceed the total size of (free space
  // + used space of all caches) * 0.6.
  int64_t max_size_of_all_caches_in_bytes =
      (total_bytes_of_all_caches + free_space_in_bytes) * 0.6;

  // The max size of each individual cache should never go past 5GB either.
  int64_t max_bytes_per_cache =
      std::min(static_cast<int64_t>(max_size_of_all_caches_in_bytes) /
                   static_cast<int64_t>(initialized_caches_.size()),
               kMaxAllowedCacheSizeInBytes);

  for (const auto& [base64_encoded_provider_folder_name, content_cache] :
       initialized_caches_) {
    if (!content_cache) {
      continue;
    }
    int64_t old_max_size = content_cache->GetSize().max_bytes_on_disk;
    if (old_max_size == max_bytes_per_cache) {
      VLOG(2) << "No cache resize needed: "
              << base64_encoded_provider_folder_name;
      VLOG(2) << " > Old max bytes: " << old_max_size;
      VLOG(2) << " > New max bytes: " << max_bytes_per_cache;
      continue;
    }

    // TODO(b/330602540): Move these logs into the `ContentCache` once
    // eviction has been implemented.
    VLOG(1) << "Potential cache resize event: "
            << base64_encoded_provider_folder_name;
    VLOG(1) << " > Old max bytes: " << old_max_size;
    VLOG(1) << " > New max bytes: " << max_bytes_per_cache;
    content_cache->SetMaxBytesOnDisk(max_bytes_per_cache);
  }
}

void CacheManagerImpl::OnSpaceUpdate(const SpaceEvent& event) {
  if (event.free_space_bytes() < 0) {
    LOG(ERROR) << "Free space is negative";
    return;
  }
  OnFreeSpace(event.free_space_bytes());
}

}  // namespace ash::file_system_provider

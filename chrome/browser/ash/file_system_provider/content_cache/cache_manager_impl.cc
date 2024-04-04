// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/cache_manager_impl.h"

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_cache_impl.h"
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

}  // namespace

CacheManagerImpl::CacheManagerImpl(const base::FilePath& profile_path,
                                   bool in_memory_only)
    : root_content_cache_directory_(
          profile_path.Append(kFspContentCacheDirName)),
      in_memory_only_(in_memory_only) {}

CacheManagerImpl::~CacheManagerImpl() = default;

std::unique_ptr<CacheManager> CacheManagerImpl::Create(
    const base::FilePath& profile_path,
    bool in_memory_only) {
  return std::make_unique<CacheManagerImpl>(profile_path, in_memory_only);
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

  if (in_memory_only_) {
    OnInitializeForProvider(std::move(callback), cache_directory_path,
                            base::File::FILE_OK);
    return;
  }

  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&CreateProviderDirectory, cache_directory_path),
      base::BindOnce(&CacheManagerImpl::OnInitializeForProvider,
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
  if (!initialized_providers_.contains(base64_encoded_provider_folder_name)) {
    OnUninitializeForProvider(base64_encoded_provider_folder_name,
                              base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  // Remove the provider from the set.
  initialized_providers_.erase(base64_encoded_provider_folder_name);

  if (in_memory_only_) {
    OnUninitializeForProvider(base64_encoded_provider_folder_name,
                              base::File::FILE_OK);
    return;
  }

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
  return initialized_providers_.contains(base64_encoded_provider_folder_name);
}

void CacheManagerImpl::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void CacheManagerImpl::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void CacheManagerImpl::OnInitializeForProvider(
    FileErrorOrContentCacheCallback callback,
    base::FilePath cache_directory_path,
    base::File::Error result) {
  const base::FilePath base64_encoded_provider_folder_name =
      cache_directory_path.BaseName();
  if (result != base::File::FILE_OK) {
    std::move(callback).Run(base::unexpected(result));
  } else {
    initialized_providers_.emplace(base64_encoded_provider_folder_name);
    std::move(callback).Run(ContentCacheImpl::Create(cache_directory_path));
  }

  // Notify all observers.
  for (auto& observer : observers_) {
    observer.OnProviderInitializationComplete(
        base64_encoded_provider_folder_name, result);
  }
}

void CacheManagerImpl::OnUninitializeForProvider(
    const base::FilePath& base64_encoded_provider_folder_name,
    base::File::Error result) {
  LOG_IF(ERROR, result != base::File::FILE_OK)
      << "Failed to uninitialize provider";
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

}  // namespace ash::file_system_provider

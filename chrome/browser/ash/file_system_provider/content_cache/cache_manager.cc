// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/cache_manager.h"

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/types/expected.h"

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

CacheManager::CacheManager(const base::FilePath& profile_path,
                           bool in_memory_only)
    : root_content_cache_directory_(
          profile_path.Append(kFspContentCacheDirName)),
      in_memory_only_(in_memory_only) {}

CacheManager::~CacheManager() = default;

void CacheManager::InitializeForProvider(
    const base::FilePath& provider_folder_name,
    FileErrorOrContentCacheCallback callback) {
  if (provider_folder_name.empty()) {
    std::move(callback).Run(
        base::unexpected(base::File::FILE_ERROR_INVALID_URL));
    return;
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

  if (in_memory_only_) {
    OnInitializeForProvider(std::move(callback), cache_directory_path,
                            base::File::FILE_OK);
    return;
  }

  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&CreateProviderDirectory, cache_directory_path),
      base::BindOnce(&CacheManager::OnInitializeForProvider,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     cache_directory_path));
}

void CacheManager::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void CacheManager::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void CacheManager::OnInitializeForProvider(
    FileErrorOrContentCacheCallback callback,
    base::FilePath cache_directory_path,
    base::File::Error result) {
  const base::FilePath base64_encoded_provider_folder_name =
      cache_directory_path.BaseName();
  if (result != base::File::FILE_OK) {
    std::move(callback).Run(base::unexpected(result));
  } else {
    initialized_providers_.emplace(base64_encoded_provider_folder_name);
    std::move(callback).Run(
        std::make_unique<ContentCache>(cache_directory_path));
  }

  // Notify all observers once the ContentCache is initialisation is complete.
  for (auto& observer : observers_) {
    observer.OnContentCacheInitializeComplete(
        base64_encoded_provider_folder_name, result);
  }
}

}  // namespace ash::file_system_provider

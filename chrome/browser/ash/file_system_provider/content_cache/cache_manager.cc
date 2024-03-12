// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/cache_manager.h"

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
    : profile_path_(profile_path), in_memory_only_(in_memory_only) {}

CacheManager::~CacheManager() = default;

void CacheManager::InitializeForProvider(
    const base::FilePath& provider_mount_path,
    FileErrorOrContentCacheCallback callback) {
  if (provider_mount_path.empty()) {
    std::move(callback).Run(
        base::unexpected(base::File::FILE_ERROR_INVALID_URL));
    return;
  }

  if (in_memory_only_) {
    OnInitializeForProvider(std::move(callback), provider_mount_path,
                            base::File::FILE_OK);
    return;
  }

  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CreateProviderDirectory,
                     profile_path_.Append(kFspContentCacheDirName)
                         .Append(provider_mount_path)),
      base::BindOnce(&CacheManager::OnInitializeForProvider,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     provider_mount_path));
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
    base::FilePath mount_path,
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    std::move(callback).Run(base::unexpected(result));
  } else {
    initialized_providers_.emplace(mount_path);
    std::move(callback).Run(std::make_unique<ContentCache>(
        profile_path_.Append(kFspContentCacheDirName).Append(mount_path)));
  }

  // Notify all observers once the ContentCache is initialisation is complete.
  for (auto& observer : observers_) {
    observer.OnContentCacheInitializeComplete(mount_path, result);
  }
}

}  // namespace ash::file_system_provider

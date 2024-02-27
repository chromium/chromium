// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/cache_manager.h"

#include "base/files/file_util.h"

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

CacheManager::CacheManager(const base::FilePath& profile_path)
    : profile_path_(profile_path) {}

CacheManager::~CacheManager() = default;

void CacheManager::InitializeForProvider(
    const base::FilePath& provider_mount_path,
    FileErrorCallback callback) {
  if (provider_mount_path.empty()) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_URL);
    return;
  }

  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CreateProviderDirectory,
                     profile_path_.Append(kFspContentCacheDirName)
                         .Append(provider_mount_path)),
      base::BindOnce(&CacheManager::OnInitialized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     provider_mount_path));
}

void CacheManager::OnInitialized(FileErrorCallback callback,
                                 base::FilePath mount_path,
                                 base::File::Error result) {
  if (result == base::File::FILE_OK) {
    initialized_providers_.emplace(mount_path);
  }
  std::move(callback).Run(result);
}

}  // namespace ash::file_system_provider

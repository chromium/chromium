// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/file_manager_ash.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"

namespace crosapi {
namespace {

// Adapts a platform_util::OpenOperationResult to a crosapi::mojom::OpenResult
// when running a |callback|.
void RunWithOpenResult(base::OnceCallback<void(mojom::OpenResult)> callback,
                       platform_util::OpenOperationResult result) {
  mojom::OpenResult mojo_result;
  switch (result) {
    case platform_util::OPEN_SUCCEEDED:
      mojo_result = mojom::OpenResult::kSucceeded;
      break;
    case platform_util::OPEN_FAILED_PATH_NOT_FOUND:
      mojo_result = mojom::OpenResult::kFailedPathNotFound;
      break;
    case platform_util::OPEN_FAILED_INVALID_TYPE:
      mojo_result = mojom::OpenResult::kFailedInvalidType;
      break;
    case platform_util::OPEN_FAILED_NO_HANLDER_FOR_FILE_TYPE:
      mojo_result = mojom::OpenResult::kFailedNoHandlerForFileType;
      break;
    case platform_util::OPEN_FAILED_FILE_ERROR:
      mojo_result = mojom::OpenResult::kFailedFileError;
      break;
  }
  std::move(callback).Run(mojo_result);
}

// Opens an item of |type| at |path| and runs |callback| with the result.
void OpenItem(const base::FilePath& path,
              platform_util::OpenItemType item_type,
              base::OnceCallback<void(mojom::OpenResult)> callback) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  file_manager::util::OpenItem(
      primary_profile, path, item_type,
      base::BindOnce(&RunWithOpenResult, std::move(callback)));
}

}  // namespace

FileManagerAsh::FileManagerAsh() = default;

FileManagerAsh::~FileManagerAsh() = default;

void FileManagerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::FileManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FileManagerAsh::DeprecatedShowItemInFolder(const base::FilePath& path) {
  // As of OS M89 all lacros clients now use ShowItemInFolder() below.
  NOTIMPLEMENTED();
}

void FileManagerAsh::ShowItemInFolder(const base::FilePath& path,
                                      ShowItemInFolderCallback callback) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  file_manager::util::ShowItemInFolder(
      primary_profile, path,
      base::BindOnce(&RunWithOpenResult, std::move(callback)));
}

void FileManagerAsh::OpenFolder(const base::FilePath& path,
                                OpenFolderCallback callback) {
  OpenItem(path, platform_util::OPEN_FOLDER, std::move(callback));
}

void FileManagerAsh::OpenFile(const base::FilePath& path,
                              OpenFileCallback callback) {
  OpenItem(path, platform_util::OPEN_FILE, std::move(callback));
}

}  // namespace crosapi

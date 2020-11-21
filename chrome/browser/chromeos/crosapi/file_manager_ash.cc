// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/file_manager_ash.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/file_manager/open_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"

namespace crosapi {
namespace {

// Lacros does not support multi-signin. Lacros uses /home/chronos/user as the
// base for all system-level directories but the file manager expects the raw
// profile path with the /home/chronos/u-{hash} prefix. Clean up the path.
// TODO(https://crbug.com/1150702): Delete this function after all Lacros
// clients are on M89. Lacros is switching to use the raw profile path.
base::FilePath ExpandPath(Profile* primary_profile,
                          const base::FilePath& path) {
  return file_manager::util::ReplacePathPrefix(
      path, base::FilePath(crosapi::kHomeChronosUserPath),
      primary_profile->GetPath());
}

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
  base::FilePath full_path = ExpandPath(primary_profile, path);
  file_manager::util::OpenItem(
      primary_profile, full_path, item_type,
      base::BindOnce(&RunWithOpenResult, std::move(callback)));
}

}  // namespace

FileManagerAsh::FileManagerAsh(
    mojo::PendingReceiver<mojom::FileManager> receiver)
    : receiver_(this, std::move(receiver)) {}

FileManagerAsh::~FileManagerAsh() = default;

void FileManagerAsh::DeprecatedShowItemInFolder(const base::FilePath& path) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  base::FilePath final_path = ExpandPath(primary_profile, path);
  // NOTE: This will show error dialogs for files and paths that cannot be
  // opened, but the dialogs are modal to an ash-chrome window, not the
  // lacros-chrome window that opened the file. That's why this version is
  // deprecated.
  platform_util::ShowItemInFolder(primary_profile, final_path);
}

void FileManagerAsh::ShowItemInFolder(const base::FilePath& path,
                                      ShowItemInFolderCallback callback) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  base::FilePath full_path = ExpandPath(primary_profile, path);
  file_manager::util::ShowItemInFolder(
      primary_profile, full_path,
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

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/file_manager_ash.h"

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"

namespace crosapi {

FileManagerAsh::FileManagerAsh(
    mojo::PendingReceiver<mojom::FileManager> receiver)
    : receiver_(this, std::move(receiver)) {}

FileManagerAsh::~FileManagerAsh() = default;

void FileManagerAsh::ShowItemInFolder(const base::FilePath& path) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  // Lacros does not support multi-signin. Lacros uses /home/chronos/user as the
  // base for all system-level directories but the file manager expects the raw
  // profile path with the /home/chronos/u-{hash} prefix. Clean up the path.
  base::FilePath final_path = file_manager::util::ReplacePathPrefix(
      path, base::FilePath(crosapi::kHomeChronosUserPath),
      primary_profile->GetPath());
  // Use platform_util instead of calling directly into file manager code
  // because platform_util_ash.cc handles showing error dialogs for files and
  // paths that cannot be opened.
  platform_util::ShowItemInFolder(primary_profile, final_path);
}

}  // namespace crosapi

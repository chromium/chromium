// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/file_manager_ash.h"

#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace crosapi {

FileManagerAsh::FileManagerAsh(
    mojo::PendingReceiver<mojom::FileManager> receiver)
    : receiver_(this, std::move(receiver)) {}

FileManagerAsh::~FileManagerAsh() = default;

void FileManagerAsh::ShowItemInFolder(const base::FilePath& path) {
  // Use platform_util instead of calling directly into file manager code
  // because platform_util_ash.cc handles showing error dialogs for files and
  // paths that cannot be opened.
  platform_util::ShowItemInFolder(ProfileManager::GetActiveUserProfile(), path);
}

}  // namespace crosapi

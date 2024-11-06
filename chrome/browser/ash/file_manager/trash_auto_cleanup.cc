// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/trash_auto_cleanup.h"

#include "base/system/sys_info.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"

namespace file_manager::trash {

TrashAutoCleanup::TrashAutoCleanup(Profile* profile) : profile_(profile) {}

std::unique_ptr<TrashAutoCleanup> TrashAutoCleanup::Create(Profile* profile) {
  // Only run the auto cleanup process for regular profiles on ChromeOS.
  if (!file_manager::trash::IsTrashEnabledForProfile(profile) || !profile ||
      !profile->IsRegularProfile() || !base::SysInfo::IsRunningOnChromeOS()) {
    return nullptr;
  }

  auto instance = base::WrapUnique(new TrashAutoCleanup(profile));
  instance->Init();
  return instance;
}

void TrashAutoCleanup::Init() {
  // TODO: Start the cleanup process.
}

}  // namespace file_manager::trash

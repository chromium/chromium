// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/test_sea_pen_wallpaper_manager_session_delegate.h"

#include <utility>

#include "ash/wallpaper/sea_pen_wallpaper_manager.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TestSeaPenWallpaperManagerSessionDelegate::
    TestSeaPenWallpaperManagerSessionDelegate() = default;

TestSeaPenWallpaperManagerSessionDelegate::
    ~TestSeaPenWallpaperManagerSessionDelegate() = default;

base::FilePath TestSeaPenWallpaperManagerSessionDelegate::GetStorageDirectory(
    const AccountId& account_id) {
  if (!scoped_temp_dir_.IsValid()) {
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }
  // Public account and guest users do not have an account id key.
  const std::string account_identifier = account_id.HasAccountIdKey()
                                             ? account_id.GetAccountIdKey()
                                             : account_id.GetUserEmail();
  EXPECT_NE(std::string(), account_identifier);
  return scoped_temp_dir_.GetPath()
      .Append(account_identifier)
      .Append("wallpaper")
      .Append(wallpaper_constants::kSeaPenWallpaperDirName);
}

PrefService* TestSeaPenWallpaperManagerSessionDelegate::GetPrefService(
    const AccountId& account_id) {
  auto [it, was_inserted] = pref_services_.try_emplace(account_id);
  if (was_inserted) {
    SeaPenWallpaperManager::RegisterProfilePrefs(it->second.registry());
  }
  return &it->second;
}

}  // namespace ash

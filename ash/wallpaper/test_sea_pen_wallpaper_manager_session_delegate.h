// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_TEST_SEA_PEN_WALLPAPER_MANAGER_SESSION_DELEGATE_H_
#define ASH_WALLPAPER_TEST_SEA_PEN_WALLPAPER_MANAGER_SESSION_DELEGATE_H_

#include <map>

#include "ash/wallpaper/sea_pen_wallpaper_manager.h"
#include "base/files/scoped_temp_dir.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"

namespace ash {

class TestSeaPenWallpaperManagerSessionDelegate
    : public SeaPenWallpaperManager::SessionDelegate {
 public:
  TestSeaPenWallpaperManagerSessionDelegate();

  TestSeaPenWallpaperManagerSessionDelegate(
      const TestSeaPenWallpaperManagerSessionDelegate&) = delete;
  TestSeaPenWallpaperManagerSessionDelegate& operator=(
      const TestSeaPenWallpaperManagerSessionDelegate&) = delete;

  ~TestSeaPenWallpaperManagerSessionDelegate() override;

  // SeaPenWallpaperManager::SessionDelegate:
  base::FilePath GetStorageDirectory(const AccountId& account_id) override;
  PrefService* GetPrefService(const AccountId& account_id) override;

 private:
  base::ScopedTempDir scoped_temp_dir_;
  std::map<AccountId, TestingPrefServiceSimple> pref_services_;
};

}  // namespace ash

#endif  // ASH_WALLPAPER_TEST_SEA_PEN_WALLPAPER_MANAGER_SESSION_DELEGATE_H_

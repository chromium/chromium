// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_SEA_PEN_WALLPAPER_MANAGER_SESSION_DELEGATE_IMPL_H_
#define ASH_WALLPAPER_SEA_PEN_WALLPAPER_MANAGER_SESSION_DELEGATE_IMPL_H_

#include "ash/wallpaper/sea_pen_wallpaper_manager.h"

class AccountId;
class PrefService;

namespace ash {

class SeaPenWallpaperManagerSessionDelegateImpl
    : public SeaPenWallpaperManager::SessionDelegate {
 public:
  SeaPenWallpaperManagerSessionDelegateImpl();

  SeaPenWallpaperManagerSessionDelegateImpl(
      const SeaPenWallpaperManagerSessionDelegateImpl&) = delete;
  SeaPenWallpaperManagerSessionDelegateImpl& operator=(
      const SeaPenWallpaperManagerSessionDelegateImpl&) = delete;

  ~SeaPenWallpaperManagerSessionDelegateImpl() override;

  // SeaPenWallpaperManager::SessionDelegate:
  base::FilePath GetStorageDirectory(const AccountId& account_id) override;
  PrefService* GetPrefService(const AccountId& account_id) override;
};

}  // namespace ash

#endif  // ASH_WALLPAPER_SEA_PEN_WALLPAPER_MANAGER_SESSION_DELEGATE_IMPL_H_

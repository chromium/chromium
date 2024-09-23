// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/sea_pen_wallpaper_manager_session_delegate_impl.h"

#include <string_view>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "base/files/file_path.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

constexpr std::string_view kWallpaperDirName = "wallpaper";

}  // namespace

SeaPenWallpaperManagerSessionDelegateImpl::
    SeaPenWallpaperManagerSessionDelegateImpl() = default;

SeaPenWallpaperManagerSessionDelegateImpl::
    ~SeaPenWallpaperManagerSessionDelegateImpl() = default;

base::FilePath SeaPenWallpaperManagerSessionDelegateImpl::GetStorageDirectory(
    const AccountId& account_id) {
  const auto profile_path =
      Shell::Get()->session_controller()->GetProfilePath(account_id);
  if (profile_path.empty()) {
    LOG(WARNING) << "Empty profile path.";
    return base::FilePath();
  }
  return profile_path.Append(kWallpaperDirName)
      .Append(wallpaper_constants::kSeaPenWallpaperDirName);
}

PrefService* SeaPenWallpaperManagerSessionDelegateImpl::GetPrefService(
    const AccountId& account_id) {
  auto* pref_service =
      Shell::Get()->session_controller()->GetUserPrefServiceForUser(account_id);
  DCHECK(pref_service);
  return pref_service;
}

}  // namespace ash

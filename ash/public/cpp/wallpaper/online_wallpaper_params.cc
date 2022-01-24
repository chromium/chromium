// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"

#include <cstdint>
#include <string>

#include "components/account_id/account_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {

OnlineWallpaperParams::OnlineWallpaperParams(
    const AccountId& in_account_id,
    const absl::optional<uint64_t>& in_asset_id,
    const GURL& in_url,
    const std::string& in_collection_id,
    WallpaperLayout in_layout,
    bool in_preview_mode,
    bool in_from_user,
    bool in_daily_refresh_enabled)
    : account_id(in_account_id),
      asset_id(in_asset_id),
      url(in_url),
      collection_id(in_collection_id),
      layout(in_layout),
      preview_mode(in_preview_mode),
      from_user(in_from_user),
      daily_refresh_enabled(in_daily_refresh_enabled) {}

OnlineWallpaperParams::OnlineWallpaperParams(
    const OnlineWallpaperParams& other) = default;

OnlineWallpaperParams::OnlineWallpaperParams(OnlineWallpaperParams&& other) =
    default;

OnlineWallpaperParams& OnlineWallpaperParams::operator=(
    const OnlineWallpaperParams& other) = default;

OnlineWallpaperParams::~OnlineWallpaperParams() = default;

}  // namespace ash

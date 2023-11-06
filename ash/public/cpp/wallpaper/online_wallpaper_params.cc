// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"

#include <cstdint>
#include <string>

#include "components/account_id/account_id.h"
#include "url/gurl.h"

namespace ash {

OnlineWallpaperParams::OnlineWallpaperParams(
    const AccountId& in_account_id,
    const std::string& in_collection_id,
    WallpaperLayout in_layout,
    bool in_preview_mode,
    bool in_from_user,
    bool in_daily_refresh_enabled,
    uint64_t in_unit_id,
    const std::vector<OnlineWallpaperVariant>& in_variants)
    : account_id(in_account_id),
      collection_id(in_collection_id),
      layout(in_layout),
      preview_mode(in_preview_mode),
      from_user(in_from_user),
      daily_refresh_enabled(in_daily_refresh_enabled),
      unit_id(in_unit_id),
      variants(in_variants) {}

OnlineWallpaperParams::OnlineWallpaperParams(
    const OnlineWallpaperParams& other) = default;

OnlineWallpaperParams::OnlineWallpaperParams(OnlineWallpaperParams&& other) =
    default;

OnlineWallpaperParams& OnlineWallpaperParams::operator=(
    const OnlineWallpaperParams& other) = default;

OnlineWallpaperParams::~OnlineWallpaperParams() = default;

std::ostream& operator<<(std::ostream& os,
                         const OnlineWallpaperParams& params) {
  os << "OnlineWallpaperParams:" << std::endl
     << "  account_id: " << params.account_id << std::endl
     << "  collection_id: " << params.collection_id << std::endl
     << "  layout: " << params.layout << std::endl
     << "  preview_mode: " << params.preview_mode << std::endl
     << "  from_user: " << params.from_user << std::endl
     << "  daily_refresh_enabled: " << params.daily_refresh_enabled << std::endl
     << "  unit_id: " << params.unit_id << std::endl
     << "  variants_size: " << params.variants.size() << std::endl;
  return os;
}

}  // namespace ash

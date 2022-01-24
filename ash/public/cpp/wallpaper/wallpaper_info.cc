// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper/wallpaper_info.h"

#include <iostream>

#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"

namespace ash {

WallpaperInfo::WallpaperInfo() {
  layout = WALLPAPER_LAYOUT_CENTER;
  type = WallpaperType::kCount;
}

WallpaperInfo::WallpaperInfo(
    const OnlineWallpaperParams& online_wallpaper_params)
    : WallpaperInfo(online_wallpaper_params.url.spec(),
                    online_wallpaper_params.asset_id,
                    online_wallpaper_params.collection_id,
                    online_wallpaper_params.layout,
                    WallpaperType::kOnline,
                    base::Time::Now()) {}

WallpaperInfo::WallpaperInfo(const std::string& in_location,
                             const absl::optional<uint64_t>& in_asset_id,
                             const std::string& in_collection_id,
                             WallpaperLayout in_layout,
                             WallpaperType in_type,
                             const base::Time& in_date)
    : location(in_location),
      asset_id(in_asset_id),
      collection_id(in_collection_id),
      layout(in_layout),
      type(in_type),
      date(in_date) {}

WallpaperInfo::WallpaperInfo(const std::string& in_location,
                             WallpaperLayout in_layout,
                             WallpaperType in_type,
                             const base::Time& in_date)
    : location(in_location), layout(in_layout), type(in_type), date(in_date) {}

WallpaperInfo::WallpaperInfo(const WallpaperInfo& other) = default;
WallpaperInfo& WallpaperInfo::operator=(const WallpaperInfo& other) = default;

WallpaperInfo::WallpaperInfo(WallpaperInfo&& other) = default;
WallpaperInfo& WallpaperInfo::operator=(WallpaperInfo&& other) = default;

bool WallpaperInfo::operator==(const WallpaperInfo& other) const {
  return (location == other.location) && (asset_id == other.asset_id) &&
         (collection_id == other.collection_id) && (layout == other.layout) &&
         (type == other.type);
}

bool WallpaperInfo::operator!=(const WallpaperInfo& other) const {
  return !(*this == other);
}

WallpaperInfo::~WallpaperInfo() = default;

std::ostream& operator<<(std::ostream& os, const WallpaperInfo& info) {
  os << "WallpaperInfo:" << std::endl;
  os << "  location: " << info.location << std::endl;
  os << "  asset_id: " << info.asset_id.value_or(-1) << std::endl;
  os << "  collection_id: " << info.collection_id << std::endl;
  os << "  layout: " << info.layout << std::endl;
  os << "  type: " << static_cast<int>(info.type) << std::endl;
  os << "  date: " << info.date << std::endl;
  return os;
}

}  // namespace ash

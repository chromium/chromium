// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper/wallpaper_info.h"

#include <iostream>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/version.h"

namespace ash {

namespace {

// Populates online wallpaper related info in `info`.
void PopulateOnlineWallpaperInfo(WallpaperInfo* info,
                                 const base::Value::Dict& info_dict) {
  const std::string* asset_id_str =
      info_dict.FindString(WallpaperInfo::kNewWallpaperAssetIdNodeName);
  const std::string* collection_id =
      info_dict.FindString(WallpaperInfo::kNewWallpaperCollectionIdNodeName);
  const std::string* dedup_key =
      info_dict.FindString(WallpaperInfo::kNewWallpaperDedupKeyNodeName);
  const std::string* unit_id_str =
      info_dict.FindString(WallpaperInfo::kNewWallpaperUnitIdNodeName);
  const base::Value::List* variant_list =
      info_dict.FindList(WallpaperInfo::kNewWallpaperVariantListNodeName);

  info->collection_id = collection_id ? *collection_id : std::string();
  info->dedup_key = dedup_key ? std::make_optional(*dedup_key) : std::nullopt;

  if (asset_id_str) {
    uint64_t asset_id;
    if (base::StringToUint64(*asset_id_str, &asset_id)) {
      info->asset_id = std::make_optional(asset_id);
    }
  }
  if (unit_id_str) {
    uint64_t unit_id;
    if (base::StringToUint64(*unit_id_str, &unit_id)) {
      info->unit_id = std::make_optional(unit_id);
    }
  }
  if (variant_list) {
    std::vector<OnlineWallpaperVariant> variants;
    for (const auto& variant_info_value : *variant_list) {
      if (!variant_info_value.is_dict()) {
        continue;
      }
      const base::Value::Dict& variant_info = variant_info_value.GetDict();
      const std::string* variant_asset_id_str =
          variant_info.FindString(WallpaperInfo::kNewWallpaperAssetIdNodeName);
      const std::string* url =
          variant_info.FindString(WallpaperInfo::kOnlineWallpaperUrlNodeName);
      std::optional<int> type =
          variant_info.FindInt(WallpaperInfo::kOnlineWallpaperTypeNodeName);
      if (variant_asset_id_str && url && type.has_value()) {
        uint64_t variant_asset_id;
        if (base::StringToUint64(*variant_asset_id_str, &variant_asset_id)) {
          variants.emplace_back(
              variant_asset_id, GURL(*url),
              static_cast<backdrop::Image::ImageType>(type.value()));
        }
      }
    }
    info->variants = std::move(variants);
  }
}

}  // namespace

WallpaperInfo::WallpaperInfo() {
  layout = WALLPAPER_LAYOUT_CENTER;
  type = WallpaperType::kCount;
}

WallpaperInfo::WallpaperInfo(
    const OnlineWallpaperParams& online_wallpaper_params,
    const OnlineWallpaperVariant& target_variant)
    : location(target_variant.raw_url.spec()),
      layout(online_wallpaper_params.layout),
      type(online_wallpaper_params.daily_refresh_enabled
               ? WallpaperType::kDaily
               : WallpaperType::kOnline),
      date(base::Time::Now()),
      collection_id(online_wallpaper_params.collection_id),
      unit_id(online_wallpaper_params.unit_id),
      variants(online_wallpaper_params.variants) {
  if (features::IsVersionWallpaperInfoEnabled()) {
    version = GetSupportedVersion(type);
  } else {
    asset_id = target_variant.asset_id;
  }
}

WallpaperInfo::WallpaperInfo(
    const GooglePhotosWallpaperParams& google_photos_wallpaper_params)
    : layout(google_photos_wallpaper_params.layout), date(base::Time::Now()) {
  if (google_photos_wallpaper_params.daily_refresh_enabled) {
    type = WallpaperType::kDailyGooglePhotos;
    collection_id = google_photos_wallpaper_params.id;
  } else {
    type = WallpaperType::kOnceGooglePhotos;
    location = google_photos_wallpaper_params.id;
    dedup_key = google_photos_wallpaper_params.dedup_key;
  }
  if (features::IsVersionWallpaperInfoEnabled()) {
    version = GetSupportedVersion(type);
  }
}

WallpaperInfo::WallpaperInfo(const std::string& in_location,
                             WallpaperLayout in_layout,
                             WallpaperType in_type,
                             const base::Time& in_date,
                             const std::string& in_user_file_path)
    : location(in_location),
      user_file_path(in_user_file_path),
      layout(in_layout),
      type(in_type),
      date(in_date) {
  if (features::IsVersionWallpaperInfoEnabled()) {
    version = GetSupportedVersion(type);
  }
}

WallpaperInfo::WallpaperInfo(const WallpaperInfo& other) = default;
WallpaperInfo& WallpaperInfo::operator=(const WallpaperInfo& other) = default;

WallpaperInfo::WallpaperInfo(WallpaperInfo&& other) = default;
WallpaperInfo& WallpaperInfo::operator=(WallpaperInfo&& other) = default;

bool WallpaperInfo::MatchesSelection(const WallpaperInfo& other) const {
  if (features::IsVersionWallpaperInfoEnabled()) {
    // Checks for exact match of the version here to avoid unexpected data
    // mismatch between two WallpaperInfos. Any difference in version should
    // surface to the callers of this function so they can decide how to handle
    // it.
    if (!version.IsValid() || !other.version.IsValid() ||
        version != other.version) {
      return false;
    }
  }
  // |location| are skipped on purpose in favor of |unit_id| as
  // online wallpapers can vary across devices due to their color mode. Other
  // wallpaper types still require location to be equal.
  switch (type) {
    case WallpaperType::kOnline:
    case WallpaperType::kDaily:
      return type == other.type && layout == other.layout &&
             collection_id == other.collection_id && unit_id == other.unit_id &&
             base::ranges::equal(variants, other.variants);
    case WallpaperType::kOnceGooglePhotos:
    case WallpaperType::kDailyGooglePhotos:
      return location == other.location && layout == other.layout &&
             collection_id == other.collection_id;
    case WallpaperType::kCustomized:
      // |location| is skipped for customized wallpaper as it includes files id
      // which is different between devices even it refers to the same file.
      // Comparing |user_file_path| that contains the absolute path should be
      // enough.
      return type == other.type && layout == other.layout &&
             user_file_path == other.user_file_path;
    case WallpaperType::kSeaPen:
    case WallpaperType::kDefault:
    case WallpaperType::kPolicy:
    case WallpaperType::kThirdParty:
    case WallpaperType::kDevice:
    case WallpaperType::kOneShot:
    case WallpaperType::kOobe:
    case WallpaperType::kCount:
      return type == other.type && layout == other.layout &&
             location == other.location;
  }
}

bool WallpaperInfo::MatchesAsset(const WallpaperInfo& other) const {
  if (!MatchesSelection(other))
    return false;

  switch (type) {
    case WallpaperType::kOnline:
    case WallpaperType::kDaily:
      return location == other.location;
    case WallpaperType::kOnceGooglePhotos:
    case WallpaperType::kDailyGooglePhotos:
    case WallpaperType::kCustomized:
    case WallpaperType::kDefault:
    case WallpaperType::kPolicy:
    case WallpaperType::kThirdParty:
    case WallpaperType::kDevice:
    case WallpaperType::kOneShot:
    case WallpaperType::kOobe:
    case WallpaperType::kSeaPen:
    case WallpaperType::kCount:
      return true;
  }
}

// static
std::optional<WallpaperInfo> WallpaperInfo::FromDict(
    const base::Value::Dict& dict) {
  const std::string* location =
      dict.FindString(WallpaperInfo::kNewWallpaperLocationNodeName);
  const std::string* file_path =
      dict.FindString(WallpaperInfo::kNewWallpaperUserFilePathNodeName);
  std::optional<int> layout =
      dict.FindInt(WallpaperInfo::kNewWallpaperLayoutNodeName);
  std::optional<int> type =
      dict.FindInt(WallpaperInfo::kNewWallpaperTypeNodeName);
  const std::string* date_string =
      dict.FindString(WallpaperInfo::kNewWallpaperDateNodeName);

  if (!location || !layout || !type || !date_string) {
    return std::nullopt;
  }

  // Perform special handling of pref values >= kCount before hitting the DCHECK
  // below. This can happen in normal operation when syncing from a newer
  // release to an older one, so should not DCHECK.
  if (type.value() >= base::to_underlying(WallpaperType::kCount)) {
    LOG(WARNING) << "Skipping wallpaper sync due to unrecognized WallpaperType="
                 << type.value()
                 << ". This likely happened due to sync from a newer version "
                    "of ChromeOS.";
    return std::nullopt;
  }

  WallpaperType wallpaper_type = static_cast<WallpaperType>(type.value());
  DCHECK(IsAllowedInPrefs(wallpaper_type))
      << "Invalid WallpaperType=" << base::to_underlying(wallpaper_type)
      << " in prefs";

  WallpaperInfo info;
  info.type = wallpaper_type;

  const std::string* version =
      dict.FindString(WallpaperInfo::kNewWallpaperVersionNodeName);
  if (version) {
    info.version = base::Version(*version);
  }

  int64_t date_val;
  if (!base::StringToInt64(*date_string, &date_val)) {
    return std::nullopt;
  }

  info.location = *location;
  // The old wallpaper didn't include file path information. For migration,
  // check whether file_path is a null pointer before setting user_file_path.
  info.user_file_path = file_path ? *file_path : "";
  info.layout = static_cast<WallpaperLayout>(layout.value());
  if (info.layout >= WallpaperLayout::NUM_WALLPAPER_LAYOUT) {
    LOG(WARNING) << "Invalid WallpaperLayout=" << info.layout << " in prefs";
    return std::nullopt;
  }
  // TODO(skau): Switch to TimeFromValue
  info.date =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(date_val));
  PopulateOnlineWallpaperInfo(&info, dict);
  return info;
}

base::Value::Dict WallpaperInfo::ToDict() const {
  base::Value::Dict wallpaper_info_dict;
  if (version.IsValid()) {
    wallpaper_info_dict.Set(kNewWallpaperVersionNodeName, version.GetString());
  }
  if (asset_id.has_value()) {
    wallpaper_info_dict.Set(kNewWallpaperAssetIdNodeName,
                            base::NumberToString(asset_id.value()));
  }
  if (unit_id.has_value()) {
    wallpaper_info_dict.Set(kNewWallpaperUnitIdNodeName,
                            base::NumberToString(unit_id.value()));
  }
  base::Value::List online_wallpaper_variant_list;
  for (const auto& variant : variants) {
    base::Value::Dict online_wallpaper_variant_dict;
    online_wallpaper_variant_dict.Set(kNewWallpaperAssetIdNodeName,
                                      base::NumberToString(variant.asset_id));
    online_wallpaper_variant_dict.Set(kOnlineWallpaperUrlNodeName,
                                      variant.raw_url.spec());
    online_wallpaper_variant_dict.Set(kOnlineWallpaperTypeNodeName,
                                      static_cast<int>(variant.type));
    online_wallpaper_variant_list.Append(
        std::move(online_wallpaper_variant_dict));
  }

  wallpaper_info_dict.Set(kNewWallpaperVariantListNodeName,
                          std::move(online_wallpaper_variant_list));
  wallpaper_info_dict.Set(kNewWallpaperCollectionIdNodeName, collection_id);
  // TODO(skau): Change time representation to TimeToValue.
  wallpaper_info_dict.Set(
      kNewWallpaperDateNodeName,
      base::NumberToString(date.ToDeltaSinceWindowsEpoch().InMicroseconds()));
  if (dedup_key) {
    wallpaper_info_dict.Set(kNewWallpaperDedupKeyNodeName, dedup_key.value());
  }
  wallpaper_info_dict.Set(kNewWallpaperLocationNodeName, location);
  wallpaper_info_dict.Set(kNewWallpaperUserFilePathNodeName, user_file_path);
  wallpaper_info_dict.Set(kNewWallpaperLayoutNodeName, layout);
  wallpaper_info_dict.Set(kNewWallpaperTypeNodeName, static_cast<int>(type));
  return wallpaper_info_dict;
}

WallpaperInfo::~WallpaperInfo() = default;

std::ostream& operator<<(std::ostream& os, const WallpaperInfo& info) {
  os << "WallpaperInfo:" << std::endl;
  os << "  location: " << info.location << std::endl;
  os << "  user_file_path: " << info.user_file_path << std::endl;
  os << "  layout: " << info.layout << std::endl;
  os << "  type: " << static_cast<int>(info.type) << std::endl;
  os << "  date: " << info.date << std::endl;
  os << "  dedup_key: " << info.dedup_key.value_or("") << std::endl;
  os << "  asset_id: " << info.asset_id.value_or(-1) << std::endl;
  os << "  collection_id: " << info.collection_id << std::endl;
  os << "  unit_id: " << info.unit_id.value_or(-1) << std::endl;
  os << "  variants_size: " << info.variants.size() << std::endl;
  os << "  version: " << info.version.GetString() << std::endl;
  return os;
}

}  // namespace ash

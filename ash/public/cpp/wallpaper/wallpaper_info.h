// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_INFO_H_
#define ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_INFO_H_

#include <ostream>
#include <string>
#include <string_view>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/wallpaper/google_photos_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

struct ASH_PUBLIC_EXPORT WallpaperInfo {
  // Names of nodes with wallpaper info in |kUserWallpaperInfo| dictionary.
  static constexpr std::string_view kNewWallpaperAssetIdNodeName = "asset_id";
  static constexpr std::string_view kNewWallpaperCollectionIdNodeName =
      "collection_id";
  static constexpr std::string_view kNewWallpaperDateNodeName = "date";
  static constexpr std::string_view kNewWallpaperDedupKeyNodeName = "dedup_key";
  static constexpr std::string_view kNewWallpaperLocationNodeName = "file";
  static constexpr std::string_view kNewWallpaperUserFilePathNodeName =
      "file_path";
  static constexpr std::string_view kNewWallpaperLayoutNodeName = "layout";
  static constexpr std::string_view kNewWallpaperTypeNodeName = "type";
  static constexpr std::string_view kNewWallpaperUnitIdNodeName = "unit_id";
  static constexpr std::string_view kNewWallpaperVariantListNodeName =
      "variants";
  static constexpr std::string_view kNewWallpaperVersionNodeName = "version";

  // Names of nodes for the online wallpaper variant dictionary.
  static constexpr std::string_view kOnlineWallpaperTypeNodeName =
      "online_image_type";
  static constexpr std::string_view kOnlineWallpaperUrlNodeName = "url";

  WallpaperInfo();

  // `target_variant` should match one of the
  // `online_wallpaper_params.variants`.
  explicit WallpaperInfo(const OnlineWallpaperParams& online_wallpaper_params,
                         const OnlineWallpaperVariant& target_variant);
  explicit WallpaperInfo(
      const GooglePhotosWallpaperParams& google_photos_wallpaper_params);

  WallpaperInfo(const std::string& in_location,
                WallpaperLayout in_layout,
                WallpaperType in_type,
                const base::Time& in_date,
                const std::string& in_user_file_path = "");

  WallpaperInfo(const WallpaperInfo& other);
  WallpaperInfo& operator=(const WallpaperInfo& other);

  WallpaperInfo(WallpaperInfo&& other);
  WallpaperInfo& operator=(WallpaperInfo&& other);

  // MatchesAsset() takes the current wallpaper variant into account, whereas
  // MatchesSelection() doesn't. For example if WallpaperInfo A has theme X with
  // variant 1, and WallpaperInfo B has theme X with variant 2,
  // MatchesSelection() will be true and MatchesAsset() will be false. Put
  // differently, MatchesSelection() tells whether the same wallpaper has been
  // selected, whereas MatchesAsset() tells whether the exact same wallpaper
  // image is active.
  bool MatchesSelection(const WallpaperInfo& other) const;
  bool MatchesAsset(const WallpaperInfo& other) const;

  // Used to convert from local or remote syncable pref dict to a WallpaperInfo.
  // Returns nullopt if the |dict| contains any invalid value which may come
  // from future versions of the remote pref .e.g wallpaper type.
  static std::optional<WallpaperInfo> FromDict(const base::Value::Dict& dict);

  // Returns the dictionary representation of the `WallpaperInfo` to be saved
  // into pref store.
  base::Value::Dict ToDict() const;

  ~WallpaperInfo();

  // The version associated with the wallpaper. Expected to be in the form of
  // "major.minor". Major version indicates breaking change, and incompatible
  // with the other versions. Check `base::Version::IsValid()` before using.
  base::Version version;

  // Either file name of migrated wallpaper including first directory level
  // (corresponding to user wallpaper_files_id), online wallpaper URL, or
  // Google Photos id.
  // For SeaPen wallpaper, location is a uint32 id as a string.
  std::string location;

  // user_file_path is the full path of the wallpaper file and is used as
  // the new CurrentWallpaper key. This field is required as the old key which
  // was set to the filename part made the UI mistakenly highlight multiple
  // files with the same name as the currently set wallpaper (b/229420564).
  std::string user_file_path;
  WallpaperLayout layout;
  WallpaperType type;
  // The timestamp at which this wallpaper was first rendered. This is usually
  // synonymous with the user selecting it unless there were delays or
  // unexpected errors when trying to download/decode the wallpaper before it's
  // actually rendered.
  //
  // Note the following do not affect this timestamp:
  // a) Re-rendering this wallpaper (ex: after a reboot/re-login)
  // b) Rendering a different variant of this wallpaper
  //    (ex: dark/light mode changes).
  base::Time date;

  // These fields are applicable if |type| == WallpaperType::kOnceGooglePhotos
  // or WallpaperType::kDailyGooglePhotos.
  std::optional<std::string> dedup_key;

  // These fields are applicable if |type| == WallpaperType::kOnline or
  // WallpaperType::kDaily.
  // TODO(b/279781227): Remove this field in favor of |unit_id|. Note: Do *not*
  // read |asset_id| to make migration easier.
  std::optional<uint64_t> asset_id;
  std::string collection_id;
  std::optional<uint64_t> unit_id;
  std::vector<OnlineWallpaperVariant> variants;

  // Not empty if type == WallpaperType::kOneShot.
  // This field is filled in by ShowWallpaperImage when image is already
  // decoded.
  gfx::ImageSkia one_shot_wallpaper;
};

// For logging use only. Prints out text representation of the `WallpaperInfo`.
ASH_PUBLIC_EXPORT std::ostream& operator<<(std::ostream& os,
                                           const WallpaperInfo& info);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_INFO_H_

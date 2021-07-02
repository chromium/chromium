// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_TYPES_H_
#define ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_TYPES_H_

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// The value assigned to the wallpaper color calculation result if calculation
// fails or is disabled (e.g. from command line, lock/login screens).
constexpr SkColor kInvalidWallpaperColor = SK_ColorTRANSPARENT;

// The width and height of small/large resolution wallpapers. When screen size
// is smaller than |kSmallWallpaperMaxWidth| and |kSmallWallpaperMaxHeight|,
// the small wallpaper is used. Otherwise, use the large wallpaper.
constexpr int kSmallWallpaperMaxWidth = 1366;
constexpr int kSmallWallpaperMaxHeight = 800;
constexpr int kLargeWallpaperMaxWidth = 2560;
constexpr int kLargeWallpaperMaxHeight = 1700;

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered,
//   (b) new constants should only be appended at the end of the enumeration.
enum WallpaperLayout {
  // Center the wallpaper on the desktop without scaling it. The wallpaper
  // may be cropped.
  WALLPAPER_LAYOUT_CENTER,
  // Scale the wallpaper (while preserving its aspect ratio) to cover the
  // desktop; the wallpaper may be cropped.
  WALLPAPER_LAYOUT_CENTER_CROPPED,
  // Scale the wallpaper (without preserving its aspect ratio) to match the
  // desktop's size.
  WALLPAPER_LAYOUT_STRETCH,
  // Tile the wallpaper over the background without scaling it.
  WALLPAPER_LAYOUT_TILE,
  // This must remain last.
  NUM_WALLPAPER_LAYOUT,
};

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered,
//   (b) new constants should only be appended at the end of the enumeration.
enum WallpaperType {
  DAILY = 0,         // Surprise wallpaper. Changes once a day if enabled.
  CUSTOMIZED = 1,    // Selected by user.
  DEFAULT = 2,       // Default.
  /* UNKNOWN = 3 */  // Removed.
  ONLINE = 4,        // WallpaperInfo.location denotes an URL.
  POLICY = 5,        // Controlled by policy, can't be changed by the user.
  THIRDPARTY = 6,    // Current wallpaper is set by a third party app.
  DEVICE = 7,        // Current wallpaper is the device policy controlled
                     // wallpaper. It shows on the login screen if the device
                     // is an enterprise managed device.
  ONE_SHOT = 8,      // Current wallpaper is shown one-time only, which doesn't
                     // belong to a particular user and isn't saved to file. It
                     // goes away when another wallpaper is shown or the browser
                     // process exits. Note: the image will never be blurred or
                     // dimmed.
  WALLPAPER_TYPE_COUNT = 9
};

// The color profile type, ordered as the color profiles applied in
// ash::WallpaperController.
enum class ColorProfileType {
  DARK_VIBRANT = 0,
  NORMAL_VIBRANT,
  LIGHT_VIBRANT,
  DARK_MUTED,
  NORMAL_MUTED,
  LIGHT_MUTED,

  NUM_OF_COLOR_PROFILES,
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_TYPES_H_

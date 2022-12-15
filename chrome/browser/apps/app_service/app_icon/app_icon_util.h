// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_UTIL_H_

#include <map>
#include <vector>

#include "base/files/file_path.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace apps {

// A bitwise-or of icon post-processing effects.
//
// It derives from a uint32_t because it needs to be the same size as the
// uint32_t IconKey.icon_effects field.

// This enum is used to mask the icon_effects value in crosapi, which is a
// stable interface that needs to be backwards compatible. Do not change the
// masks here.
enum IconEffects : uint32_t {
  kNone = 0x00,

  // The icon effects are applied in numerical order, low to high. It is always
  // resize-and-then-badge and never badge-and-then-resize, which can matter if
  // the badge has a fixed size.
  kMdIconStyle = 0x01,   // Icon should have Material Design style. Resize and
                         // add padding if necessary.
  kChromeBadge = 0x02,   // Another (Android) app has the same name.
  kBlocked = 0x04,       // Disabled apps are grayed out and badged.
  kRoundCorners = 0x08,  // Bookmark apps get round corners.
  kPaused = 0x10,  // Paused apps are grayed out and badged to indicate they
                   // cannot be launched.
  kCrOsStandardBackground =
      0x40,                   // Add the white background to the standard icon.
  kCrOsStandardMask = 0x80,   // Apply the mask to the standard icon.
  kCrOsStandardIcon = 0x100,  // Add the white background, maybe shrink the
                              // icon, and apply the mask to the standard icon
                              // This effect combines kCrOsStandardBackground
                              // and kCrOsStandardMask together.
};

inline IconEffects operator|(IconEffects a, IconEffects b) {
  return static_cast<IconEffects>(static_cast<uint32_t>(a) |
                                  static_cast<uint32_t>(b));
}

inline IconEffects operator|=(IconEffects& a, IconEffects b) {
  a = a | b;
  return a;
}

inline IconEffects operator&(IconEffects a, uint32_t b) {
  return static_cast<IconEffects>(static_cast<uint32_t>(a) &
                                  static_cast<uint32_t>(b));
}

inline IconEffects operator&=(IconEffects& a, uint32_t b) {
  a = a & b;
  return a;
}

// Constructs path to an app icon file for the given `app_id` and
// `icon_size_in_px`.
base::FilePath GetIconPath(const base::FilePath& base_path,
                           const std::string& app_id,
                           int32_t icon_size_in_px,
                           bool is_maskable_icon);

// Reads one single icon file for the given `app_id` and `icon_size_in_px`, and
// returns the compressed icon. If there is a maskable icon file, reads the
// maskable icon file. Otherwise, reads other icon file. If there is no
// appropriate icon file, or failed reading the icon file, return nullptr.
IconValuePtr ReadOnBackgroundThread(const base::FilePath& base_path,
                                    const std::string& app_id,
                                    int32_t icon_size_in_px);

// Calls ReadOnBackgroundThread to read icon files for all scale factors for
// the given `app_id` and `size_in_dip`, and returns the compressed icons for
// all scale factors. The same as ReadOnBackgroundThread, reads the maskable
// icon files as the higher priority, and if there is no appropriate icon file,
// or failed reading the icon file, return nullptr for the scale factor.
std::map<ui::ResourceScaleFactor, IconValuePtr> ReadIconFilesOnBackgroundThread(
    const base::FilePath& base_path,
    const std::string& app_id,
    int32_t size_in_dip);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_UTIL_H_

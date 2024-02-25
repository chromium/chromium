// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_COLOR_UTIL_H_
#define ASH_STYLE_COLOR_UTIL_H_

#include "ash/ash_export.h"
#include "ui/color/color_provider_source.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Note: Please stop adding functions to this file. Adding them to
// ash/style/style_util.h file instead. As we are going to merge this class to
// StyleUtil soon.
// TODO(b/279613862): Merging this file to ash/style/style_util.h.
class ASH_EXPORT ColorUtil {
 public:
  // Returns the color provider source for the given `window` if it has
  // has a root window, otherwise returns nullptr.
  static ui::ColorProviderSource* GetColorProviderSourceForWindow(
      const aura::Window* window);

  // Returns an adjusted k means color. For dark mode, it will be dark muted
  // wallpaper prominent color + SK_ColorBLACK 50%. For light mode, it will be
  // light muted wallpaper prominent color + SK_ColorWHITE 50%. Extracts the
  // color on dark mode if `use_dark_color` is true.
  static SkColor AdjustKMeansColor(SkColor k_means_color, bool use_dark_color);

  // Gets the disabled color on |enabled_color|. It can be a disabled
  // background, disabled icon, etc.
  static SkColor GetDisabledColor(SkColor enabled_color);

  // Gets the color of second tone on the given |color_of_first_tone|. e.g,
  // power status icon inside status area is a dual tone icon.
  static SkColor GetSecondToneColor(SkColor color_of_first_tone);

 private:
  ColorUtil() = default;
  ColorUtil(const ColorUtil&) = delete;
  ColorUtil& operator=(const ColorUtil&) = delete;
  ~ColorUtil() = default;
};

}  // namespace ash

#endif  // ASH_STYLE_COLOR_UTIL_H_

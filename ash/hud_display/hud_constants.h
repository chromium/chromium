// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_HUD_CONSTANTS_H_
#define ASH_HUD_DISPLAY_HUD_CONSTANTS_H_

#include "third_party/skia/include/core/SkColor.h"

namespace ash {
namespace hud_display {

constexpr SkAlpha kHUDAlpha = 204;  // = 0.8 * 255

// Use light orange color.
constexpr SkColor kHUDDefaultColor =
    SkColorSetARGB(kHUDAlpha, 0xFF, 0xB2, 0x66);

constexpr SkColor kHUDBackground = SkColorSetARGB(kHUDAlpha, 17, 17, 17);
constexpr SkColor kHUDLegendBackground = kHUDBackground;

// Radius of rounded corners for tabs.
// Must be divisible by 3 to make kTabOverlayWidth an integer.
constexpr int kHUDTabOverlayCornerRadius = 9;

// Border around settings icon in the settings button.
constexpr int kHUDSettingsIconBorder = 5;

// Settings button icon size.
constexpr int kHUDSettingsIconSize = 18;

// Visible border inside the |HUDDisplayView| rectangle around contents.
// |HUDDisplayView| does not use insets itself. Children substitute this inset
// where needed.
constexpr int kHUDInset = 5;

// Defines both the pixel width of the graphs and the amount of data stored
// in each graph ring buffer.
static constexpr size_t kHUDGraphWidth = 190;

// Thickness of reference lines on and around the graphs.
constexpr int kHUDGraphReferenceLineWidth = 1;

// HUD display modes.
enum class HUDDisplayMode {
  CPU = 1,  // First value should be different from default Views::ID = 0.
  MEMORY,
  FPS,
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_HUD_CONSTANTS_H_

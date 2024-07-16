// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UTIL_AMBIENT_UTIL_H_
#define ASH_AMBIENT_UTIL_AMBIENT_UTIL_H_

#include <string>
#include <string_view>

#include "ash/ash_export.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/style/ash_color_provider.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"

#include "ui/gfx/font_list.h"
#include "ui/gfx/shadow_value.h"

namespace ui {
class ColorProvider;
}

namespace ash {

namespace ambient {
namespace util {

inline constexpr int kDefaultTextShadowElevation = 2;

// Returns true if Ash is showing lock screen.
ASH_EXPORT bool IsShowing(LockScreen::ScreenType type);

// Ambient mode uses non-standard colors for some text and the media icon, so
// provides a wrapper for |ColorProvider::GetColor|. This is currently only
// supported for primary and secondary text and icons.
ASH_EXPORT SkColor GetColor(const ui::ColorProvider* color_provider,
                            ui::ColorId color_id,
                            bool dark_mode_enabled);

// Version of the above that uses AshColorProvider::IsDarkModeEnabled().
ASH_EXPORT SkColor GetColor(const ui::ColorProvider* color_provider,
                            ui::ColorId color_id);

// Returns the default fontlist for Ambient Mode.
ASH_EXPORT const gfx::FontList& GetDefaultFontlist();

// Returns the default static text shadow for Ambient Mode. |theme| can be a
// nullptr if the ShadowValues returned are only used to calculate margins, in
// which kPlaceholderColor will be used for the shadow color.
ASH_EXPORT gfx::ShadowValues GetTextShadowValues(
    const ui::ColorProvider* color_provider,
    int elevation = kDefaultTextShadowElevation);

ASH_EXPORT bool IsAmbientModeTopicTypeAllowed(::ambient::TopicType topic);

// A "dynamic" asset is a placeholder in an ambient Lottie animation where a
// photo of interest goes (ex: from a user’s google photos album). This
// contrasts with a "static" asset, which is a fixed image in the animation that
// does not change between animation cycles.
//
// The dynamic asset ids for ambient mode take the following format:
// "_CrOS_Photo_Position<position_id>_<idx>".
//
// A "position" represents a physical location on the screen where a photo
// appears. Its identifier is arbitrary and opaque. But there may be multiple
// assets assigned to a given position. For example, if an animation has a
// cross-fade transition from image 1 to image 2, there may be 2 image assets
// in the animation that share the same position id. However, their indices
// (the last element of the identifier) will be different. Example:
// "_CrOS_Photo_PositionA_1"
// "_CrOS_Photo_PositionA_2"
// ...
//
// The only requirement for the index is that it must reflect the order in which
// that asset appears at its position. The absolute index values do not matter.
//
// Note this naming convention is agreed upon with the animation designer, so
// any changes to the logic must be confirmed with them.
//
// Returns false and leaves the output argument untouched if the |asset_id|
// does not match the naming convention above.
struct ASH_EXPORT ParsedDynamicAssetId {
  // Orders by index first, then by position if indices match:
  // "_CrOS_Photo_PositionA_1"
  // "_CrOS_Photo_PositionB_1"
  // "_CrOS_Photo_PositionA_2"
  // "_CrOS_Photo_PositionB_2"
  bool operator<(const ParsedDynamicAssetId& other) const;

  std::string position_id;
  int idx;
};
ASH_EXPORT bool ParseDynamicLottieAssetId(std::string_view asset_id,
                                          ParsedDynamicAssetId& parsed_output);

// AmbientTheme converted to a string for readability. The returned
// std::string_view is guaranteed to be null-terminated and point to memory
// valid for the lifetime of the program.
ASH_EXPORT std::string_view AmbientThemeToString(
    personalization_app::mojom::AmbientTheme theme);

}  // namespace util
}  // namespace ambient
}  // namespace ash

#endif  // ASH_AMBIENT_UTIL_AMBIENT_UTIL_H_

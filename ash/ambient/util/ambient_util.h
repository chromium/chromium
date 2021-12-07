// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UTIL_AMBIENT_UTIL_H_
#define ASH_AMBIENT_UTIL_AMBIENT_UTIL_H_

#include "ash/ash_export.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/style/ash_color_provider.h"
#include "base/strings/string_piece.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/shadow_value.h"

namespace ui {
class ColorProvider;
}

namespace ash {

namespace ambient {
namespace util {

// Returns true if Ash is showing lock screen.
ASH_EXPORT bool IsShowing(LockScreen::ScreenType type);

// Ambient mode uses non-standard colors for some text and the media icon, so
// provides a wrapper for |AshColorProvider::GetContentLayerColor|. This is
// currently only supported for primary and secondary text and icons.
ASH_EXPORT SkColor
GetContentLayerColor(AshColorProvider::ContentLayerType content_layer_type);

// Returns the default fontlist for Ambient Mode.
ASH_EXPORT const gfx::FontList& GetDefaultFontlist();

// Returns the default static text shadow for Ambient Mode. |theme| can be a
// nullptr if the ShadowValues returned are only used to calculate margins, in
// which kPlaceholderColor will be used for the shadow color.
ASH_EXPORT gfx::ShadowValues GetTextShadowValues(
    const ui::ColorProvider* color_provider);

ASH_EXPORT bool IsAmbientModeTopicTypeAllowed(::ambient::TopicType topic);

// A "dynamic" asset is a spot in an ambient Lottie animation where a photo of
// interest goes (ex: from a user’s google photos album) and is changed each
// time a new animation cycle starts. This contrasts with a "static" asset,
// which is a fixed image in the animation that does not change between
// animation cycles.
//
// Note this function is specific to ash/ambient mode animations and not a
// generic Lottie file standard; i.e. do not use this function for
// non-ash/ambient Lottie files.
ASH_EXPORT bool IsDynamicLottieAsset(base::StringPiece asset_id);

}  // namespace util
}  // namespace ambient
}  // namespace ash

#endif  // ASH_AMBIENT_UTIL_AMBIENT_UTIL_H_

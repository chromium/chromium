// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/util/ambient_util.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/style/ash_color_provider.h"
#include "base/no_destructor.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/shadow_value.h"
#include "ui/native_theme/native_theme.h"

namespace ash {
namespace ambient {
namespace util {

// Appearance of the text shadow.
constexpr int kTextShadowElevation = 2;

bool IsShowing(LockScreen::ScreenType type) {
  return LockScreen::HasInstance() && LockScreen::Get()->screen_type() == type;
}

SkColor GetContentLayerColor(
    AshColorProvider::ContentLayerType content_layer_type) {
  auto* ash_color_provider = AshColorProvider::Get();

  switch (content_layer_type) {
    case AshColorProvider::ContentLayerType::kTextColorPrimary:
    case AshColorProvider::ContentLayerType::kTextColorSecondary:
    case AshColorProvider::ContentLayerType::kIconColorPrimary:
    case AshColorProvider::ContentLayerType::kIconColorSecondary:
      return ash_color_provider->IsDarkModeEnabled()
                 ? ash_color_provider->GetContentLayerColor(content_layer_type)
                 : SK_ColorWHITE;
    default:
      NOTREACHED() << "Unsupported content layer type";
      // Return a very bright color so it's obvious there is a mistake.
      return gfx::kPlaceholderColor;
  }
}

const gfx::FontList& GetDefaultFontlist() {
  static const base::NoDestructor<gfx::FontList> font_list("Google Sans, 64px");
  return *font_list;
}

gfx::ShadowValues GetTextShadowValues(const ui::NativeTheme* theme) {
  // If the theme does not exist the shadow values are being created in
  // order to calculate margins. In that case the color plays no role so set it
  // to gfx::kPlaceholderColor.
  // Currently an elevation of 2 falls back to MakeMdShadowValues so use
  // kColorId_ShadowBase, which is the base shadow color for MdShadowValues,
  // until MakeMdShadowValues is refactored to take in it’s own
  // |key_shadow_color| and |ambient_shadow_color|.
  // TODO(elainechien): crbug.com/1056950
  SkColor shadow_base_color =
      theme ? theme->GetSystemColor(ui::NativeTheme::kColorId_ShadowBase)
            : gfx::kPlaceholderColor;
  return gfx::ShadowValue::MakeShadowValues(
      kTextShadowElevation, shadow_base_color, shadow_base_color);
}

bool IsAmbientModeTopicTypeAllowed(AmbientModeTopicType topic_type) {
  switch (topic_type) {
    case ash::AmbientModeTopicType::kCurated:
      return chromeos::features::kAmbientModeDefaultFeedEnabled.Get();
    case ash::AmbientModeTopicType::kCapturedOnPixel:
      return chromeos::features::kAmbientModeCapturedOnPixelPhotosEnabled.Get();
    case ash::AmbientModeTopicType::kCulturalInstitute:
      return chromeos::features::kAmbientModeCulturalInstitutePhotosEnabled
          .Get();
    case ash::AmbientModeTopicType::kFeatured:
      return chromeos::features::kAmbientModeFeaturedPhotosEnabled.Get();
    case ash::AmbientModeTopicType::kGeo:
      return chromeos::features::kAmbientModeGeoPhotosEnabled.Get();
    case ash::AmbientModeTopicType::kPersonal:
      return chromeos::features::kAmbientModePersonalPhotosEnabled.Get();
    case ash::AmbientModeTopicType::kRss:
      return chromeos::features::kAmbientModeRssPhotosEnabled.Get();
    case ash::AmbientModeTopicType::kOther:
      return false;
  }
}

}  // namespace util
}  // namespace ambient
}  // namespace ash

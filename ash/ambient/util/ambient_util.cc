// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/util/ambient_util.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/style/ash_color_provider.h"
#include "base/no_destructor.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/shadow_value.h"
#include "ui/native_theme/native_theme.h"

namespace ash {
namespace ambient {
namespace util {

// Appearance of the text shadow. If changing the elevation, also change the
// colors accordingly.
constexpr int kTextShadowElevation = 2;
constexpr ui::NativeTheme::ColorId kTextShadowKeyShadowColor =
    ui::NativeTheme::kColorId_ShadowValueKeyShadowElevationTwo;
constexpr ui::NativeTheme::ColorId kTextShadowAmbientShadowColor =
    ui::NativeTheme::kColorId_ShadowValueAmbientShadowElevationTwo;

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
  SkColor key_shadow_color =
      theme ? theme->GetSystemColor(kTextShadowKeyShadowColor)
            : gfx::kPlaceholderColor;
  SkColor ambient_shadow_color =
      theme ? theme->GetSystemColor(kTextShadowAmbientShadowColor)
            : gfx::kPlaceholderColor;
  return gfx::ShadowValue::MakeShadowValues(
      kTextShadowElevation, key_shadow_color, ambient_shadow_color);
}

bool IsAmbientModeTopicTypeAllowed(::ambient::TopicType topic_type) {
  switch (topic_type) {
    case ::ambient::TopicType::kCurated:
      return chromeos::features::kAmbientModeDefaultFeedEnabled.Get();
    case ::ambient::TopicType::kCapturedOnPixel:
      return chromeos::features::kAmbientModeCapturedOnPixelPhotosEnabled.Get();
    case ::ambient::TopicType::kCulturalInstitute:
      return chromeos::features::kAmbientModeCulturalInstitutePhotosEnabled
          .Get();
    case ::ambient::TopicType::kFeatured:
      return chromeos::features::kAmbientModeFeaturedPhotosEnabled.Get();
    case ::ambient::TopicType::kGeo:
      return chromeos::features::kAmbientModeGeoPhotosEnabled.Get();
    case ::ambient::TopicType::kPersonal:
      return chromeos::features::kAmbientModePersonalPhotosEnabled.Get();
    case ::ambient::TopicType::kRss:
      return chromeos::features::kAmbientModeRssPhotosEnabled.Get();
    case ::ambient::TopicType::kOther:
      return false;
  }
}

}  // namespace util
}  // namespace ambient
}  // namespace ash

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/util/ambient_util.h"

#include "ash/ambient/ambient_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/style/ash_color_provider.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/shadow_value.h"

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

gfx::ShadowValues GetTextShadowValues(const ui::ColorProvider* color_provider) {
  // If `color_provider` does not exist the shadow values are being created in
  // order to calculate margins. In that case the color plays no role so set it
  // to gfx::kPlaceholderColor.
  // Currently an elevation of 2 falls back to MakeMdShadowValues so use
  // ui::kColorShadowBase, which is the base shadow color for MdShadowValues,
  // until MakeMdShadowValues is refactored to take in it’s own
  // |key_shadow_color| and |ambient_shadow_color|.
  // TODO(elainechien): crbug.com/1056950
  SkColor shadow_base_color =
      color_provider ? color_provider->GetColor(ui::kColorShadowBase)
                     : gfx::kPlaceholderColor;
  return gfx::ShadowValue::MakeShadowValues(
      kTextShadowElevation, shadow_base_color, shadow_base_color);
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

bool IsDynamicLottieAsset(base::StringPiece asset_id) {
  return base::StartsWith(asset_id, kLottieDynamicAssetIdPrefix);
}

}  // namespace util
}  // namespace ambient
}  // namespace ash

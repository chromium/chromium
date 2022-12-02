// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/util/ambient_util.h"

#include "ash/ambient/ambient_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/utility/lottie_util.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/shadow_value.h"

namespace ash {
namespace ambient {
namespace util {

bool IsShowing(LockScreen::ScreenType type) {
  return LockScreen::HasInstance() && LockScreen::Get()->screen_type() == type;
}

SkColor GetContentLayerColor(
    AshColorProvider::ContentLayerType content_layer_type) {
  return GetContentLayerColor(
      content_layer_type,
      DarkLightModeControllerImpl::Get()->IsDarkModeEnabled());
}

SkColor GetContentLayerColor(
    AshColorProvider::ContentLayerType content_layer_type,
    bool dark_mode_enable) {
  auto* ash_color_provider = AshColorProvider::Get();

  switch (content_layer_type) {
    case AshColorProvider::ContentLayerType::kTextColorPrimary:
    case AshColorProvider::ContentLayerType::kTextColorSecondary:
    case AshColorProvider::ContentLayerType::kIconColorPrimary:
    case AshColorProvider::ContentLayerType::kIconColorSecondary:
      return dark_mode_enable
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

gfx::ShadowValues GetTextShadowValues(const ui::ColorProvider* color_provider,
                                      int elevation) {
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
  return gfx::ShadowValue::MakeShadowValues(elevation, shadow_base_color,
                                            shadow_base_color);
}

bool IsAmbientModeTopicTypeAllowed(::ambient::TopicType topic_type) {
  switch (topic_type) {
    case ::ambient::TopicType::kCurated:
      return features::kAmbientModeDefaultFeedEnabled.Get();
    case ::ambient::TopicType::kCapturedOnPixel:
      return features::kAmbientModeCapturedOnPixelPhotosEnabled.Get();
    case ::ambient::TopicType::kCulturalInstitute:
      return features::kAmbientModeCulturalInstitutePhotosEnabled.Get();
    case ::ambient::TopicType::kFeatured:
      return features::kAmbientModeFeaturedPhotosEnabled.Get();
    case ::ambient::TopicType::kGeo:
      return features::kAmbientModeGeoPhotosEnabled.Get();
    case ::ambient::TopicType::kPersonal:
      return features::kAmbientModePersonalPhotosEnabled.Get();
    case ::ambient::TopicType::kRss:
      return features::kAmbientModeRssPhotosEnabled.Get();
    case ::ambient::TopicType::kOther:
      return false;
  }
}

bool ParsedDynamicAssetId::operator<(const ParsedDynamicAssetId& other) const {
  return idx == other.idx ? position_id < other.position_id : idx < other.idx;
}

bool ParseDynamicLottieAssetId(base::StringPiece asset_id,
                               ParsedDynamicAssetId& parsed_output) {
  static const base::NoDestructor<std::string> kAssetIdPatternStr(
      base::StrCat({kLottieCustomizableIdPrefix,
                    R"(_Photo_Position([[:alnum:]]+)_([[:digit:]]+).*)"}));
  static const base::NoDestructor<RE2> kAssetIdPattern(
      kAssetIdPatternStr->data());
  return RE2::FullMatch(asset_id.data(), *kAssetIdPattern,
                        &parsed_output.position_id, &parsed_output.idx);
}

}  // namespace util
}  // namespace ambient
}  // namespace ash

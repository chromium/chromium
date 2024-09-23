// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/util/ambient_util.h"

#include <string_view>

#include "ash/ambient/ambient_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/utility/lottie_util.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
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

SkColor GetColor(const ui::ColorProvider* color_provider,
                 ui::ColorId color_id) {
  return GetColor(color_provider, color_id,
                  DarkLightModeControllerImpl::Get()->IsDarkModeEnabled());
}

SkColor GetColor(const ui::ColorProvider* color_provider,
                 ui::ColorId color_id,
                 bool dark_mode_enabled) {
  switch (color_id) {
    case kColorAshTextColorPrimary:
    case kColorAshTextColorSecondary:
    case kColorAshIconColorPrimary:
    case kColorAshIconColorSecondary:
      return dark_mode_enabled ? color_provider->GetColor(color_id)
                               : SK_ColorWHITE;
    default:
      NOTREACHED() << "Unsupported content layer type";
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
    case ::ambient::TopicType::kFeatured:
    case ::ambient::TopicType::kGeo:
    case ::ambient::TopicType::kPersonal:
      return true;
    case ::ambient::TopicType::kCurated:
    case ::ambient::TopicType::kCapturedOnPixel:
    case ::ambient::TopicType::kCulturalInstitute:
    case ::ambient::TopicType::kRss:
    case ::ambient::TopicType::kOther:
      return false;
  }
}

bool ParsedDynamicAssetId::operator<(const ParsedDynamicAssetId& other) const {
  return idx == other.idx ? position_id < other.position_id : idx < other.idx;
}

bool ParseDynamicLottieAssetId(std::string_view asset_id,
                               ParsedDynamicAssetId& parsed_output) {
  static const base::NoDestructor<std::string> kAssetIdPatternStr(
      base::StrCat({kLottieCustomizableIdPrefix,
                    R"(_Photo_Position([[:alnum:]]+)_([[:digit:]]+).*)"}));
  static const base::NoDestructor<RE2> kAssetIdPattern(*kAssetIdPatternStr);
  return RE2::FullMatch(asset_id, *kAssetIdPattern, &parsed_output.position_id,
                        &parsed_output.idx);
}

std::string_view AmbientThemeToString(
    personalization_app::mojom::AmbientTheme theme) {
  // See the "AmbientModeThemes" <variants> tag in histograms.xml. These names
  // are currently used for metrics purposes, so they cannot be arbitrarily
  // renamed.
  switch (theme) {
    case personalization_app::mojom::AmbientTheme::kSlideshow:
      return "SlideShow";
    case personalization_app::mojom::AmbientTheme::kFeelTheBreeze:
      return "FeelTheBreeze";
    case personalization_app::mojom::AmbientTheme::kFloatOnBy:
      return "FloatOnBy";
    case personalization_app::mojom::AmbientTheme::kVideo:
      return "Video";
  }
}

}  // namespace util
}  // namespace ambient
}  // namespace ash

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_properties.h"

#include <memory>
#include <optional>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/themes/browser_theme_pack.h"
#include "ui/gfx/color_palette.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace {

// Strings used in alignment properties.
constexpr char kAlignmentCenter[] = "center";
constexpr char kAlignmentTop[] = "top";
constexpr char kAlignmentBottom[] = "bottom";
constexpr char kAlignmentLeft[] = "left";
constexpr char kAlignmentRight[] = "right";

// Strings used in background tiling repetition properties.
constexpr char kTilingNoRepeat[] = "no-repeat";
constexpr char kTilingRepeatX[] = "repeat-x";
constexpr char kTilingRepeatY[] = "repeat-y";
constexpr char kTilingRepeat[] = "repeat";

SkColor GetLightModeColor(int id) {
#if BUILDFLAG(IS_WIN)
  const SkColor kDefaultColorNTPBackground =
      color_utils::GetSysSkColor(COLOR_WINDOW);
  const SkColor kDefaultColorNTPText =
      color_utils::GetSysSkColor(COLOR_WINDOWTEXT);
  const SkColor kDefaultColorNTPLink =
      color_utils::GetSysSkColor(COLOR_HOTLIGHT);
#else
  // TODO(beng): source from theme provider.
  constexpr SkColor kDefaultColorNTPBackground = SK_ColorWHITE;
  constexpr SkColor kDefaultColorNTPText = SK_ColorBLACK;
  constexpr SkColor kDefaultColorNTPLink = SkColorSetRGB(0x06, 0x37, 0x74);
#endif  // BUILDFLAG(IS_WIN)

  switch (id) {
    // Properties stored in theme pack.  If you change these defaults, you must
    // increment the version number in browser_theme_pack.cc.
    case ThemeProperties::COLOR_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE:
    case ThemeProperties::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_ACTIVE:
      return SkColorSetRGB(0xDE, 0xE1, 0xE6);
    case ThemeProperties::COLOR_FRAME_INACTIVE:
    case ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE:
    case ThemeProperties::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INACTIVE:
      return color_utils::HSLShift(
          GetLightModeColor(ThemeProperties::COLOR_FRAME_ACTIVE),
          ThemeProperties::GetDefaultTint(ThemeProperties::TINT_FRAME_INACTIVE,
                                          false));
    case ThemeProperties::COLOR_TOOLBAR:
    case ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE:
      return SK_ColorWHITE;
    case ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE:
    case ThemeProperties::COLOR_TOOLBAR_TEXT:
      return gfx::kGoogleGrey800;
    case ThemeProperties::COLOR_NTP_BACKGROUND:
      return kDefaultColorNTPBackground;
    case ThemeProperties::COLOR_NTP_TEXT:
      return kDefaultColorNTPText;
    case ThemeProperties::COLOR_NTP_LINK:
      return kDefaultColorNTPLink;
    case ThemeProperties::COLOR_NTP_HEADER:
      return SkColorSetRGB(0x96, 0x96, 0x96);
    case ThemeProperties::COLOR_CONTROL_BUTTON_BACKGROUND:
      return SK_ColorTRANSPARENT;
    case ThemeProperties::COLOR_NTP_LOGO:
      return SkColorSetRGB(0xEE, 0xEE, 0xEE);

    // Properties not stored in theme pack.
    case ThemeProperties::COLOR_TAB_STROKE_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TAB_STROKE_FRAME_INACTIVE:
    case ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_INACTIVE:
      return SkColorSetA(SK_ColorBLACK, 0x40);
    case ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_BACKGROUND:
    case ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_DEFAULT_BUTTON_FOREGROUND:
      return gfx::kGoogleBlue700;
    case ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_BUTTON_BORDER:
      return gfx::kGoogleGrey300;
    case ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_CLOSE_BUTTON_INK_DROP:
      return gfx::kGoogleBlue300;
    case ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_FOREGROUND:
    case ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_DEFAULT_BUTTON_BACKGROUND:
      return SK_ColorWHITE;

    case ThemeProperties::COLOR_FRAME_ACTIVE_INCOGNITO:
    case ThemeProperties::COLOR_FRAME_INACTIVE_INCOGNITO:
    case ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE_INCOGNITO:
    case ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE_INCOGNITO:
    case ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE_INCOGNITO:
    case ThemeProperties::
        COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE_INCOGNITO:
    case ThemeProperties::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE_INCOGNITO:
    case ThemeProperties::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_INACTIVE_INCOGNITO:
    case ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE_INCOGNITO:
    case ThemeProperties::
        COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE_INCOGNITO:
    case ThemeProperties::
        COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INCOGNITO_ACTIVE:
    case ThemeProperties::
        COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INCOGNITO_INACTIVE:
      NOTREACHED_IN_MIGRATION()
          << "This color should be queried via its non-incognito "
             "equivalent and an appropriate |incognito| value.";
      return gfx::kPlaceholderColor;
    default:
      NOTREACHED_IN_MIGRATION()
          << "This color should only be queried through ThemeService.";
      return gfx::kPlaceholderColor;
  }
}

std::optional<SkColor> GetIncognitoColor(int id) {
  switch (id) {
    case ThemeProperties::COLOR_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE:
      return color_utils::HSLShift(
          GetLightModeColor(ThemeProperties::COLOR_FRAME_ACTIVE),
          ThemeProperties::GetDefaultTint(ThemeProperties::TINT_FRAME, true));
    case ThemeProperties::COLOR_FRAME_INACTIVE:
    case ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE:
      return color_utils::HSLShift(
          GetLightModeColor(ThemeProperties::COLOR_FRAME_ACTIVE),
          ThemeProperties::GetDefaultTint(ThemeProperties::TINT_FRAME_INACTIVE,
                                          true));
    case ThemeProperties::COLOR_TOOLBAR:
    case ThemeProperties::COLOR_NTP_BACKGROUND:
    case ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE:
      return SkColorSetRGB(0x35, 0x36, 0x3A);
    case ThemeProperties::COLOR_TOOLBAR_TEXT:
      return SK_ColorWHITE;
    case ThemeProperties::COLOR_NTP_TEXT:
      return gfx::kGoogleGrey200;
    case ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE:
      return gfx::kGoogleGrey400;
    case ThemeProperties::COLOR_NTP_LINK:
      return gfx::kGoogleBlue300;
    default:
      return std::nullopt;
  }
}

std::optional<SkColor> GetDarkModeColor(int id) {
  // Current UX thinking is to use the same colors for dark mode and incognito,
  // but this is very subject to change. Additionally, dark mode incognito may
  // end up having a different look. For now, just call into GetIncognitoColor
  // for convenience, but maintain a separate interface.
  return GetIncognitoColor(id);
}

}  // namespace

// static
constexpr int ThemeProperties::kFrameHeightAboveTabs;

// static
int ThemeProperties::StringToAlignment(const std::string& alignment) {
  int alignment_mask = 0;
  for (const std::string& component : base::SplitString(
           alignment, base::kWhitespaceASCII,
           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (base::EqualsCaseInsensitiveASCII(component, kAlignmentTop))
      alignment_mask |= ALIGN_TOP;
    else if (base::EqualsCaseInsensitiveASCII(component, kAlignmentBottom))
      alignment_mask |= ALIGN_BOTTOM;
    else if (base::EqualsCaseInsensitiveASCII(component, kAlignmentLeft))
      alignment_mask |= ALIGN_LEFT;
    else if (base::EqualsCaseInsensitiveASCII(component, kAlignmentRight))
      alignment_mask |= ALIGN_RIGHT;
  }
  return alignment_mask;
}

// static
int ThemeProperties::StringToTiling(const std::string& tiling) {
  if (base::EqualsCaseInsensitiveASCII(tiling, kTilingRepeatX))
    return REPEAT_X;
  if (base::EqualsCaseInsensitiveASCII(tiling, kTilingRepeatY))
    return REPEAT_Y;
  if (base::EqualsCaseInsensitiveASCII(tiling, kTilingRepeat))
    return REPEAT;
  // NO_REPEAT is the default choice.
  return NO_REPEAT;
}

// static
std::string ThemeProperties::AlignmentToString(int alignment) {
  // Convert from an AlignmentProperty back into a string.
  std::string vertical_string(kAlignmentCenter);
  std::string horizontal_string(kAlignmentCenter);

  if (alignment & ALIGN_TOP)
    vertical_string = kAlignmentTop;
  else if (alignment & ALIGN_BOTTOM)
    vertical_string = kAlignmentBottom;

  if (alignment & ALIGN_LEFT)
    horizontal_string = kAlignmentLeft;
  else if (alignment & ALIGN_RIGHT)
    horizontal_string = kAlignmentRight;

  return horizontal_string + " " + vertical_string;
}

// static
std::string ThemeProperties::TilingToString(int tiling) {
  // Convert from a TilingProperty back into a string.
  if (tiling == REPEAT_X)
    return kTilingRepeatX;
  if (tiling == REPEAT_Y)
    return kTilingRepeatY;
  if (tiling == REPEAT)
    return kTilingRepeat;
  return kTilingNoRepeat;
}

// static
color_utils::HSL ThemeProperties::GetDefaultTint(int id,
                                                 bool incognito,
                                                 bool dark_mode) {
  DCHECK(id != TINT_FRAME_INCOGNITO && id != TINT_FRAME_INCOGNITO_INACTIVE)
      << "These values should be queried via their respective non-incognito "
         "equivalents and an appropriate |incognito| value.";

  // If you change these defaults, you must increment the version number in
  // browser_theme_pack.cc.

  // TINT_BUTTONS is used by ThemeService::GetDefaultColor() for both incognito
  // and dark mode, and so must be applied to both.
  if ((id == TINT_BUTTONS) && (incognito || dark_mode))
    return {-1, 0.57, 0.9605};  // kGoogleGrey700 -> kGoogleGrey100

  if ((id == TINT_FRAME) && incognito)
    return {-1, 0.7, 0.075};  // #DEE1E6 -> kGoogleGrey900
  if (id == TINT_FRAME_INACTIVE) {
    // |dark_mode| is only true here when attempting to tint the Windows native
    // frame color while in dark mode when using OS accent titlebar colors.
    // The goal in this case is to match the difference between Chrome default
    // dark mode active and inactive frames as closely as possible without
    // a hue change.
    if (dark_mode)
      return {-1, 0.54, 0.567};  // Roughly kGoogleGrey900 -> kGoogleGrey800

    if (incognito)
      return {0.57, 0.65, 0.1405};  // #DEE1E6 -> kGoogleGrey800
    return {-1, -1, 0.642};         // #DEE1E6 -> #E7EAED
  }

  return {-1, -1, -1};
}

// static
SkColor ThemeProperties::GetDefaultColor(int id,
                                         bool incognito,
                                         bool dark_mode) {
  if (incognito) {
    std::optional<SkColor> incognito_color = GetIncognitoColor(id);
    if (incognito_color.has_value())
      return incognito_color.value();
  }
  if (dark_mode) {
    std::optional<SkColor> dark_mode_color = GetDarkModeColor(id);
    if (dark_mode_color.has_value())
      return dark_mode_color.value();
  }
  return GetLightModeColor(id);
}

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_properties.h"

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/themes/browser_theme_pack.h"
#include "ui/gfx/color_palette.h"

#if defined(OS_WIN)
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

}  // namespace

// static
constexpr int ThemeProperties::kFrameHeightAboveTabs;

// static
int ThemeProperties::StringToAlignment(const std::string& alignment) {
  int alignment_mask = 0;
  for (const std::string& component : base::SplitString(
           alignment, base::kWhitespaceASCII,
           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (base::LowerCaseEqualsASCII(component, kAlignmentTop))
      alignment_mask |= ALIGN_TOP;
    else if (base::LowerCaseEqualsASCII(component, kAlignmentBottom))
      alignment_mask |= ALIGN_BOTTOM;
    else if (base::LowerCaseEqualsASCII(component, kAlignmentLeft))
      alignment_mask |= ALIGN_LEFT;
    else if (base::LowerCaseEqualsASCII(component, kAlignmentRight))
      alignment_mask |= ALIGN_RIGHT;
  }
  return alignment_mask;
}

// static
int ThemeProperties::StringToTiling(const std::string& tiling) {
  if (base::LowerCaseEqualsASCII(tiling, kTilingRepeatX))
    return REPEAT_X;
  if (base::LowerCaseEqualsASCII(tiling, kTilingRepeatY))
    return REPEAT_Y;
  if (base::LowerCaseEqualsASCII(tiling, kTilingRepeat))
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
color_utils::HSL ThemeProperties::GetDefaultTint(int id, bool incognito) {
  DCHECK(id != TINT_FRAME_INCOGNITO && id != TINT_FRAME_INCOGNITO_INACTIVE)
      << "These values should be queried via their respective non-incognito "
         "equivalents and an appropriate |incognito| value.";

  // If you change these defaults, you must increment the version number in
  // browser_theme_pack.cc.
  if (incognito) {
    if (id == TINT_FRAME)
      return {-1, 0.2, 0.35};
    if (id == TINT_FRAME_INACTIVE)
      return {-1, 0.3, 0.6};
    if (id == TINT_BUTTONS)
      return {-1, -1, 0.96};
  } else if (id == TINT_FRAME_INACTIVE) {
    return {-1, -1, 0.75};
  }
  return {-1, -1, -1};
}

// static
SkColor ThemeProperties::GetDefaultColor(int id, bool incognito) {
#if defined(OS_WIN)
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
#endif  // OS_WIN

  switch (id) {
    // Properties stored in theme pack.  If you change these defaults, you must
    // increment the version number in browser_theme_pack.cc.
    case COLOR_FRAME:
    case COLOR_BACKGROUND_TAB:
      return incognito ? gfx::kGoogleGrey900 : SkColorSetRGB(0xDE, 0xE1, 0xE6);
    case COLOR_FRAME_INACTIVE:
    case COLOR_BACKGROUND_TAB_INACTIVE:
      return incognito ? gfx::kGoogleGrey800 : SkColorSetRGB(0xE7, 0xEA, 0xED);
    case COLOR_TOOLBAR:
      return incognito ? SkColorSetRGB(0x32, 0x36, 0x39) : SK_ColorWHITE;
    case COLOR_BOOKMARK_TEXT:
    case COLOR_TAB_TEXT:
    case COLOR_TAB_TEXT_INACTIVE:
      return incognito ? gfx::kGoogleGrey100 : gfx::kGoogleGrey800;
    case COLOR_BACKGROUND_TAB_TEXT:
    case COLOR_BACKGROUND_TAB_TEXT_INACTIVE:
      return incognito ? gfx::kGoogleGrey400 : gfx::kChromeIconGrey;
    case COLOR_NTP_BACKGROUND:
      return incognito ? SkColorSetRGB(0x32, 0x36, 0x39)
                       : kDefaultColorNTPBackground;
    case COLOR_NTP_TEXT:
      return kDefaultColorNTPText;
    case COLOR_NTP_LINK:
      return kDefaultColorNTPLink;
    case COLOR_NTP_HEADER:
      return SkColorSetRGB(0x96, 0x96, 0x96);
    case COLOR_BUTTON_BACKGROUND:
      return SK_ColorTRANSPARENT;

    // Properties not stored in theme pack.
    case COLOR_TAB_CLOSE_BUTTON_ACTIVE:
    case COLOR_TOOLBAR_BUTTON_ICON:
      return incognito ? gfx::kGoogleGrey100 : gfx::kChromeIconGrey;
    case COLOR_TAB_CLOSE_BUTTON_INACTIVE:
    case COLOR_TAB_ALERT_AUDIO:
      return incognito ? gfx::kGoogleGrey400 : gfx::kChromeIconGrey;
    case COLOR_TAB_CLOSE_BUTTON_BACKGROUND_HOVER:
      return incognito ? gfx::kGoogleGrey700 : gfx::kGoogleGrey200;
    case COLOR_TAB_CLOSE_BUTTON_BACKGROUND_PRESSED:
      return incognito ? gfx::kGoogleGrey600 : gfx::kGoogleGrey300;
    case COLOR_TAB_ALERT_RECORDING:
      return incognito ? gfx::kGoogleGrey400 : gfx::kGoogleRed600;
    case COLOR_TAB_ALERT_CAPTURING:
    case COLOR_TAB_PIP_PLAYING:
      return incognito ? gfx::kGoogleGrey400 : gfx::kGoogleBlue600;
    case COLOR_CONTROL_BACKGROUND:
      return SK_ColorWHITE;
    case COLOR_BOOKMARK_BAR_INSTRUCTIONS_TEXT:
      return incognito ? SkColorSetA(SK_ColorWHITE, 0x8A)
                       : SkColorSetRGB(0x64, 0x64, 0x64);
    case COLOR_DETACHED_BOOKMARK_BAR_SEPARATOR:
      // We shouldn't reach this case because the color is calculated from
      // others.
      NOTREACHED();
      return gfx::kPlaceholderColor;
    case COLOR_DETACHED_BOOKMARK_BAR_BACKGROUND:
      return incognito ? SkColorSetRGB(0x32, 0x36, 0x39) : SK_ColorWHITE;
    case COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR:
      return incognito ? SkColorSetRGB(0x28, 0x28, 0x28)
                       : SkColorSetRGB(0xB6, 0xB4, 0xB6);
    case COLOR_TOOLBAR_TOP_SEPARATOR:
    case COLOR_TOOLBAR_TOP_SEPARATOR_INACTIVE:
      return SkColorSetA(SK_ColorBLACK, 0x40);
#if defined(OS_WIN)
    case COLOR_ACCENT_BORDER:
      NOTREACHED();
      return gfx::kPlaceholderColor;
#endif

    case COLOR_FRAME_INCOGNITO:
    case COLOR_FRAME_INCOGNITO_INACTIVE:
    case COLOR_BACKGROUND_TAB_INCOGNITO:
    case COLOR_BACKGROUND_TAB_INCOGNITO_INACTIVE:
    case COLOR_BACKGROUND_TAB_TEXT_INCOGNITO:
    case COLOR_BACKGROUND_TAB_TEXT_INCOGNITO_INACTIVE:
      NOTREACHED() << "These values should be queried via their respective "
                      "non-incognito equivalents and an appropriate "
                      "|incognito| value.";
      FALLTHROUGH;
    default:
      return gfx::kPlaceholderColor;
  }
}

// static
SkColor ThemeProperties::GetDefaultColor(PropertyLookupPair lookup_pair) {
  return GetDefaultColor(lookup_pair.property_id, lookup_pair.is_incognito);
}

// static
ThemeProperties::PropertyLookupPair ThemeProperties::GetLookupID(int input_id) {
  // Mapping of incognito property ids to their corresponding non-incognito
  // property ids.
  base::flat_map<int, int> incognito_property_map({
      {COLOR_FRAME_INCOGNITO, COLOR_FRAME},
      {COLOR_FRAME_INCOGNITO_INACTIVE, COLOR_FRAME_INACTIVE},
      {COLOR_BACKGROUND_TAB_INCOGNITO, COLOR_BACKGROUND_TAB},
      {COLOR_BACKGROUND_TAB_INCOGNITO_INACTIVE, COLOR_BACKGROUND_TAB_INACTIVE},
      {COLOR_BACKGROUND_TAB_TEXT_INCOGNITO, COLOR_BACKGROUND_TAB_TEXT},
      {COLOR_BACKGROUND_TAB_TEXT_INCOGNITO_INACTIVE,
       COLOR_BACKGROUND_TAB_TEXT_INACTIVE},
      {TINT_FRAME_INCOGNITO, TINT_FRAME},
      {TINT_FRAME_INCOGNITO_INACTIVE, TINT_FRAME_INACTIVE},
  });

  const auto found_entry = incognito_property_map.find(input_id);
  if (found_entry != incognito_property_map.end())
    return {found_entry->second, true};
  return {input_id, false};
}

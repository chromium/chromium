// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ASH_COLOR_ID_H_
#define ASH_STYLE_ASH_COLOR_ID_H_

#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"

namespace ash {

// clang-format off
#define ASH_COLOR_IDS \
  /* Shield and Base layer colors. */ \
  E_CPONLY(kColorAshShieldAndBase20) \
  E_CPONLY(kColorAshShieldAndBase40) \
  E_CPONLY(kColorAshShieldAndBase60) \
  E_CPONLY(kColorAshShieldAndBase80) \
  E_CPONLY(kColorAshInvertedShieldAndBase80) \
  E_CPONLY(kColorAshShieldAndBase90) \
  E_CPONLY(kColorAshShieldAndBase95) \
  E_CPONLY(kColorAshShieldAndBaseOpaque) \
  E_CPONLY(kColorAshShieldAndBase80Light) \
  /* Controls Layer colors. */ \
  E_CPONLY(kColorAshHairlineBorderColor) \
  E_CPONLY(kColorAshControlBackgroundColorActive) \
  E_CPONLY(kColorAshControlBackgroundColorAlert) \
  E_CPONLY(kColorAshControlBackgroundColorInactive) \
  E_CPONLY(kColorAshControlBackgroundColorWarning) \
  E_CPONLY(kColorAshControlBackgroundColorPositive) \
  E_CPONLY(kColorAshFocusAuraColor) \
  E_CPONLY(kColorAshSecondaryButtonBackgroundColor)\
  /* Content layer colors. */ \
  E_CPONLY(kColorAshScrollBarColor) \
  E_CPONLY(kColorAshSeparatorColor) \
  E_CPONLY(kColorAshTextColorPrimary) \
  /* Inverted `kColorAshTextColorPrimary` on current color mode. */ \
  E_CPONLY(kColorAshInvertedTextColorPrimary) \
  E_CPONLY(kColorAshTextColorSecondary) \
  E_CPONLY(kColorAshTextColorAlert) \
  E_CPONLY(kColorAshTextColorWarning) \
  E_CPONLY(kColorAshTextColorPositive) \
  E_CPONLY(kColorAshTextColorURL) \
  E_CPONLY(kColorAshIconColorPrimary) \
  E_CPONLY(kColorAshIconColorSecondary) \
  E_CPONLY(kColorAshIconColorAlert) \
  E_CPONLY(kColorAshIconColorWarning) \
  E_CPONLY(kColorAshIconColorPositive) \
  /* Color for prominent icon, e.g, "Add connection" icon button inside
     VPN detailed view. */ \
  E_CPONLY(kColorAshIconColorProminent) \
  /*  Background for kColorAshIconColorSecondary. */ \
  E_CPONLY(kColorAshIconColorSecondaryBackground) \
  /* Colors for Bar Chart within System Info Answer Cards in the Launcher. */ \
  E_CPONLY(kColorAshSystemInfoBarChartColorForeground) \
  E_CPONLY(kColorAshSystemInfoBarChartWarningColorForeground) \
  E_CPONLY(kColorAshSystemInfoBarChartColorBackground) \
  /* The default color for button labels. */ \
  E_CPONLY(kColorAshButtonLabelColor) \
  E_CPONLY(kColorAshButtonLabelColorLight) \
  /* Inverted `kColorAshButtonLabelColor` on current color mode. */ \
  E_CPONLY(kColorAshInvertedButtonLabelColor) \
  E_CPONLY(kColorAshTextColorSuggestion) \
  E_CPONLY(kColorAshButtonLabelColorPrimary) \
  E_CPONLY(kColorAshTextOnBackgroundColor) \
  E_CPONLY(kColorAshIconOnBackgroundColor) \
  /* Color for blue button labels, e.g, 'Retry' button of the system toast. */ \
  E_CPONLY(kColorAshButtonLabelColorBlue) \
  E_CPONLY(kColorAshButtonIconColor) \
  E_CPONLY(kColorAshButtonIconColorLight) \
  E_CPONLY(kColorAshButtonIconColorPrimary) \
  E_CPONLY(kColorAshAppStateIndicatorColor) \
  E_CPONLY(kColorAshAppStateIndicatorColorInactive) \
  /* Color for the shelf drag handle in tablet mode. */ \
  E_CPONLY(kColorAshShelfHandleColor) \
  E_CPONLY(kColorAshShelfTooltipBackgroundColor) \
  E_CPONLY(kColorAshShelfTooltipForegroundColor) \
  E_CPONLY(kColorAshSliderColorActive) \
  E_CPONLY(kColorAshSliderColorInactive) \
  E_CPONLY(kColorAshRadioColorActive) \
  E_CPONLY(kColorAshRadioColorInactive) \
  /* Colors for toggle button. */ \
  E_CPONLY(kColorAshSwitchKnobColorActive) \
  E_CPONLY(kColorAshSwitchKnobColorInactive) \
  E_CPONLY(kColorAshSwitchTrackColorActive) \
  E_CPONLY(kColorAshSwitchTrackColorInactive) \
  /* Color for current active desk's border. */ \
  E_CPONLY(kColorAshCurrentDeskColor) \
  /* Color for the battery's badge (bolt, unreliable, X). */ \
  E_CPONLY(kColorAshBatteryBadgeColor) \
  /* Colors for the switch access's back button. */ \
  E_CPONLY(kColorAshSwitchAccessInnerStrokeColor) \
  E_CPONLY(kColorAshSwitchAccessOuterStrokeColor) \
  /* Colors for the media controls. */ \
  E_CPONLY(kColorAshProgressBarColorForeground) \
  E_CPONLY(kColorAshProgressBarColorBackground) \
  /* Color used to highlight a hovered view. */ \
  E_CPONLY(kColorAshHighlightColorHover) \
  /* Color for the background of battery system info view. */ \
  E_CPONLY(kColorAshBatterySystemInfoBackgroundColor) \
  /* Color for the battery icon in the system info view. */ \
  E_CPONLY(kColorAshBatterySystemInfoIconColor) \
  /* Color of the capture region in the capture session. */ \
  E_CPONLY(kColorAshCaptureRegionColor) \
  E_CPONLY(kColorAshInkDrop) \
  E_CPONLY(kColorAshInkDropOpaqueColor) \
  /* Colors for Google Assistant */ \
  E_CPONLY(kColorAshAssistantBgPlate) \
  E_CPONLY(kColorAshAssistantGreetingEnabled) \
  E_CPONLY(kColorAshSuggestionChipViewTextView) \
  E_CPONLY(kColorAshAssistantQueryHighConfidenceLabel) \
  E_CPONLY(kColorAshAssistantQueryLowConfidenceLabel) \
  E_CPONLY(kColorAshAssistantTextColorPrimary) \
  /* Color for dialog background in arc */ \
  E_CPONLY(kColorAshDialogBackgroundColor) \
  /* Color for disabled button icon */ \
  E_CPONLY(kColorAshButtonIconDisabledColor) \
  E_CPONLY(kColorAshIconSecondaryDisabledColor) \
  E_CPONLY(kColorAshIconPrimaryDisabledColor) \
  E_CPONLY(KColorAshTextDisabledColor) \
  /* Color for icon of the blocked bluetooth device */ \
  E_CPONLY(kColorAshIconColorBlocked)\
  /* Color for icon in title of app streaming bubble */ \
  E_CPONLY(kColorAshEcheIconColorStreaming) \
  /* Color for text of the holding space view with multi select enabled */ \
  E_CPONLY(kColorAshMultiSelectTextColor) \
  /* Color for checkmark icon in holding space */ \
  E_CPONLY(kColorAshCheckmarkIconColor) \
  /* Color for drag image overflow badge text in holding space */ \
  E_CPONLY(kColorAshDragImageOverflowBadgeTextColor) \
  /* Color for feature tile small circle */ \
  E_CPONLY(kColorAshTileSmallCircle) \
  /* Color for the background of the app count indicator on a folder */ \
  E_CPONLY(kColorAshFolderItemCountBackgroundColor) \
  /* Color for the background of the phantom window */ \
  E_CPONLY(kColorAshPhantomWindowBackgroundColor) \
  /* Color for the stroke on the window header view */ \
  E_CPONLY(kColorAshWindowHeaderStrokeColor) \
  /* Color for the 6+ scrollable list view on the login screen */ \
  E_CPONLY(kColorAshLoginScrollableUserListBackground) \
  /* Color for the resize shadow */ \
  E_CPONLY(kColorAshResizeShadowColor)

#include "ui/color/color_id_macros.inc"

enum AshColorIds : ui::ColorId {
  kAshColorsStart = cros_tokens::kCrosSysColorsEnd,

  ASH_COLOR_IDS

  kAshColorsEnd,
};

// Note that this second include is not redundant. The second inclusion of the
// .inc file serves to undefine the macros the first inclusion defined.
#include "ui/color/color_id_macros.inc"

// clang-format on

}  // namespace ash

#endif  // ASH_STYLE_ASH_COLOR_ID_H_

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_PROPERTIES_H_
#define CHROME_BROWSER_THEMES_THEME_PROPERTIES_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"

// Static only class for querying which properties / images are themeable and
// the defaults of these properties.
// All methods are thread safe unless indicated otherwise.
class ThemeProperties {
 public:
  // ---------------------------------------------------------------------------
  // The int values of OverwritableByUserThemeProperties, Alignment, and Tiling
  // are used as a key to store the property in the browser theme pack. If you
  // modify any of these enums, increment the version number in
  // browser_theme_pack.cc.

  enum OverwritableByUserThemeProperty {
    COLOR_FRAME_ACTIVE,
    COLOR_FRAME_INACTIVE,
    // Instead of using the INCOGNITO variants directly, most code should
    // use the original color ID in an incognito-aware context (such as
    // GetDefaultColor).  This comment applies to other properties tagged
    // INCOGNITO below as well.
    COLOR_FRAME_ACTIVE_INCOGNITO,
    COLOR_FRAME_INACTIVE_INCOGNITO,
    COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE,
    COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE,
    COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE_INCOGNITO,
    COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE_INCOGNITO,
    COLOR_TOOLBAR,
    COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE,
    COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE,
    COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE,
    COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE_INCOGNITO,
    COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE_INCOGNITO,
    COLOR_BOOKMARK_TEXT,
    COLOR_NTP_BACKGROUND,
    COLOR_NTP_TEXT,
    COLOR_NTP_LINK,
    COLOR_NTP_HEADER,
    COLOR_CONTROL_BUTTON_BACKGROUND,
    COLOR_TOOLBAR_BUTTON_ICON,
    COLOR_OMNIBOX_TEXT,
    COLOR_OMNIBOX_BACKGROUND,

    TINT_BUTTONS,
    TINT_FRAME,
    TINT_FRAME_INACTIVE,
    TINT_FRAME_INCOGNITO,
    TINT_FRAME_INCOGNITO_INACTIVE,
    TINT_BACKGROUND_TAB,

    NTP_BACKGROUND_ALIGNMENT,
    NTP_BACKGROUND_TILING,
    NTP_LOGO_ALTERNATE,
  };

  // A bitfield mask for alignments.
  enum Alignment {
    ALIGN_CENTER = 0,
    ALIGN_LEFT   = 1 << 0,
    ALIGN_TOP    = 1 << 1,
    ALIGN_RIGHT  = 1 << 2,
    ALIGN_BOTTOM = 1 << 3,
  };

  // Background tiling choices.
  enum Tiling {
    NO_REPEAT = 0,
    REPEAT_X = 1,
    REPEAT_Y = 2,
    REPEAT = 3
  };

  // --------------------------------------------------------------------------
  // The int value of the properties in NotOverwritableByUserThemeProperties
  // has no special meaning. Modify the enum to your heart's content.
  // The enum takes on values >= 1000 as not to overlap with
  // OverwritableByUserThemeProperties.
  enum NotOverwritableByUserThemeProperty {
    // The color of the border drawn around the location bar.
    COLOR_LOCATION_BAR_BORDER = 1000,

    // The color of the line separating the bottom of the toolbar from the
    // contents.
    COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR,

    // The color of a toolbar button's icon when it is being hovered or pressed.
    COLOR_TOOLBAR_BUTTON_ICON_HOVERED,
    COLOR_TOOLBAR_BUTTON_ICON_PRESSED,

    // The color of a disabled toolbar button's icon.
    COLOR_TOOLBAR_BUTTON_ICON_INACTIVE,

    // The color of the line separating the top of the toolbar from the region
    // above. For a tabbed browser window, this is the line along the bottom
    // edge of the tabstrip, the stroke around the tabs, and the new tab button
    // stroke/shadow color.
    COLOR_TOOLBAR_TOP_SEPARATOR,
    COLOR_TOOLBAR_TOP_SEPARATOR_INACTIVE,

    // Colors of vertical separators, such as on the bookmark bar or on the DL
    // shelf.
    COLOR_TOOLBAR_VERTICAL_SEPARATOR,

    // Opaque base color for toolbar button ink drops.
    COLOR_TOOLBAR_INK_DROP,

    // Color used for various 'shelves' and 'bars'.
    COLOR_DOWNLOAD_SHELF,
    COLOR_INFOBAR,
    COLOR_STATUS_BUBBLE,

    // Colors used when displaying hover cards.
    COLOR_HOVER_CARD_NO_PREVIEW_FOREGROUND,
    COLOR_HOVER_CARD_NO_PREVIEW_BACKGROUND,

    // Colors used for the active tab.
    COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE,
    COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE,
    COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE_INCOGNITO,
    COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE_INCOGNITO,

    COLOR_TAB_FOREGROUND_ACTIVE_FRAME_INACTIVE,
    COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE_INCOGNITO,
    COLOR_TAB_FOREGROUND_ACTIVE_FRAME_INACTIVE_INCOGNITO,

    // The throbber colors for tabs or anything on a toolbar (currently, only
    // the download shelf). If you're adding a throbber elsewhere, such as in
    // a dialog or bubble, you likely want
    // NativeTheme::kColorId_ThrobberSpinningColor.
    COLOR_TAB_THROBBER_SPINNING,
    COLOR_TAB_THROBBER_WAITING,

    // Note: All tab group color ids must be grouped together consecutively and
    // grouped together by use (eg grouped by dialog, context menu etc).
    // This permits range checking and reduces redundant code. If you change or
    // add to any of the below color ids, change the relevant code in
    // ThemeHelper.

    // The colors used for tab groups in the tabstrip.
    COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_GREY,
    COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_BLUE,
    COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_RED,
    COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_YELLOW,
    COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_GREEN,
    COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_PINK,
    COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_PURPLE,
    COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_CYAN,
    COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_GREY,
    COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_BLUE,
    COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_RED,
    COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_YELLOW,
    COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_GREEN,
    COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_PINK,
    COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_PURPLE,
    COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_CYAN,
    // The colors used for tab groups in the bubble dialog view.
    COLOR_TAB_GROUP_DIALOG_GREY,
    COLOR_TAB_GROUP_DIALOG_BLUE,
    COLOR_TAB_GROUP_DIALOG_RED,
    COLOR_TAB_GROUP_DIALOG_YELLOW,
    COLOR_TAB_GROUP_DIALOG_GREEN,
    COLOR_TAB_GROUP_DIALOG_PINK,
    COLOR_TAB_GROUP_DIALOG_PURPLE,
    COLOR_TAB_GROUP_DIALOG_CYAN,
    // The colors used for tab groups in the context submenu.
    COLOR_TAB_GROUP_CONTEXT_MENU_GREY,
    COLOR_TAB_GROUP_CONTEXT_MENU_BLUE,
    COLOR_TAB_GROUP_CONTEXT_MENU_RED,
    COLOR_TAB_GROUP_CONTEXT_MENU_YELLOW,
    COLOR_TAB_GROUP_CONTEXT_MENU_GREEN,
    COLOR_TAB_GROUP_CONTEXT_MENU_PINK,
    COLOR_TAB_GROUP_CONTEXT_MENU_PURPLE,
    COLOR_TAB_GROUP_CONTEXT_MENU_CYAN,

    // Calculated representative colors for the background of window control
    // buttons.
    COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_ACTIVE,
    COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INACTIVE,
    COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INCOGNITO_ACTIVE,
    COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INCOGNITO_INACTIVE,

    COLOR_NTP_TEXT_LIGHT,
    COLOR_NTP_LOGO,
    // Color for the background of the most visited/custom link tile.
    COLOR_NTP_SHORTCUT,

#if defined(OS_WIN)
    // The colors of the 1px border around the window on Windows 10.
    COLOR_ACCENT_BORDER_ACTIVE,
    COLOR_ACCENT_BORDER_INACTIVE,
#endif  // OS_WIN

    SHOULD_FILL_BACKGROUND_TAB_COLOR,

    // Colors for in-product help promo bubbles.
    COLOR_FEATURE_PROMO_BUBBLE_TEXT,
    COLOR_FEATURE_PROMO_BUBBLE_BACKGROUND,

    COLOR_OMNIBOX_BACKGROUND_HOVERED,
    COLOR_OMNIBOX_SELECTED_KEYWORD,
    COLOR_OMNIBOX_TEXT_DIMMED,
    COLOR_OMNIBOX_RESULTS_BG,
    COLOR_OMNIBOX_RESULTS_BG_HOVERED,
    COLOR_OMNIBOX_RESULTS_BG_SELECTED,
    COLOR_OMNIBOX_RESULTS_TEXT_SELECTED,
    COLOR_OMNIBOX_RESULTS_TEXT_DIMMED,
    COLOR_OMNIBOX_RESULTS_TEXT_DIMMED_SELECTED,
    COLOR_OMNIBOX_RESULTS_ICON,
    COLOR_OMNIBOX_RESULTS_ICON_SELECTED,
    COLOR_OMNIBOX_RESULTS_URL,
    COLOR_OMNIBOX_RESULTS_URL_SELECTED,
    COLOR_OMNIBOX_RESULTS_FOCUS_BAR,
    COLOR_OMNIBOX_RESULTS_BUTTON_BORDER,
    COLOR_OMNIBOX_BUBBLE_OUTLINE,
    COLOR_OMNIBOX_BUBBLE_OUTLINE_EXPERIMENTAL_KEYWORD_MODE,
    COLOR_OMNIBOX_SECURITY_CHIP_DEFAULT,
    COLOR_OMNIBOX_SECURITY_CHIP_SECURE,
    COLOR_OMNIBOX_SECURITY_CHIP_DANGEROUS,
  };

  // Themes are hardcoded to draw frame images as if they start this many DIPs
  // above the top of the tabstrip, no matter how much space actually exists.
  // This aids with backwards compatibility (for some themes; Chrome's behavior
  // has been inconsistent over time), provides a consistent alignment point for
  // theme authors, and ensures the frame image won't need to be mirrored above
  // the tabs in Refresh (since frame heights above the tabs are never greater
  // than this).
  static constexpr int kFrameHeightAboveTabs = 16;

  // Used by the browser theme pack to parse alignments from something like
  // "top left" into a bitmask of Alignment.
  static int StringToAlignment(const std::string& alignment);

  // Used by the browser theme pack to parse alignments from something like
  // "no-repeat" into a Tiling value.
  static int StringToTiling(const std::string& tiling);

  // Converts a bitmask of Alignment into a string like "top left". The result
  // is used to generate a CSS value.
  static std::string AlignmentToString(int alignment);

  // Converts a Tiling into a string like "no-repeat". The result is used to
  // generate a CSS value.
  static std::string TilingToString(int tiling);

  // Returns the default tint for the given tint |id| TINT_* enum value.
  // Returns an HSL value of {-1, -1, -1} if |id| is invalid.
  static color_utils::HSL GetDefaultTint(int id,
                                         bool incognito,
                                         bool dark_mode = false);

  // Returns the default color for the given color |id| COLOR_* enum value.
  // Returns gfx::kPlaceholderColor if |id| is invalid.
  static SkColor GetDefaultColor(int id,
                                 bool incognito,
                                 bool dark_mode = false);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ThemeProperties);
};

#endif  // CHROME_BROWSER_THEMES_THEME_PROPERTIES_H_

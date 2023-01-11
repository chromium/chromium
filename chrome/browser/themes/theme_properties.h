// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_PROPERTIES_H_
#define CHROME_BROWSER_THEMES_THEME_PROPERTIES_H_

#include <set>
#include <string>

#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"

// Static only class for querying which properties / images are themeable and
// the defaults of these properties.
// All methods are thread safe unless indicated otherwise.
class ThemeProperties {
 public:
  // ---------------------------------------------------------------------------
  // The int values of the enums below are used as keys to store properties in
  // the browser theme pack.
  //
  // /!\ If you make any changes to these enums, you must also increment
  // kThemePackVersion in browser_theme_pack.cc, or else themes will display
  // incorrectly.

  enum OverwritableByUserThemeProperty {
    // Instead of using the INCOGNITO variants directly, most code should
    // use the original color ID in an incognito-aware context (such as
    // GetDefaultColor).  This comment applies to other properties tagged
    // INCOGNITO below as well.
    COLOR_BOOKMARK_TEXT,
    COLOR_CONTROL_BUTTON_BACKGROUND,
    COLOR_FRAME_ACTIVE,
    COLOR_FRAME_ACTIVE_INCOGNITO,
    COLOR_FRAME_INACTIVE,
    COLOR_FRAME_INACTIVE_INCOGNITO,
    COLOR_NTP_BACKGROUND,
    COLOR_NTP_LINK,
    COLOR_NTP_HEADER,
    COLOR_NTP_TEXT,
    COLOR_OMNIBOX_BACKGROUND,
    COLOR_OMNIBOX_TEXT,
    COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE,
    COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE_INCOGNITO,
    COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE,
    COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE_INCOGNITO,
    COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE,
    COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE,
    COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE_INCOGNITO,
    COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE,
    COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE_INCOGNITO,
    COLOR_TOOLBAR,
    COLOR_TOOLBAR_BUTTON_ICON,
    COLOR_TOOLBAR_TEXT,

    TINT_BACKGROUND_TAB,
    TINT_BUTTONS,
    TINT_FRAME,
    TINT_FRAME_INACTIVE,
    TINT_FRAME_INCOGNITO,
    TINT_FRAME_INCOGNITO_INACTIVE,

    NTP_BACKGROUND_ALIGNMENT,
    NTP_BACKGROUND_TILING,
    NTP_LOGO_ALTERNATE,

    // /!\ If you make any changes to this enum, you must also increment
    // kThemePackVersion in browser_theme_pack.cc, or else themes will display
    // incorrectly.
  };

  // A bitfield mask for alignments.
  enum Alignment {
    ALIGN_CENTER = 0,
    ALIGN_LEFT = 1 << 0,
    ALIGN_TOP = 1 << 1,
    ALIGN_RIGHT = 1 << 2,
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
  //
  // /!\ If you make any changes to this enum, you must also increment
  // kThemePackVersion in browser_theme_pack.cc, or else themes will display
  // incorrectly.
  enum NotOverwritableByUserThemeProperty {
    // The color of the border drawn around the location bar.
    COLOR_LOCATION_BAR_BORDER = 1000,
    COLOR_LOCATION_BAR_BORDER_OPAQUE,

    COLOR_TOOLBAR_BUTTON_BORDER,
    COLOR_TOOLBAR_BUTTON_ICON_HOVERED,
    COLOR_TOOLBAR_BUTTON_ICON_INACTIVE,
    COLOR_TOOLBAR_BUTTON_ICON_PRESSED,
    COLOR_TOOLBAR_BUTTON_TEXT,

    // The color of the line separating the bottom of the toolbar from the
    // contents.
    COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR,

    // Opaque base color for toolbar button ink drops.
    COLOR_TOOLBAR_INK_DROP,

    // The color of the line separating the top of the toolbar from the region
    // above. For a tabbed browser window, this is the line along the bottom
    // edge of the tabstrip.
    COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_ACTIVE,
    COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_INACTIVE,

    // /!\ If you make any changes to this enum, you must also increment
    // kThemePackVersion in browser_theme_pack.cc, or else themes will display
    // incorrectly.

    // Colors of vertical separators, such as on the bookmark bar or on the DL
    // shelf.
    COLOR_TOOLBAR_VERTICAL_SEPARATOR,

    // Colors used for the active tab.
    COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE,
    COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE,
    COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE_INCOGNITO,
    COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE_INCOGNITO,

    COLOR_TAB_FOREGROUND_ACTIVE_FRAME_INACTIVE,
    COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE_INCOGNITO,
    COLOR_TAB_FOREGROUND_ACTIVE_FRAME_INACTIVE_INCOGNITO,

    // Colors used for the stroke around tabs.
    COLOR_TAB_STROKE_FRAME_ACTIVE,
    COLOR_TAB_STROKE_FRAME_INACTIVE,

    // The throbber colors for tabs or anything on a toolbar (currently, only
    // the download shelf). Do not use directly; only for use inside
    // browser_theme_pack.cc.
    COLOR_TAB_THROBBER_SPINNING,
    COLOR_TAB_THROBBER_WAITING,

    // /!\ If you make any changes to this enum, you must also increment
    // kThemePackVersion in browser_theme_pack.cc, or else themes will display
    // incorrectly.

    // Calculated representative colors for the background of window control
    // buttons.
    COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_ACTIVE,
    COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INACTIVE,
    COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INCOGNITO_ACTIVE,
    COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INCOGNITO_INACTIVE,

    COLOR_NTP_LOGO,
    COLOR_NTP_SECTION_BORDER,
    COLOR_NTP_TEXT_LIGHT,

#if BUILDFLAG(IS_WIN)
    // The colors of the 1px border around the window on Windows 10.
    COLOR_ACCENT_BORDER_ACTIVE,
    COLOR_ACCENT_BORDER_INACTIVE,
#endif  // BUILDFLAG(IS_WIN)

    SHOULD_FILL_BACKGROUND_TAB_COLOR,

    // Colors for in-product help promo bubbles.
    COLOR_FEATURE_PROMO_BUBBLE_BACKGROUND,
    COLOR_FEATURE_PROMO_BUBBLE_BUTTON_BORDER,
    COLOR_FEATURE_PROMO_BUBBLE_CLOSE_BUTTON_INK_DROP,
    COLOR_FEATURE_PROMO_BUBBLE_DEFAULT_BUTTON_BACKGROUND,
    COLOR_FEATURE_PROMO_BUBBLE_DEFAULT_BUTTON_FOREGROUND,
    COLOR_FEATURE_PROMO_BUBBLE_FOREGROUND,

    // Colors used for the Bookmark bar
    COLOR_BOOKMARK_BAR_BACKGROUND,
    // If COLOR_TOOLBAR_BUTTON_ICON is defined in the custom theme, that color
    // will be returned, otherwise it will be transparent so the default
    // favicon color is retained.
    COLOR_BOOKMARK_FAVICON,
    COLOR_BOOKMARK_SEPARATOR,

    // Colors used for the frame caption/foreground
    COLOR_FRAME_CAPTION_ACTIVE,
    COLOR_FRAME_CAPTION_INACTIVE,

    // Colors used for the FlyingIndicator
    COLOR_FLYING_INDICATOR_BACKGROUND,
    COLOR_FLYING_INDICATOR_FOREGROUND,

    // /!\ If you make any changes to this enum, you must also increment
    // kThemePackVersion in browser_theme_pack.cc, or else themes will display
    // incorrectly.
  };

  // Themes are hardcoded to draw frame images as if they start this many DIPs
  // above the top of the tabstrip, no matter how much space actually exists.
  // This aids with backwards compatibility (for some themes; Chrome's behavior
  // has been inconsistent over time), provides a consistent alignment point for
  // theme authors, and ensures the frame image won't need to be mirrored above
  // the tabs in Refresh (since frame heights above the tabs are never greater
  // than this).
  static constexpr int kFrameHeightAboveTabs = 16;

  ThemeProperties() = delete;
  ThemeProperties(const ThemeProperties&) = delete;
  ThemeProperties& operator=(const ThemeProperties&) = delete;

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
};

#endif  // CHROME_BROWSER_THEMES_THEME_PROPERTIES_H_

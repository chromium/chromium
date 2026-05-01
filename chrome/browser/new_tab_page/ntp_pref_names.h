// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_NTP_PREF_NAMES_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_NTP_PREF_NAMES_H_

namespace ntp_prefs {

// Tracks whether the user has chosen to hide the shortcuts tiles on the NTP.
inline constexpr char kNtpShortcutsVisible[] = "ntp.shortcust_visible";

// Tracks whether the user has chosen to use custom links or most visited sites
// for the shortcut tiles on the NTP. This pref is migrated to
// `kNtpShortcutsType`.
inline constexpr char kNtpUseMostVisitedTiles[] = "ntp.use_most_visited_tiles";

// Tracks what type of shortcuts tiles to show. Values must stay in sync with
// `TileType` enum (0 = TopSites, 1 = CustomLinks). This pref is migrated to
// `kNtpCustomLinksVisible` and `kNtpEnterpriseShortcutsVisible`.
inline constexpr char kNtpShortcutsType[] = "ntp.shortcuts_type";

// Tracks whether the user has chosen to show custom links on the NTP.
// When false, the user has chosen to show top sites.
inline constexpr char kNtpCustomLinksVisible[] = "ntp.custom_links_visible";

// Tracks whether the user has chosen to hide the enterprise shortcuts tiles on
// the NTP.
inline constexpr char kNtpEnterpriseShortcutsVisible[] =
    "ntp.enterprise_shortcuts_visible";

// Tracks whether the user has chosen to hide the personal tiles on the NTP.
// Used when enterprise shortcuts are enabled and mixed with personal tiles.
inline constexpr char kNtpPersonalShortcutsVisible[] =
    "ntp.personal_shortcuts_visible";

// Tracks whether the user has chosen to show all most visited tiles on the NTP.
inline constexpr char kNtpShowAllMostVisitedTiles[] =
    "ntp.show_all_most_visited_tiles";

// Tracks the last time the staleness counter was updated for shortcuts.
inline constexpr char kNtpLastShortcutsStalenessUpdate[] =
    "ntp.last_shortcuts_staleness_update";

// Tracks the staleness counter for shortcuts.
inline constexpr char kNtpShortcutsStalenessCount[] =
    "ntp.shortcuts_staleness_count";

// Tracks whether shortcuts auto-removal for inactivity is disabled.
inline constexpr char kNtpShortcutsAutoRemovalDisabled[] =
    "ntp.shortcuts_auto_removal_disabled";

// Tracks the last time the staleness counter was updated for modules.
inline constexpr char kNtpLastModuleStalenessUpdate[] =
    "ntp.last_module_staleness_update";

// Tracks the staleness counter for each module id.
inline constexpr char kNtpModuleStalenessCountDict[] =
    "ntp.module_staleness_count_dict";

// Tracks whether module auto-removal for inactivity is disabled for each
// module. A special key of "all_modules" indicates that the auto-removal is
// disabled for all modules.
inline constexpr char kNtpModulesAutoRemovalDisabledDict[] =
    "ntp.modules_auto_removal_disabled";

// Tracks whether the user has enabled animated doodles on the NTP.
inline constexpr char kNtpAnimatedDoodlesEnabled[] =
    "ntp.animated_doodles_enabled";

// Tracks whether the user has enabled doodle murals on the NTP.
inline constexpr char kNtpDoodleMuralsEnabled[] = "ntp.doodle_murals_enabled";

// Tracks how many times a user hovers on a NewTabPage tile link.
inline constexpr char kNtpMostVisitedTileHoverCount[] =
    "ntp.most_visited_tile_hover_count";

// Tracks how many times a user navigates to a NewTabPage tile link.
inline constexpr char kNtpMostVisitedTileNavigationCount[] =
    "ntp.most_visited_tile_navigation_count";

}  // namespace ntp_prefs

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_NTP_PREF_NAMES_H_

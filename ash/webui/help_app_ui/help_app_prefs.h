// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_HELP_APP_UI_HELP_APP_PREFS_H_
#define ASH_WEBUI_HELP_APP_UI_HELP_APP_PREFS_H_

class PrefRegistrySimple;

namespace ash::help_app::prefs {

// Boolean pref for whether or not the user has completed the new device
// checklist in the help app.
inline constexpr char kHelpAppHasCompletedNewDeviceChecklist[] =
    "help_app.has_completed_new_device_checklist";

// Boolean pref for whether or not the user has ever visited the how_to page in
// the help app.
inline constexpr char kHelpAppHasVisitedHowToPage[] =
    "help_app.has_visited_how_to_page";

// Last milestone on which a Help App notification was shown.
inline constexpr char kHelpAppNotificationLastShownMilestone[] =
    "help_app_notification_last_shown_milestone";

// Pref name for whether we should show the Getting Started module in the Help
// app.
inline constexpr char kHelpAppShouldShowGetStarted[] =
    "help_app.should_show_get_started";

// Pref name for whether we should show the Parental Control module in the Help
// app.
inline constexpr char kHelpAppShouldShowParentalControl[] =
    "help_app.should_show_parental_control";

// Pref name for whether the device was in tablet mode when going through
// the OOBE.
inline constexpr char kHelpAppTabletModeDuringOobe[] =
    "help_app.tablet_mode_during_oobe";

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace ash::help_app::prefs

#endif  // ASH_WEBUI_HELP_APP_UI_HELP_APP_PREFS_H_

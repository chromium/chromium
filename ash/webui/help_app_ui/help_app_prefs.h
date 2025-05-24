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

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace ash::help_app::prefs

#endif  // ASH_WEBUI_HELP_APP_UI_HELP_APP_PREFS_H_

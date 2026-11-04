// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_bar_utils.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/common/bookmark_bar_visibility_state.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/search/search.h"
#include "components/user_prefs/user_prefs.h"

namespace chrome {

void ToggleBookmarkBarWhenVisible(content::BrowserContext* browser_context) {
  PrefService* prefs = user_prefs::UserPrefs::Get(browser_context);
  // The user changed when the bookmark bar is shown, update the preferences.
  if (base::FeatureList::IsEnabled(
          ntp_features::kNtpSimplificationBookmarkBar)) {
    auto current_state = static_cast<bookmarks::BookmarkBarVisibilityState>(
        prefs->GetInteger(bookmarks::prefs::kBookmarkBarVisibilityState));
    if (current_state == bookmarks::BookmarkBarVisibilityState::kAlwaysShow) {
      prefs->SetInteger(
          bookmarks::prefs::kBookmarkBarVisibilityState,
          static_cast<int>(bookmarks::BookmarkBarVisibilityState::kAlwaysHide));
    } else {
      prefs->SetInteger(
          bookmarks::prefs::kBookmarkBarVisibilityState,
          static_cast<int>(bookmarks::BookmarkBarVisibilityState::kAlwaysShow));
    }
  } else {
    const bool always_show =
        !prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar);
    prefs->SetBoolean(bookmarks::prefs::kShowBookmarkBar, always_show);
  }
}

// Called upon direct user interaction with the Bookmarks Bar UI (e.g. clicking
// a bookmark/folder, interacting with saved tab groups, or modifying items via
// context menu/drag-and-drop).
//
// For users in the NTP Simplification transition period (`IsDefaultValue()` is
// true), this explicitly sets `kBookmarkBarVisibilityState` to
// `kOnlyShowOnNtp`. This establishes the user store as the controlling
// preference store, transitioning the user out of the experiment's default
// state and preventing future auto-hiding.
//
// Note: Users who had `kShowBookmarkBar` set to true prior to the experiment
// are upgraded to `kAlwaysShow` at startup in `BookmarkBarController`, making
// `IsDefaultValue()` false. Therefore, `IsDefaultValue()` is only true here for
// users in the "off by default" / "only show on NTP" transition group.
void UpdateBookmarkBarVisibilityPrefOnUserAction(Profile* profile) {
  if (!profile || !profile->GetPrefs()) {
    return;
  }

  if (!base::FeatureList::IsEnabled(
          ntp_features::kNtpSimplificationBookmarkBar)) {
    return;
  }

  PrefService* prefs = profile->GetPrefs();
  const PrefService::Preference* state_pref =
      prefs->FindPreference(bookmarks::prefs::kBookmarkBarVisibilityState);

  if (state_pref && state_pref->IsDefaultValue()) {
    prefs->SetInteger(
        bookmarks::prefs::kBookmarkBarVisibilityState,
        static_cast<int>(
            bookmarks::BookmarkBarVisibilityState::kOnlyShowOnNtp));
  }
}

bool IsAppsShortcutEnabled(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  // Chrome OS uses the app list / app launcher. Android has no apps shortcut on
  // the bookmark bar.
  return false;
#else
  return search::IsInstantExtendedAPIEnabled() && !profile->IsOffTheRecord();
#endif
}

bool ShouldShowAppsShortcutInBookmarkBar(Profile* profile) {
  return IsAppsShortcutEnabled(profile) &&
         profile->GetPrefs()->GetBoolean(
             bookmarks::prefs::kShowAppsShortcutInBookmarkBar);
}

bool ShouldShowTabGroupsInBookmarkBar(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      bookmarks::prefs::kShowTabGroupsInBookmarkBar);
}

}  // namespace chrome

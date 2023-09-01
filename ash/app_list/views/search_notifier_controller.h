// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_NOTIFIER_CONTROLLER_H_
#define ASH_APP_LIST_VIEWS_SEARCH_NOTIFIER_CONTROLLER_H_

#include "ash/ash_export.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

// Controls the toast search notifier in launcher search.
class ASH_EXPORT SearchNotifierController {
 public:
  SearchNotifierController();
  SearchNotifierController(const SearchNotifierController&) = delete;
  SearchNotifierController& operator=(const SearchNotifierController&) = delete;
  ~SearchNotifierController() = default;

  // Registers profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Clears the profile perfs related to the search notifier. Used when a new
  // user session is added in the app list controller. This feature can be
  // toggled in chrome:://flags with launcher-nudge-session-reset flag.
  static void ResetPrefsForNewUserSession(PrefService* prefs);

  // Gets the number of times that the privacy notice has been shown.
  static int GetPrivacyNoticeShownCount(PrefService* prefs);

  // Enables the image search category in launcher search.
  void EnableImageSearch();

  // Returns true if the privacy notice should be shown.
  bool ShouldShowPrivacyNotice() const;

  // Updates the privacy notice's accepted pref.
  void SetPrivacyNoticeAcceptedPref();

  // Whether the privacy notice is accepted by the user.
  bool IsPrivacyNoticeAccepted() const;

  // Updates the current notifier visibility state in prefs to determine if a
  // notifier should be showing.
  void UpdateNotifierVisibility(bool visible);

  bool is_visible() const { return is_visible_; }

 private:
  // Records if the notifier is visible.
  bool is_visible_ = false;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_NOTIFIER_CONTROLLER_H_

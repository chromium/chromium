// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_STATE_H_
#define CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_STATE_H_

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class Profile;

class QuietNotificationPermissionUiState {
 public:
  // Defines the method by which the quiet UI was enabled.
  enum class EnablingMethod {
    // The quiet UI is either not enabled, or was enabled in M87 or earlier,
    // which did not keep track of the enabling method. The latter situation
    // should be self-correcting as the
    // AdaptiveQuietNotificationPermissionUiEnabler will backfill these values
    // shortly after start-up.
    kUnspecified = 0,

    // The user manually selected the quiet UI in settings.
    kManual,

    // The adaptive activation mechanism in
    // AdaptiveQuietNotificationPermissionUiEnabler has enabled the quiet UI.
    kAdaptive,
  };

  // Register Profile-keyed preferences used for permission UI selection.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Whether to show a promo for the prompt indicator.
  static bool ShouldShowPromo(Profile* profile);

  // Records that the promo was shown.
  static void PromoWasShown(Profile* profile);

  // Returns the method that was used to enable the quiet UI.
  static EnablingMethod GetQuietUiEnablingMethod(Profile* profile);
};

#endif  // CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_STATE_H_:b

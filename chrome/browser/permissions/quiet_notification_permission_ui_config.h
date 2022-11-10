// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_CONFIG_H_
#define CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_CONFIG_H_

#include "build/build_config.h"

namespace base {
class TimeDelta;
}

// Field trial configuration for the quiet notification permission request UI.
class QuietNotificationPermissionUiConfig {
 public:
  enum class InfobarLinkTextVariation { kDetails = 0, kManage = 1 };

  // Name of the boolean variation parameter that determines if the quiet
  // notification permission prompt UI should be enabled adaptively after three
  // consecutive prompt denies.
  static const char kEnableAdaptiveActivation[];

  // Name of the boolean variation parameter that determines if the adaptive
  // activation quiet UI dry run study is enabled.
  static const char kEnableAdaptiveActivationDryRun[];

  // Name of the integer variation parameter that determines history windows
  // size in days in which 3 consecutive denies should be monitored.
  static const char kAdaptiveActivationActionWindowSizeInDays[];

  // Name of the boolean variation parameter that determines if the quiet
  // notification permission prompt UI should be enabled as a one-off based on
  // crowd deny data, that is, on sites with a low notification permission grant
  // rate.
  static const char kEnableCrowdDenyTriggering[];

  // Name of the boolean variation parameter that determines if the quiet
  // notification permission prompt UI should be enabled as a one-off on sites
  // with abusive permission request flows.
  static const char kEnableAbusiveRequestBlocking[];

  // Name of the boolean variation parameter that determines if a console
  // message in Developer Tools should be printed on sites that are on the
  // warning list for abusive permission request flows.
  static const char kEnableAbusiveRequestWarning[];

  // Name of the boolean variation parameter that determines if the quiet
  // notification permission prompt UI should be enabled as a one-off on sites
  // that are on the blocking list for showing abusive notification content.
  static const char kEnableAbusiveContentTriggeredRequestBlocking[];

  // Name of the boolean variation parameter that determines if a console
  // message in Developer Tools should be printed on sites that are on the
  // warning list for showing abusive notification content.
  static const char kEnableAbusiveContentTriggeredRequestWarning[];

  // Name of the variation parameter that represents the chance that a
  // quiet notifications permission prompt UI triggered by crowd deny will be
  // replaced by the normal UI. This ensures that a small percentage of
  // permission requests still use the normal UI and allows sites to recover
  // from a bad reputation score. Represented as a number in the [0,1] interval.
  // If the quiet UI is enabled in preferences, the quiet UI is always used.
  static const char kCrowdDenyHoldBackChance[];

  // Name of the variation parameter that determines which experimental string
  // to use for the link in the mini infobar in Android, which upon being
  // clicked, expands the mini infobar to show more options.
  static const char kMiniInfobarExpandLinkText[];

  // Whether or not adaptive activation is enabled. Adaptive activation means
  // that quiet notifications permission prompts will be turned on after three
  // consecutive prompt denies.
  static bool IsAdaptiveActivationEnabled();

  // Whether or not adaptive activation dry run is enabled. Adaptive activation
  // dry run means that UKM `Permission` events will be annotated with the
  // `SatisfiedAdaptiveTriggers` metric indicating whether the user had three
  // consecutive prompt denies.
  static bool IsAdaptiveActivationDryRunEnabled();

  // How long the window extends into the past, in which the user needs to make
  // 3 consecutive permission denies.
  static base::TimeDelta GetAdaptiveActivationWindowSize();

  // Whether or not triggering via crowd deny is enabled. This means that on
  // sites with a low notification permission grant rate, the quiet UI will be
  // shown as a one-off, even when it is not turned on for all sites in prefs.
  static bool IsCrowdDenyTriggeringEnabled();

  // The chance that a quiet notifications permission prompt UI triggered by
  // crowd deny will be replaced by the normal UI. This is per individual
  // permission prompt.
  static double GetCrowdDenyHoldBackChance();

  // The text of the link to be shown in the mini infobar in Android.
  static InfobarLinkTextVariation GetMiniInfobarExpandLinkText();

  // Whether or not triggering via the abusive requests list is enabled. This
  // means that on sites with abusive permission request flows, the quiet UI
  // will be shown as a one-off, even when it is not turned on for all sites in
  // prefs.
  static bool IsAbusiveRequestBlockingEnabled();

  // Whether or not showing a console message in Developer Tools is enabled for
  // sites on the abusive requests warning list.
  static bool IsAbusiveRequestWarningEnabled();

  // Whether or not triggering via the abusive notification content list is
  // enabled. This means that on sites with abusive notification content, the
  // quiet UI will be shown as a one-off, even when it is not turned on for all
  // sites in prefs.
  static bool IsAbusiveContentTriggeredRequestBlockingEnabled();

  // Whether or not showing a console message in Developer Tools is enabled for
  // sites on the abusive content warning list.
  static bool IsAbusiveContentTriggeredRequestWarningEnabled();

  // Whether or not triggering via the disruptive behavior list is enabled. This
  // means that on sites with disruptive behavior permission request flows, the
  // quiet UI will be shown as a one-off, even when it is not turned on for all
  // sites in prefs.
  static bool IsDisruptiveBehaviorRequestBlockingEnabled();
};

#endif  // CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_CONFIG_H_

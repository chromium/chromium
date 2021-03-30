// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APPS_METRICS_INTENT_HANDLING_METRICS_H_
#define CHROME_BROWSER_ASH_APPS_METRICS_INTENT_HANDLING_METRICS_H_

#include <string>
#include <utility>

#include "chrome/browser/apps/intent_helper/apps_navigation_throttle.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/ash/arc/intent_helper/arc_external_protocol_dialog.h"
#include "components/arc/metrics/arc_metrics_constants.h"

namespace apps {

class IntentHandlingMetrics {
 public:
  // The type of app the link came from, used for intent handling metrics.
  // This enum is used for recording histograms, and must be treated as
  // append-only.
  enum class AppType {
    kArc = 0,  // From an Android app
    kWeb,      // From a web app
    kMaxValue = kWeb,
  };

  // These enums are used to define the buckets for an enumerated UMA histogram
  // and need to be synced with the ArcIntentHandlerAction enum in enums.xml.
  // This enum class should also be treated as append-only.
  enum class PickerAction : int {
    // Picker errors occurring after the picker is shown.
    ERROR_AFTER_PICKER = 0,
    // DIALOG_DEACTIVATED keeps track of the user dismissing the UI via clicking
    // the close button or clicking outside of the IntentPickerBubbleView
    // surface. As with CHROME_PRESSED, the user stays in Chrome, however we
    // keep both options since CHROME_PRESSED is tied to an explicit intent of
    // staying in Chrome, not only just getting rid of the
    // IntentPickerBubbleView UI.
    DIALOG_DEACTIVATED = 1,
    OBSOLETE_ALWAYS_PRESSED = 2,
    OBSOLETE_JUST_ONCE_PRESSED = 3,
    PREFERRED_ARC_ACTIVITY_FOUND = 4,
    // The prefix "CHROME"/"ARC_APP"/"PWA_APP" determines whether the user
    // pressed [Stay in Chrome] or [Use app] at IntentPickerBubbleView.
    // "PREFERRED" denotes when the user decides to save this selection, whether
    // an app or Chrome was selected.
    CHROME_PRESSED = 5,
    CHROME_PREFERRED_PRESSED = 6,
    ARC_APP_PRESSED = 7,
    ARC_APP_PREFERRED_PRESSED = 8,
    PWA_APP_PRESSED = 9,
    // Picker errors occurring before the picker is shown.
    ERROR_BEFORE_PICKER = 10,
    INVALID = 11,
    DEVICE_PRESSED = 12,
    MAC_OS_APP_PRESSED = 13,
    PWA_APP_PREFERRED_PRESSED = 14,
    PREFERRED_PWA_FOUND = 15,
    PREFERRED_CHROME_BROWSER_FOUND = 16,
    kMaxValue = PREFERRED_CHROME_BROWSER_FOUND,
  };

  // As for PickerAction, these define the buckets for an UMA histogram, so this
  // must be treated in an append-only fashion. This helps specify where a
  // navigation will continue. Must be kept in sync with the
  // ArcIntentHandlerDestinationPlatform enum in enums.xml.
  enum class Platform : int {
    ARC = 0,
    CHROME = 1,
    PWA = 2,
    DEVICE = 3,
    MAC_OS = 4,
    kMaxValue = MAC_OS,
  };

  // TODO(ajlinker): move these two functions below to IntentHandlingMetrics.
  // Determines the destination of the current navigation. We know that if the
  // |picker_action| is either ERROR or DIALOG_DEACTIVATED the navigation MUST
  // stay in Chrome, and when |picker_action| is PWA_APP_PRESSED the navigation
  // goes to a PWA. Otherwise we can assume the navigation goes to ARC with the
  // exception of the |selected_launch_name| being Chrome.
  static Platform GetDestinationPlatform(
      const std::string& selected_launch_name,
      PickerAction picker_action);

  // Converts the provided |entry_type|, |close_reason| and |should_persist|
  // boolean to a PickerAction value for recording in UMA.
  static PickerAction GetPickerAction(PickerEntryType entry_type,
                                      IntentPickerCloseReason close_reason,
                                      bool should_persist);

  IntentHandlingMetrics();
  static void RecordIntentPickerMetrics(Source source,
                                        bool should_persist,
                                        PickerAction action,
                                        Platform platform);

  static void RecordIntentPickerUserInteractionMetrics(
      const std::string& selected_app_package,
      PickerEntryType entry_type,
      IntentPickerCloseReason close_reason,
      Source source,
      bool should_persist);

  static void RecordExternalProtocolMetrics(arc::Scheme scheme,
                                            apps::PickerEntryType entry_type,
                                            bool accepted,
                                            bool persisted);

  static void RecordOpenBrowserMetrics(AppType type);
};

}  // namespace apps

#endif  // CHROME_BROWSER_ASH_APPS_METRICS_INTENT_HANDLING_METRICS_H_

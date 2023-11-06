// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_METRICS_INTENT_HANDLING_METRICS_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_METRICS_INTENT_HANDLING_METRICS_H_

#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/link_capturing/intent_picker_info.h"

namespace content {
class BrowserContext;
}  // namespace content

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

  // An action taken by the user in the HTTP/HTTPS Intent Picker dialog.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class IntentPickerAction {
    kInvalid = 0,
    kError = 1,
    kDialogDeactivated = 2,
    kChromeSelected = 3,
    kChromeSelectedAndPreferred = 4,
    kArcAppSelected = 5,
    kArcAppSelectedAndPreferred = 6,
    kPwaSelected = 7,
    kPwaSelectedAndPreferred = 8,
    kMaxValue = kPwaSelectedAndPreferred
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

  // These are the events that occur in the link capturing flow.
  enum class LinkCapturingEvent {
    // An entry point for the link capturing flow was shown, in the form of the
    // Intent Chip or Intent Picker.
    kEntryPointShown = 0,
    // The link was captured (opened) in an available app.
    kAppOpened = 1,
    // The user accepted the option to automatically open similar links in the
    // future with this same app selection.
    kSettingsChanged = 2,
    kMaxValue = kSettingsChanged,
  };

  IntentHandlingMetrics();

  // Records metrics for the outcome of a user selection in the http/https
  // Intent Picker UI. |entry_type| is the type of the selected app,
  // |close_reason| is the reason why the bubble closed, and |should_persist| is
  // whether the persistence checkbox was checked.
  static void RecordIntentPickerMetrics(PickerEntryType entry_type,
                                        IntentPickerCloseReason close_reason,
                                        bool should_persist);

  // Records metrics for when a link is clicked which can handle a preferred
  // app, as the result of a user previously setting a preference for that app.
  static void RecordPreferredAppLinkClickMetrics(Platform platform);

  // Records metrics for when an entry point is shown for the link capturing
  // flow. An entry point can be the Intent Chip or Intent Picker.
  static void RecordLinkCapturingEntryPointShown(
      const std::vector<IntentPickerAppInfo>& app_infos);

  // Records metrics for link capturing flow events, including when an app is
  // opened and when settings are saved.
  static void RecordLinkCapturingEvent(PickerEntryType app_type,
                                       LinkCapturingEvent event);

#if BUILDFLAG(IS_CHROMEOS_ASH)

  static void RecordExternalProtocolUserInteractionMetrics(
      content::BrowserContext* context,
      PickerEntryType entry_type,
      IntentPickerCloseReason close_reason,
      bool should_persist);

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_METRICS_INTENT_HANDLING_METRICS_H_

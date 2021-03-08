// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/apps/metrics/intent_handling_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "components/arc/metrics/arc_metrics_constants.h"

namespace apps {

IntentHandlingMetrics::IntentHandlingMetrics() {}

void IntentHandlingMetrics::RecordIntentPickerMetrics(Source source,
                                                      bool should_persist,
                                                      PickerAction action,
                                                      Platform platform) {
  // TODO(crbug.com/985233) For now External Protocol Dialog is only querying
  // ARC apps.
  if (source == Source::kExternalProtocol) {
    UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.ExternalProtocolDialog", action);
  } else {
    UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.IntentPickerAction", action);

    UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.IntentPickerDestinationPlatform",
                              platform);
  }
}

void IntentHandlingMetrics::RecordIntentPickerUserInteractionMetrics(
    const std::string& selected_app_package,
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    Source source,
    bool should_persist) {
  if (entry_type == PickerEntryType::kArc &&
      (close_reason == IntentPickerCloseReason::PREFERRED_APP_FOUND ||
       close_reason == IntentPickerCloseReason::OPEN_APP)) {
    UMA_HISTOGRAM_ENUMERATION("Arc.UserInteraction",
                              arc::UserInteractionType::APP_STARTED_FROM_LINK);
  }
  PickerAction action =
      GetPickerAction(entry_type, close_reason, should_persist);
  Platform platform = GetDestinationPlatform(selected_app_package, action);
  RecordIntentPickerMetrics(source, should_persist, action, platform);
}

void IntentHandlingMetrics::RecordExternalProtocolMetrics(
    arc::Scheme scheme,
    PickerEntryType entry_type,
    bool accepted,
    bool persisted) {
  arc::ProtocolAction action =
      arc::GetProtocolAction(scheme, entry_type, accepted, persisted);
  if (accepted) {
    UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.ExternalProtocolDialog.Accepted",
                              action);
  } else {
    UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.ExternalProtocolDialog.Rejected",
                              action);
  }
}

void IntentHandlingMetrics::RecordOpenBrowserMetrics(AppType type) {
  UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.OpenBrowser", type);
}

// static
IntentHandlingMetrics::Platform IntentHandlingMetrics::GetDestinationPlatform(
    const std::string& selected_launch_name,
    PickerAction picker_action) {
  switch (picker_action) {
    case PickerAction::ARC_APP_PRESSED:
    case PickerAction::ARC_APP_PREFERRED_PRESSED:
    case PickerAction::PREFERRED_ARC_ACTIVITY_FOUND:
      return Platform::ARC;
    case PickerAction::PWA_APP_PRESSED:
    case PickerAction::PWA_APP_PREFERRED_PRESSED:
    case PickerAction::PREFERRED_PWA_FOUND:
      return Platform::PWA;
    case PickerAction::MAC_OS_APP_PRESSED:
      return Platform::MAC_OS;
    case PickerAction::ERROR_BEFORE_PICKER:
    case PickerAction::ERROR_AFTER_PICKER:
    case PickerAction::DIALOG_DEACTIVATED:
    case PickerAction::CHROME_PRESSED:
    case PickerAction::CHROME_PREFERRED_PRESSED:
    case PickerAction::PREFERRED_CHROME_BROWSER_FOUND:
      return Platform::CHROME;
    case PickerAction::DEVICE_PRESSED:
      return Platform::DEVICE;
    case PickerAction::OBSOLETE_ALWAYS_PRESSED:
    case PickerAction::OBSOLETE_JUST_ONCE_PRESSED:
    case PickerAction::INVALID:
      break;
  }
  NOTREACHED();
  return Platform::ARC;
}

// static
IntentHandlingMetrics::PickerAction IntentHandlingMetrics::GetPickerAction(
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    bool should_persist) {
  switch (close_reason) {
    case IntentPickerCloseReason::ERROR_BEFORE_PICKER:
      return PickerAction::ERROR_BEFORE_PICKER;
    case IntentPickerCloseReason::ERROR_AFTER_PICKER:
      return PickerAction::ERROR_AFTER_PICKER;
    case IntentPickerCloseReason::DIALOG_DEACTIVATED:
      return PickerAction::DIALOG_DEACTIVATED;
    case IntentPickerCloseReason::PREFERRED_APP_FOUND:
      switch (entry_type) {
        case PickerEntryType::kUnknown:
          return PickerAction::PREFERRED_CHROME_BROWSER_FOUND;
        case PickerEntryType::kArc:
          return PickerAction::PREFERRED_ARC_ACTIVITY_FOUND;
        case PickerEntryType::kWeb:
          return PickerAction::PREFERRED_PWA_FOUND;
        case PickerEntryType::kDevice:
        case PickerEntryType::kMacOs:
          NOTREACHED();
          return PickerAction::INVALID;
      }
    case IntentPickerCloseReason::STAY_IN_CHROME:
      return should_persist ? PickerAction::CHROME_PREFERRED_PRESSED
                            : PickerAction::CHROME_PRESSED;
    case IntentPickerCloseReason::OPEN_APP:
      switch (entry_type) {
        case PickerEntryType::kUnknown:
          NOTREACHED();
          return PickerAction::INVALID;
        case PickerEntryType::kArc:
          return should_persist ? PickerAction::ARC_APP_PREFERRED_PRESSED
                                : PickerAction::ARC_APP_PRESSED;
        case PickerEntryType::kWeb:
          return should_persist ? PickerAction::PWA_APP_PREFERRED_PRESSED
                                : PickerAction::PWA_APP_PRESSED;
        case PickerEntryType::kDevice:
          return PickerAction::DEVICE_PRESSED;
        case PickerEntryType::kMacOs:
          return PickerAction::MAC_OS_APP_PRESSED;
      }
  }

  NOTREACHED();
  return PickerAction::INVALID;
}

}  // namespace apps

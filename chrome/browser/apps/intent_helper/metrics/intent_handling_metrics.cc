// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/metrics/intent_handling_metrics.h"

#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "chrome/browser/apps/intent_helper/chromeos_intent_picker_helpers.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/metrics/arc_metrics_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

const char kLinkCapturingHistogram[] = "ChromeOS.Intents.LinkCapturingEvent2";
const char kLinkCapturingHistogramWeb[] =
    "ChromeOS.Intents.LinkCapturingEvent2.WebApp";
const char kLinkCapturingHistogramArc[] =
    "ChromeOS.Intents.LinkCapturingEvent2.ArcApp";

using PickerAction = apps::IntentHandlingMetrics::PickerAction;
using IntentPickerAction = apps::IntentHandlingMetrics::IntentPickerAction;
using Platform = apps::IntentHandlingMetrics::Platform;

void RecordDestinationPlatformMetric(
    apps::IntentHandlingMetrics::Platform platform) {
  UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.IntentPickerDestinationPlatform",
                            platform);
}

IntentPickerAction GetIntentPickerAction(
    apps::PickerEntryType entry_type,
    apps::IntentPickerCloseReason close_reason,
    bool should_persist) {
  switch (close_reason) {
    case apps::IntentPickerCloseReason::ERROR_BEFORE_PICKER:
    case apps::IntentPickerCloseReason::ERROR_AFTER_PICKER:
      return IntentPickerAction::kError;
    case apps::IntentPickerCloseReason::DIALOG_DEACTIVATED:
      return IntentPickerAction::kDialogDeactivated;
    case apps::IntentPickerCloseReason::STAY_IN_CHROME:
      return should_persist ? IntentPickerAction::kChromeSelectedAndPreferred
                            : IntentPickerAction::kChromeSelected;
    case apps::IntentPickerCloseReason::OPEN_APP:
      switch (entry_type) {
        case apps::PickerEntryType::kArc:
          return should_persist
                     ? IntentPickerAction::kArcAppSelectedAndPreferred
                     : IntentPickerAction::kArcAppSelected;
        case apps::PickerEntryType::kWeb:
          return should_persist ? IntentPickerAction::kPwaSelectedAndPreferred
                                : IntentPickerAction::kPwaSelected;
        case apps::PickerEntryType::kDevice:
        case apps::PickerEntryType::kMacOs:
        case apps::PickerEntryType::kUnknown:
          NOTREACHED();
          return IntentPickerAction::kInvalid;
      }
    case apps::IntentPickerCloseReason::PREFERRED_APP_FOUND:
      // For the HTTP/HTTPS Intent Picker, preferred app metrics are recorded
      // separately in RecordPreferredAppLinkClickMetrics.
      NOTREACHED();
      return IntentPickerAction::kInvalid;
  }
}

Platform GetIntentPickerDestinationPlatform(IntentPickerAction action) {
  switch (action) {
    case IntentPickerAction::kArcAppSelected:
    case IntentPickerAction::kArcAppSelectedAndPreferred:
      return Platform::ARC;
    case IntentPickerAction::kPwaSelected:
    case IntentPickerAction::kPwaSelectedAndPreferred:
      return Platform::PWA;
    case IntentPickerAction::kChromeSelected:
    case IntentPickerAction::kChromeSelectedAndPreferred:
    case IntentPickerAction::kDialogDeactivated:
    case IntentPickerAction::kError:
      return Platform::CHROME;
    case IntentPickerAction::kInvalid:
      NOTREACHED();
      return Platform::CHROME;
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Converts the provided |entry_type|, |close_reason| and |should_persist|
// boolean to a PickerAction value for recording in UMA.
PickerAction GetExternalProtocolPickerAction(
    apps::PickerEntryType entry_type,
    apps::IntentPickerCloseReason close_reason,
    bool should_persist) {
  switch (close_reason) {
    case apps::IntentPickerCloseReason::ERROR_BEFORE_PICKER:
      return PickerAction::ERROR_BEFORE_PICKER;
    case apps::IntentPickerCloseReason::ERROR_AFTER_PICKER:
      return PickerAction::ERROR_AFTER_PICKER;
    case apps::IntentPickerCloseReason::DIALOG_DEACTIVATED:
      return PickerAction::DIALOG_DEACTIVATED;
    case apps::IntentPickerCloseReason::PREFERRED_APP_FOUND:
      switch (entry_type) {
        case apps::PickerEntryType::kUnknown:
          return PickerAction::PREFERRED_CHROME_BROWSER_FOUND;
        case apps::PickerEntryType::kArc:
          return PickerAction::PREFERRED_ARC_ACTIVITY_FOUND;
        case apps::PickerEntryType::kWeb:
          return PickerAction::PREFERRED_PWA_FOUND;
        case apps::PickerEntryType::kDevice:
        case apps::PickerEntryType::kMacOs:
          NOTREACHED();
          return PickerAction::INVALID;
      }
    case apps::IntentPickerCloseReason::STAY_IN_CHROME:
      return should_persist ? PickerAction::CHROME_PREFERRED_PRESSED
                            : PickerAction::CHROME_PRESSED;
    case apps::IntentPickerCloseReason::OPEN_APP:
      switch (entry_type) {
        case apps::PickerEntryType::kUnknown:
          NOTREACHED();
          return PickerAction::INVALID;
        case apps::PickerEntryType::kArc:
          return should_persist ? PickerAction::ARC_APP_PREFERRED_PRESSED
                                : PickerAction::ARC_APP_PRESSED;
        case apps::PickerEntryType::kWeb:
          return should_persist ? PickerAction::PWA_APP_PREFERRED_PRESSED
                                : PickerAction::PWA_APP_PRESSED;
        case apps::PickerEntryType::kDevice:
          return PickerAction::DEVICE_PRESSED;
        case apps::PickerEntryType::kMacOs:
          return PickerAction::MAC_OS_APP_PRESSED;
      }
  }

  NOTREACHED();
  return PickerAction::INVALID;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

namespace apps {

IntentHandlingMetrics::IntentHandlingMetrics() = default;

void IntentHandlingMetrics::RecordIntentPickerMetrics(
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    bool should_persist,
    PickerShowState show_state) {
  IntentPickerAction action =
      GetIntentPickerAction(entry_type, close_reason, should_persist);
  Platform platform = GetIntentPickerDestinationPlatform(action);

  UMA_HISTOGRAM_ENUMERATION("ChromeOS.Intents.IntentPickerAction", action);
  switch (show_state) {
    case PickerShowState::kOmnibox:
      UMA_HISTOGRAM_ENUMERATION(
          "ChromeOS.Intents.IntentPickerAction.FromOmniboxIcon", action);
      break;
    case PickerShowState::kPopOut:
      UMA_HISTOGRAM_ENUMERATION(
          "ChromeOS.Intents.IntentPickerAction.FromAutoPopOut", action);
      break;
  }

  RecordDestinationPlatformMetric(platform);
}

void IntentHandlingMetrics::RecordPreferredAppLinkClickMetrics(
    Platform platform) {
  RecordDestinationPlatformMetric(platform);
}

void IntentHandlingMetrics::RecordIntentPickerIconEvent(
    IntentPickerIconEvent event) {
  UMA_HISTOGRAM_ENUMERATION("ChromeOS.Intents.IntentPickerIconEvent", event);
}

void IntentHandlingMetrics::RecordLinkCapturingEvent(PickerEntryType app_type,
                                                     LinkCapturingEvent event) {
  switch (app_type) {
    case PickerEntryType::kWeb:
      base::UmaHistogramEnumeration(kLinkCapturingHistogramWeb, event);
      break;
    case PickerEntryType::kArc:
      base::UmaHistogramEnumeration(kLinkCapturingHistogramArc, event);
      break;
    case PickerEntryType::kUnknown:
    case PickerEntryType::kDevice:
    case PickerEntryType::kMacOs:
      // These cases do not represent entering an app and should not record
      // any histograms.
      return;
  }
  base::UmaHistogramEnumeration(kLinkCapturingHistogram, event);
}

void IntentHandlingMetrics::RecordLinkCapturingEntryPointShown(
    const std::vector<IntentPickerAppInfo>& app_infos) {
  if (base::Contains(app_infos, PickerEntryType::kWeb,
                     &IntentPickerAppInfo::type)) {
    base::UmaHistogramEnumeration(kLinkCapturingHistogramWeb,
                                  LinkCapturingEvent::kEntryPointShown);
  }
  if (base::Contains(app_infos, PickerEntryType::kArc,
                     &IntentPickerAppInfo::type)) {
    base::UmaHistogramEnumeration(kLinkCapturingHistogramArc,
                                  LinkCapturingEvent::kEntryPointShown);
  }
  base::UmaHistogramEnumeration(kLinkCapturingHistogram,
                                LinkCapturingEvent::kEntryPointShown);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

void IntentHandlingMetrics::RecordExternalProtocolUserInteractionMetrics(
    content::BrowserContext* context,
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    bool should_persist) {
  if (entry_type == PickerEntryType::kArc &&
      (close_reason == IntentPickerCloseReason::PREFERRED_APP_FOUND ||
       close_reason == IntentPickerCloseReason::OPEN_APP)) {
    arc::ArcMetricsService::RecordArcUserInteraction(
        context, arc::UserInteractionType::APP_STARTED_FROM_LINK);
  }

  // TODO(crbug.com/985233) For now External Protocol Dialog is only querying
  // ARC apps, so there's no need to record a destination platform.
  PickerAction action =
      GetExternalProtocolPickerAction(entry_type, close_reason, should_persist);
  UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.ExternalProtocolDialog", action);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void IntentHandlingMetrics::RecordOpenBrowserMetrics(AppType type) {
  UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.OpenBrowser", type);
}

}  // namespace apps

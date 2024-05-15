// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/metrics/intent_handling_metrics.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/metrics/arc_metrics_constants.h"
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
          NOTREACHED_IN_MIGRATION();
          return IntentPickerAction::kInvalid;
      }
    case apps::IntentPickerCloseReason::PREFERRED_APP_FOUND:
      // For the HTTP/HTTPS Intent Picker, preferred app metrics are recorded
      // separately in RecordPreferredAppLinkClickMetrics.
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
      return Platform::CHROME;
  }
}

}  // namespace

namespace apps {

IntentHandlingMetrics::IntentHandlingMetrics() = default;

void IntentHandlingMetrics::RecordIntentPickerMetrics(
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    bool should_persist) {
  IntentPickerAction action =
      GetIntentPickerAction(entry_type, close_reason, should_persist);
  UMA_HISTOGRAM_ENUMERATION("ChromeOS.Intents.IntentPickerAction", action);

  Platform platform = GetIntentPickerDestinationPlatform(action);
  RecordDestinationPlatformMetric(platform);
}

void IntentHandlingMetrics::RecordPreferredAppLinkClickMetrics(
    Platform platform) {
  RecordDestinationPlatformMetric(platform);
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
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps

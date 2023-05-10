// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_METRICS_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_METRICS_UTILS_H_

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/enterprise/connectors/reporting/reporting_service_settings.h"

namespace enterprise_connectors {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep this enum in sync with
// EnterpriseReportingEventType in enums.xml.
enum class EnterpriseReportingEventType {
  kUnknownEvent = 0,
  kPasswordReuseEvent = 1,
  kPasswordChangedEvent = 2,
  kDangerousDownloadEvent = 3,
  kInterstitialEvent = 4,
  kSensitiveDataEvent = 5,
  kUnscannedFileEvent = 6,
  kLoginEvent = 7,
  kPasswordBreachEvent = 8,
  kUrlFilteringInterstitialEvent = 9,
  kExtensionInstallEvent = 10,
  kBrowserCrashEvent = 11,
  kMaxValue = kBrowserCrashEvent,
};

// Mapping from event name to UMA enum for logging histogram.
constexpr auto kEventNameToUmaEnumMap =
    base::MakeFixedFlatMap<base::StringPiece, EnterpriseReportingEventType>({
        {extensions::SafeBrowsingPrivateEventRouter::kKeyPasswordReuseEvent,
         EnterpriseReportingEventType::kPasswordReuseEvent},
        {extensions::SafeBrowsingPrivateEventRouter::kKeyPasswordChangedEvent,
         EnterpriseReportingEventType::kPasswordChangedEvent},
        {extensions::SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent,
         EnterpriseReportingEventType::kDangerousDownloadEvent},
        {extensions::SafeBrowsingPrivateEventRouter::kKeyInterstitialEvent,
         EnterpriseReportingEventType::kInterstitialEvent},
        {extensions::SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent,
         EnterpriseReportingEventType::kSensitiveDataEvent},
        {extensions::SafeBrowsingPrivateEventRouter::kKeyUnscannedFileEvent,
         EnterpriseReportingEventType::kUnscannedFileEvent},
        {extensions::SafeBrowsingPrivateEventRouter::kKeyLoginEvent,
         EnterpriseReportingEventType::kLoginEvent},
        {extensions::SafeBrowsingPrivateEventRouter::kKeyPasswordBreachEvent,
         EnterpriseReportingEventType::kPasswordBreachEvent},
        {extensions::SafeBrowsingPrivateEventRouter::
             kKeyUrlFilteringInterstitialEvent,
         EnterpriseReportingEventType::kUrlFilteringInterstitialEvent},
        {ReportingServiceSettings::kExtensionInstallEvent,
         EnterpriseReportingEventType::kExtensionInstallEvent},
        {ReportingServiceSettings::kBrowserCrashEvent,
         EnterpriseReportingEventType::kBrowserCrashEvent},
    });

// Return the UMA EnterpriseReportingEventType enum for the given event name.
EnterpriseReportingEventType GetUmaEnumFromEventName(
    const base::StringPiece& eventName);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_METRICS_UTILS_H_

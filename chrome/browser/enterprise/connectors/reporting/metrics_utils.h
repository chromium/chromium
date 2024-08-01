// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_METRICS_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_METRICS_UTILS_H_

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"

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
  kExtensionTelemetryEvent = 12,
  kMaxValue = kExtensionTelemetryEvent,
};

// Mapping from event name to UMA enum for logging histogram.
constexpr auto kEventNameToUmaEnumMap =
    base::MakeFixedFlatMap<std::string_view, EnterpriseReportingEventType>({
        {kKeyPasswordReuseEvent,
         EnterpriseReportingEventType::kPasswordReuseEvent},
        {kKeyPasswordChangedEvent,
         EnterpriseReportingEventType::kPasswordChangedEvent},
        {kKeyDangerousDownloadEvent,
         EnterpriseReportingEventType::kDangerousDownloadEvent},
        {kKeyInterstitialEvent,
         EnterpriseReportingEventType::kInterstitialEvent},
        {kKeySensitiveDataEvent,
         EnterpriseReportingEventType::kSensitiveDataEvent},
        {kKeyUnscannedFileEvent,
         EnterpriseReportingEventType::kUnscannedFileEvent},
        {kKeyLoginEvent, EnterpriseReportingEventType::kLoginEvent},
        {kKeyPasswordBreachEvent,
         EnterpriseReportingEventType::kPasswordBreachEvent},
        {kKeyUrlFilteringInterstitialEvent,
         EnterpriseReportingEventType::kUrlFilteringInterstitialEvent},
        {kExtensionInstallEvent,
         EnterpriseReportingEventType::kExtensionInstallEvent},
        {kBrowserCrashEvent, EnterpriseReportingEventType::kBrowserCrashEvent},
        {kExtensionTelemetryEvent,
         EnterpriseReportingEventType::kExtensionTelemetryEvent},
    });

// Return the UMA EnterpriseReportingEventType enum for the given event name.
EnterpriseReportingEventType GetUmaEnumFromEventName(
    std::string_view eventName);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_METRICS_UTILS_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_DEFAULT_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_DEFAULT_UTILS_H_

#include "base/time/time.h"

namespace reporting::metrics {

// IMPORTANT: If you are updating any of the values in this file please
// make sure you keep it in sync with the comments in the proto:
// components/reporting/proto/synced/metric_data.proto

// Default app telemetry collection rate.
inline constexpr base::TimeDelta kDefaultAppUsageTelemetryCollectionRate =
    base::Minutes(15);

// Default audio telemetry collection rate.
inline constexpr base::TimeDelta kDefaultAudioTelemetryCollectionRate =
    base::Minutes(15);

// Default runtime counters telemetry collection rate.
inline constexpr base::TimeDelta
    kDefaultRuntimeCountersTelemetryCollectionRate = base::Days(1);

// Default metric collection rate used for testing purposes.
inline constexpr base::TimeDelta kDefaultCollectionRateForTesting =
    base::Minutes(2);

// Default device activity heartbeat collection rate.
inline constexpr base::TimeDelta kDefaultDeviceActivityHeartbeatCollectionRate =
    base::Minutes(15);

// Default Kiosk Heartbeat activity collection rate
inline constexpr base::TimeDelta kDefaultHeartbeatTelemetryCollectionRate =
    base::Minutes(2);

// Default Kiosk Vision telemetry collection rate
inline constexpr base::TimeDelta kDefaultKioskVisionTelemetryCollectionRate =
    base::Minutes(2);

// Default event checking rate for testing purposes.
inline constexpr base::TimeDelta kDefaultEventCheckingRateForTesting =
    base::Minutes(1);

// Default network telemetry collection rate.
inline constexpr base::TimeDelta kDefaultNetworkTelemetryCollectionRate =
    base::Minutes(60);

// Default network telemetry event checking rate.
inline constexpr base::TimeDelta kDefaultNetworkTelemetryEventCheckingRate =
    base::Minutes(10);

// Default record upload frequency.
inline constexpr base::TimeDelta kDefaultReportUploadFrequency = base::Hours(3);

// Default record upload frequency used for testing purposes.
inline constexpr base::TimeDelta kDefaultReportUploadFrequencyForTesting =
    base::Minutes(5);

// Default record upload frequency for KioskHeartbeats.
inline constexpr base::TimeDelta kDefaultKioskHeartbeatUploadFrequency =
    base::Minutes(2);

// Default record upload frequency for KioskHeartbeats.
inline constexpr base::TimeDelta
    kDefaultKioskHeartbeatUploadFrequencyForTesting = base::Minutes(1);

// Default website telemetry collection rate.
inline constexpr base::TimeDelta kDefaultWebsiteTelemetryCollectionRate =
    base::Minutes(15);

// Initial metric reporting collection delay.
inline constexpr base::TimeDelta kInitialCollectionDelay = base::Minutes(1);

// Peripheral collection delay to mitigate the race
// condition where CrosHealthD may query fwupd before it has a chance to read
// all of the USB devices that are plugged into the machine.
inline constexpr base::TimeDelta kPeripheralCollectionDelay = base::Seconds(5);

// Initial metric reporting upload delay.
inline constexpr base::TimeDelta kInitialUploadDelay = base::Minutes(3);

// Minimum usage time threshold for app usage reporting.
inline constexpr base::TimeDelta kMinimumAppUsageTime = base::Milliseconds(1);

// Minimum usage time threshold for website usage reporting.
inline constexpr base::TimeDelta kMinimumWebsiteUsageTime =
    base::Milliseconds(1);

// App event reporting rate limiter configuration.
inline constexpr size_t kAppEventsTotalSize = 4u * 1024u * 1024u;  // 4 MiB
inline constexpr base::TimeDelta kAppEventsWindow = base::Seconds(10);
inline constexpr size_t kAppEventsBucketCount = 10u;

// Website event reporting rate limiter configuration.
inline constexpr size_t kWebsiteEventsTotalSize =
    10u * 1024u * 1024u;  // 10 MiB
inline constexpr base::TimeDelta kWebsiteEventsWindow = base::Seconds(10);
inline constexpr size_t kWebsiteEventsBucketCount = 10u;

// Default value that controls app inventory reporting. Set to false even though
// the corresponding user policy is a list type to signify reporting is
// disallowed by default.
inline constexpr bool kReportAppInventoryEnabledDefaultValue = false;

// Default value for reporting device activity heartbeats.
inline constexpr bool kDeviceActivityHeartbeatEnabledDefaultValue = false;

// Default value for reporting device audio status.
inline constexpr bool kReportDeviceAudioStatusDefaultValue = true;

// Default value for reporting device network status.
inline constexpr bool kReportDeviceNetworkStatusDefaultValue = true;

// Default value for reporting device network events.
inline constexpr bool kDeviceReportNetworkEventsDefaultValue = false;

// Default value for reporting device peripheral status.
inline constexpr bool kReportDevicePeripheralsDefaultValue = false;

// Default value for reporting runtime counters.
inline constexpr bool kDeviceReportRuntimeCountersDefaultValue = false;

// Default value for reporting device graphics status.
inline constexpr bool kReportDeviceGraphicsStatusDefaultValue = false;

// Default value for reporting device app info and usage.
inline constexpr bool kReportDeviceAppInfoDefaultValue = false;

// Default value for reporting fatal crashes.
inline constexpr bool kReportDeviceCrashReportInfoDefaultValue = false;

// Default value that controls website activity event reporting. Set to false
// even though the corresponding user policy is an allowlist to signify
// reporting is disabled by default.
inline constexpr bool kReportWebsiteActivityEnabledDefaultValue = false;

// Default value for kHeartbeatTelemetry heartbeats to be sent.
inline constexpr bool kHeartbeatTelemetryDefaultValue = false;

// Default value for kKioskVisionTelemetry data to be sent.
inline constexpr bool kKioskVisionTelemetryDefaultValue = false;

// Returns the default report upload frequency for the current environment.
const base::TimeDelta GetDefaultReportUploadFrequency();

// Returns the default event checking rate for KioskHeartbeats and the current
// environment.
const base::TimeDelta GetDefaultKioskHeartbeatUploadFrequency();

// Returns the default metric collection rate for the current environment.
const base::TimeDelta GetDefaultCollectionRate(base::TimeDelta default_rate);

// Returns the default event checking rate for the current environment.
const base::TimeDelta GetDefaultEventCheckingRate(base::TimeDelta default_rate);

}  // namespace reporting::metrics

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_DEFAULT_UTILS_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_DEFAULT_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_DEFAULT_UTILS_H_

#include "base/time/time.h"

namespace reporting::metrics {

// Default audio telemetry collection rate.
constexpr base::TimeDelta kDefaultAudioTelemetryCollectionRate =
    base::Minutes(15);

// Default metric collection rate used for testing purposes.
constexpr base::TimeDelta kDefaultCollectionRateForTesting = base::Minutes(2);

// Default device activity heartbeat collection rate.
constexpr base::TimeDelta kDefaultDeviceActivityHeartbeatCollectionRate =
    base::Minutes(15);

// Default event checking rate for testing purposes.
constexpr base::TimeDelta kDefaultEventCheckingRateForTesting =
    base::Minutes(1);

// Default network telemetry collection rate.
constexpr base::TimeDelta kDefaultNetworkTelemetryCollectionRate =
    base::Minutes(60);

// Default network telemetry event checking rate.
constexpr base::TimeDelta kDefaultNetworkTelemetryEventCheckingRate =
    base::Minutes(10);

// Default record upload frequency.
constexpr base::TimeDelta kDefaultReportUploadFrequency = base::Hours(3);

// Default record upload frequency used for testing purposes.
constexpr base::TimeDelta kDefaultReportUploadFrequencyForTesting =
    base::Minutes(5);

// Initial metric reporting upload delay.
constexpr base::TimeDelta kInitialUploadDelay = base::Minutes(3);

// Default value for reporting device activity heartbeats.
constexpr bool kDeviceActivityHeartbeatEnabledDefaultValue = false;

// Default value for reporting device audio status.
constexpr bool kReportDeviceAudioStatusDefaultValue = true;

// Default value for reporting device network status.
constexpr bool kReportDeviceNetworkStatusDefaultValue = true;

// Default value for reporting device peripheral status.
constexpr bool kReportDevicePeripheralsDefaultValue = false;

// Default value for reporting device graphics status.
constexpr bool kReportDeviceGraphicsStatusDefaultValue = false;

// Default value for reporting device app info and usage.
constexpr bool kReportDeviceAppInfoDefaultValue = false;

// Returns the default report upload frequency for the current environment.
const base::TimeDelta GetDefaultReportUploadFrequency();

// Returns the default metric collection rate for the current environment.
const base::TimeDelta GetDefaultCollectionRate(base::TimeDelta default_rate);

// Returns the default event checking rate for the current environment.
const base::TimeDelta GetDefaultEventCheckingRate(base::TimeDelta default_rate);

}  // namespace reporting::metrics

// Forward declaration for the friend class below.
namespace ash::reporting {
class CrosHealthdInfoMetricsHelper;
}  // namespace ash::reporting

// Forward declaration for the friend class below.
namespace reporting {
class UsbBrowserTestHelper;
}  // namespace reporting

namespace reporting::metrics {
// Metric reporting manager initialization delay. This is for rate limiting
// in case a device frequently reboots.
class InitDelayParam {
 public:
  static const base::TimeDelta Get();

 private:
  friend class ::ash::reporting::CrosHealthdInfoMetricsHelper;

  static base::TimeDelta init_delay;
  static void SetForTesting(const base::TimeDelta& delay);
};

// Peripheral collection delay to mitigate the race
// condition where CrosHealthD may query fwupd before it has a chance to read
// all of the USB devices that are plugged into the machine.
class PeripheralCollectionDelayParam {
 public:
  static const base::TimeDelta Get();

 private:
  friend class ::reporting::UsbBrowserTestHelper;
  static void SetForTesting(const base::TimeDelta& delay);
  static base::TimeDelta collection_delay_;
};
}  // namespace reporting::metrics

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_DEFAULT_UTILS_H_

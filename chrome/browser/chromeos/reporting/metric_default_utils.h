// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_DEFAULT_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_DEFAULT_UTILS_H_

#include "base/time/time.h"

namespace reporting::metrics {

// Default audio telemetry collection rate.
constexpr base::TimeDelta kDefaultAudioTelemetryCollectionRate =
    base::Minutes(10);

// Default metric collection rate used for testing purposes.
constexpr base::TimeDelta kDefaultCollectionRateForTesting = base::Minutes(2);

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

// Metric reporting manager initialization delay.
constexpr base::TimeDelta kInitDelay = base::Minutes(1);

// Initial metric reporting upload delay.
constexpr base::TimeDelta kInitialUploadDelay = base::Minutes(3);

// Default value for reporting device audio status.
constexpr bool kReportDeviceAudioStatusDefaultValue = true;

// Default value for reporting device network status.
constexpr bool kReportDeviceNetworkStatusDefaultValue = true;

// Default value for reporting device peripheral status.
constexpr bool kReportDevicePeripheralsDefaultValue = false;

// Returns the default report upload frequency for the current environment.
const base::TimeDelta GetDefaultReportUploadFrequency();

// Returns the default metric collection rate for the current environment.
const base::TimeDelta GetDefaultCollectionRate(base::TimeDelta default_rate);

// Returns the default event checking rate for the current environment.
const base::TimeDelta GetDefaultEventCheckingRate(base::TimeDelta default_rate);

}  // namespace reporting::metrics

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_DEFAULT_UTILS_H_

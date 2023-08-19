// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Some utilities that can be used for info metric unit tests and browser tests.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_INFO_METRIC_SAMPLER_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_INFO_METRIC_SAMPLER_TEST_UTILS_H_

#include <string>

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"

namespace reporting::test {

namespace cros_healthd = ::ash::cros_healthd::mojom;

// ------- Bus -------

cros_healthd::TelemetryInfoPtr CreateUsbBusResult(
    std::vector<cros_healthd::BusDevicePtr> usb_devices);
cros_healthd::TelemetryInfoPtr CreateThunderboltBusResult(
    std::vector<cros_healthd::ThunderboltSecurityLevel> security_levels);

// ------- CPU --------

cros_healthd::KeylockerInfoPtr CreateKeylockerInfo(bool configured);
cros_healthd::TelemetryInfoPtr CreateCpuResult(
    cros_healthd::KeylockerInfoPtr keylocker_info);

// ------- memory --------

struct MemoryInfoTestCase {
  std::string test_name;
  cros_healthd::EncryptionState healthd_encryption_state;
  reporting::MemoryEncryptionState reporting_encryption_state;
  cros_healthd::CryptoAlgorithm healthd_encryption_algorithm;
  reporting::MemoryEncryptionAlgorithm reporting_encryption_algorithm;
  int64_t max_keys;
  int64_t key_length;
};

// Create a memory encryption info for memory tests.
cros_healthd::MemoryEncryptionInfoPtr CreateMemoryEncryptionInfo(
    cros_healthd::EncryptionState encryption_state,
    int64_t max_keys,
    int64_t key_length,
    cros_healthd::CryptoAlgorithm encryption_algorithm);

// Create a memory test result.
cros_healthd::TelemetryInfoPtr CreateMemoryResult(
    cros_healthd::MemoryEncryptionInfoPtr memory_encryption_info);

void AssertMemoryInfo(const MetricData& result,
                      const MemoryInfoTestCase& test_case);

// -------- input ---------

// Create a touch screen info for input tests.
cros_healthd::TelemetryInfoPtr CreateInputResult(
    std::string library_name,
    std::vector<cros_healthd::TouchscreenDevicePtr> touchscreen_devices);

// --------- display ----------

// Create a display info for display tests.
cros_healthd::TelemetryInfoPtr CreateDisplayResult(
    cros_healthd::EmbeddedDisplayInfoPtr embedded_display,
    std::vector<cros_healthd::ExternalDisplayInfoPtr> external_displays);

// Create an embedded display. Used to feed into `CreateDisplayResult`.
cros_healthd::EmbeddedDisplayInfoPtr CreateEmbeddedDisplay(
    bool privacy_screen_supported,
    int display_width,
    int display_height,
    int resolution_horizontal,
    int resolution_vertical,
    double refresh_rate,
    std::string manufacturer,
    int model_id,
    int manufacture_year,
    std::string display_name);

// Create an external display. Used to feed into `CreateDisplayResult`.
cros_healthd::ExternalDisplayInfoPtr CreateExternalDisplay(
    int display_width,
    int display_height,
    int resolution_horizontal,
    int resolution_vertical,
    double refresh_rate,
    std::string manufacturer,
    int model_id,
    int manufacture_year,
    std::string display_name);

// -------- system ---------

// Create telemetry info for runtime counters tests.
cros_healthd::TelemetryInfoPtr CreateSystemResult(
    cros_healthd::SystemInfoPtr system_info);

// Create telemetry info with a system result that contains an error.
cros_healthd::TelemetryInfoPtr CreateSystemResultWithError();

// Create system info with the PSR field filled in.
cros_healthd::SystemInfoPtr CreateSystemInfoWithPsr(
    cros_healthd::PsrInfoPtr psr_info);

// Create system info with PSR unsupported.
cros_healthd::SystemInfoPtr CreateSystemInfoWithPsrUnsupported();

// Create system info with PSR supported and a specified log state.
cros_healthd::SystemInfoPtr CreateSystemInfoWithPsrLogState(
    cros_healthd::PsrInfo::LogState log_state);

// Create system info with PSR supported and running.
cros_healthd::SystemInfoPtr CreateSystemInfoWithPsrSupportedRunning(
    uint32_t uptime_seconds,
    uint32_t s5_counter,
    uint32_t s4_counter,
    uint32_t s3_counter);
}  // namespace reporting::test

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_INFO_METRIC_SAMPLER_TEST_UTILS_H_

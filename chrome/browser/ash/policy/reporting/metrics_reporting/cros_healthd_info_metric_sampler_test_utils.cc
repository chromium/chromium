// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_info_metric_sampler_test_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace reporting::test {

// ------- Bus -------

cros_healthd::TelemetryInfoPtr CreateUsbBusResult(
    std::vector<cros_healthd::BusDevicePtr> usb_devices) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->bus_result =
      cros_healthd::BusResult::NewBusDevices(std::move(usb_devices));
  return telemetry_info;
}

cros_healthd::TelemetryInfoPtr CreateThunderboltBusResult(
    std::vector<cros_healthd::ThunderboltSecurityLevel> security_levels) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  std::vector<cros_healthd::BusDevicePtr> bus_devices;

  for (const auto& security_level : security_levels) {
    auto tbt_device = cros_healthd::BusDevice::New();
    tbt_device->bus_info = cros_healthd::BusInfo::NewThunderboltBusInfo(
        cros_healthd::ThunderboltBusInfo::New(
            security_level,
            std::vector<cros_healthd::ThunderboltBusInterfaceInfoPtr>()));
    bus_devices.push_back(std::move(tbt_device));
  }

  telemetry_info->bus_result =
      cros_healthd::BusResult::NewBusDevices(std::move(bus_devices));
  return telemetry_info;
}

// ------- CPU -------

cros_healthd::KeylockerInfoPtr CreateKeylockerInfo(bool configured) {
  return cros_healthd::KeylockerInfo::New(configured);
}

cros_healthd::TelemetryInfoPtr CreateCpuResult(
    cros_healthd::KeylockerInfoPtr keylocker_info) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->cpu_result =
      cros_healthd::CpuResult::NewCpuInfo(cros_healthd::CpuInfo::New(
          /*num_total_threads=*/0,
          /*architecture=*/cros_healthd::CpuArchitectureEnum::kX86_64,
          /*physical_cpus=*/std::vector<cros_healthd::PhysicalCpuInfoPtr>(),
          /*temperature_channels=*/
          std::vector<cros_healthd::CpuTemperatureChannelPtr>(),
          /*keylocker_info=*/std::move(keylocker_info)));

  return telemetry_info;
}

// ------- memory --------

cros_healthd::MemoryEncryptionInfoPtr CreateMemoryEncryptionInfo(
    cros_healthd::EncryptionState encryption_state,
    int64_t max_keys,
    int64_t key_length,
    cros_healthd::CryptoAlgorithm encryption_algorithm) {
  return cros_healthd::MemoryEncryptionInfo::New(
      encryption_state, max_keys, key_length, encryption_algorithm);
}

cros_healthd::TelemetryInfoPtr CreateMemoryResult(
    cros_healthd::MemoryEncryptionInfoPtr memory_encryption_info) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->memory_result =
      cros_healthd::MemoryResult::NewMemoryInfo(cros_healthd::MemoryInfo::New(
          /*total_memory=*/0, /*free_memory=*/0, /*available_memory=*/0,
          /*page_faults_since_last_boot=*/0,
          std::move(memory_encryption_info)));
  return telemetry_info;
}

void AssertMemoryInfo(const MetricData& result,
                      const MemoryInfoTestCase& test_case) {
  EXPECT_FALSE(result.has_telemetry_data());
  ASSERT_TRUE(result.has_info_data());
  const auto& info_data = result.info_data();
  ASSERT_TRUE(info_data.has_memory_info());
  ASSERT_TRUE(info_data.memory_info().has_tme_info());

  const auto& tme_info = info_data.memory_info().tme_info();
  EXPECT_EQ(tme_info.encryption_state(), test_case.reporting_encryption_state);
  EXPECT_EQ(tme_info.encryption_algorithm(),
            test_case.reporting_encryption_algorithm);
  EXPECT_EQ(tme_info.max_keys(), test_case.max_keys);
  EXPECT_EQ(tme_info.key_length(), test_case.key_length);
}

// ------- input --------

cros_healthd::TelemetryInfoPtr CreateInputResult(
    std::string library_name,
    std::vector<cros_healthd::TouchscreenDevicePtr> touchscreen_devices) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->input_result =
      cros_healthd::InputResult::NewInputInfo(cros_healthd::InputInfo::New(
          library_name, std::move(touchscreen_devices)));

  return telemetry_info;
}

// ------- display -------

cros_healthd::TelemetryInfoPtr CreateDisplayResult(
    cros_healthd::EmbeddedDisplayInfoPtr embedded_display,
    std::vector<cros_healthd::ExternalDisplayInfoPtr> external_displays) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->display_result = cros_healthd::DisplayResult::NewDisplayInfo(
      cros_healthd::DisplayInfo::New(std::move(embedded_display),
                                     std::move(external_displays)));
  return telemetry_info;
}

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
    std::string display_name) {
  return cros_healthd::EmbeddedDisplayInfo::New(
      privacy_screen_supported, /*privacy_screen_enabled*/ false,
      cros_healthd::NullableUint32::New(display_width),
      cros_healthd::NullableUint32::New(display_height),
      cros_healthd::NullableUint32::New(resolution_horizontal),
      cros_healthd::NullableUint32::New(resolution_vertical),
      cros_healthd::NullableDouble::New(refresh_rate), manufacturer,
      cros_healthd::NullableUint16::New(model_id),
      /*serial_number*/ cros_healthd::NullableUint32::New(12345),
      /*manufacture_week*/ cros_healthd::NullableUint8::New(10),
      cros_healthd::NullableUint16::New(manufacture_year),
      /*edid_version*/ "V2.0",
      /*input_type*/ cros_healthd::DisplayInputType::kDigital, display_name);
}

cros_healthd::ExternalDisplayInfoPtr CreateExternalDisplay(
    int display_width,
    int display_height,
    int resolution_horizontal,
    int resolution_vertical,
    double refresh_rate,
    std::string manufacturer,
    int model_id,
    int manufacture_year,
    std::string display_name) {
  return cros_healthd::ExternalDisplayInfo ::New(
      cros_healthd::NullableUint32::New(display_width),
      cros_healthd::NullableUint32::New(display_height),
      cros_healthd::NullableUint32::New(resolution_horizontal),
      cros_healthd::NullableUint32::New(resolution_vertical),
      cros_healthd::NullableDouble::New(refresh_rate), manufacturer,
      cros_healthd::NullableUint16::New(model_id),
      /*serial_number*/ cros_healthd::NullableUint32::New(12345),
      /*manufacture_week*/ cros_healthd::NullableUint8::New(10),
      cros_healthd::NullableUint16::New(manufacture_year),
      /*edid_version*/ "V2.0",
      /*input_type*/ cros_healthd::DisplayInputType::kDigital, display_name);
}

cros_healthd::TelemetryInfoPtr CreateSystemResult(
    cros_healthd::SystemInfoPtr system_info) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->system_result =
      cros_healthd::SystemResult::NewSystemInfo(std::move(system_info));
  return telemetry_info;
}

cros_healthd::TelemetryInfoPtr CreateSystemResultWithError() {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->system_result =
      cros_healthd::SystemResult::NewError(cros_healthd::ProbeError::New());
  return telemetry_info;
}

cros_healthd::SystemInfoPtr CreateSystemInfoWithPsr(
    cros_healthd::PsrInfoPtr psr_info) {
  // Set up the required field with minimal information.
  auto os_info = cros_healthd::OsInfo::New();
  os_info->os_version = cros_healthd::OsVersion::New();

  return cros_healthd::SystemInfo::New(std::move(os_info), /*vpd_info=*/nullptr,
                                       /*dmi_info=*/nullptr,
                                       std::move(psr_info));
}

cros_healthd::SystemInfoPtr CreateSystemInfoWithPsrUnsupported() {
  auto psr_info = cros_healthd::PsrInfo::New();
  psr_info->is_supported = false;
  return CreateSystemInfoWithPsr(std::move(psr_info));
}

cros_healthd::SystemInfoPtr CreateSystemInfoWithPsrLogState(
    cros_healthd::PsrInfo::LogState log_state) {
  auto psr_info = cros_healthd::PsrInfo::New();
  psr_info->is_supported = true;
  psr_info->log_state = log_state;
  return CreateSystemInfoWithPsr(std::move(psr_info));
}

cros_healthd::SystemInfoPtr CreateSystemInfoWithPsrSupportedRunning(
    uint32_t uptime_seconds,
    uint32_t s5_counter,
    uint32_t s4_counter,
    uint32_t s3_counter) {
  auto psr_info = cros_healthd::PsrInfo::New();
  psr_info->is_supported = true;
  psr_info->log_state = cros_healthd::PsrInfo::LogState::kStarted;
  psr_info->uptime_seconds = uptime_seconds;
  psr_info->s5_counter = s5_counter;
  psr_info->s4_counter = s4_counter;
  psr_info->s3_counter = s3_counter;
  return CreateSystemInfoWithPsr(std::move(psr_info));
}
}  // namespace reporting::test

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"
#include <vector>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting::test {

namespace cros_healthd = ::ash::cros_healthd::mojom;
using ::testing::Eq;
using ::testing::StrEq;

struct TbtTestCase {
  std::string test_name;
  std::vector<cros_healthd::ThunderboltSecurityLevel> healthd_security_levels;
  std::vector<reporting::ThunderboltSecurityLevel> reporting_security_levels;
};

struct MemoryEncryptionTestCase {
  std::string test_name;
  cros_healthd::EncryptionState healthd_encryption_state;
  reporting::MemoryEncryptionState reporting_encryption_state;
  cros_healthd::CryptoAlgorithm healthd_encryption_algorithm;
  reporting::MemoryEncryptionAlgorithm reporting_encryption_algorithm;
  int64_t max_keys;
  int64_t key_length;
};

// Memory constants.
constexpr int64_t kTmeMaxKeys = 2;
constexpr int64_t kTmeKeysLength = 4;

// Boot Performance constants.
constexpr int64_t kBootUpSeconds = 5054;
constexpr int64_t kBootUpTimestampSeconds = 23;
constexpr int64_t kShutdownSeconds = 44003;
constexpr int64_t kShutdownTimestampSeconds = 49;
constexpr char kShutdownReason[] = "user-request";
constexpr char kShutdownReasonNotApplicable[] = "N/A";

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

cros_healthd::AudioInfoPtr CreateAudioInfo(
    bool output_mute,
    bool input_mute,
    uint64_t output_volume,
    const std::string& output_device_name,
    int64_t input_gain,
    const std::string& input_device_name,
    int64_t underruns,
    int64_t severe_underruns) {
  return cros_healthd::AudioInfo::New(
      output_mute, input_mute, output_volume, output_device_name, input_gain,
      input_device_name, underruns, severe_underruns);
}

cros_healthd::TelemetryInfoPtr CreateAudioResult(
    cros_healthd::AudioInfoPtr audio_info) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->audio_result =
      cros_healthd::AudioResult::NewAudioInfo(std::move(audio_info));
  return telemetry_info;
}

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

cros_healthd::TelemetryInfoPtr CreateBootPerformanceResult(
    int64_t boot_up_seconds,
    int64_t boot_up_timestamp_seconds,
    int64_t shutdown_seconds,
    int64_t shutdown_timestamp_seconds,
    const std::string& shutdown_reason) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->boot_performance_result =
      cros_healthd::BootPerformanceResult::NewBootPerformanceInfo(
          cros_healthd::BootPerformanceInfo::New(
              boot_up_seconds, boot_up_timestamp_seconds, shutdown_seconds,
              shutdown_timestamp_seconds, shutdown_reason));
  return telemetry_info;
}

cros_healthd::TelemetryInfoPtr CreateInputInfo(
    std::string library_name,
    std::vector<cros_healthd::TouchscreenDevicePtr> touchscreen_devices) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->input_result =
      cros_healthd::InputResult::NewInputInfo(cros_healthd::InputInfo::New(
          library_name, std::move(touchscreen_devices)));

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

cros_healthd::TelemetryInfoPtr CreateDisplayResult(
    cros_healthd::EmbeddedDisplayInfoPtr embedded_display,
    std::vector<cros_healthd::ExternalDisplayInfoPtr> external_displays) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->display_result = cros_healthd::DisplayResult::NewDisplayInfo(
      cros_healthd::DisplayInfo::New(std::move(embedded_display),
                                     std::move(external_displays)));
  return telemetry_info;
}

cros_healthd::TelemetryInfoPtr CreatePrivacyScreenResult(bool supported) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->display_result = cros_healthd::DisplayResult::NewDisplayInfo(
      cros_healthd::DisplayInfo::New(cros_healthd::EmbeddedDisplayInfo::New(
          supported, /*privacy_screen_enabled*/ false)));
  return telemetry_info;
}

absl::optional<MetricData> CollectData(
    cros_healthd::TelemetryInfoPtr telemetry_info,
    cros_healthd::ProbeCategoryEnum probe_category,
    CrosHealthdMetricSampler::MetricType metric_type) {
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
  CrosHealthdMetricSampler sampler(probe_category, metric_type);
  test::TestEvent<absl::optional<MetricData>> metric_collect_event;

  sampler.MaybeCollect(metric_collect_event.cb());
  return metric_collect_event.result();
}

class CrosHealthdMetricSamplerTest : public testing::Test {
 public:
  CrosHealthdMetricSamplerTest() {
    ash::cros_healthd::FakeCrosHealthd::Initialize();
  }

  ~CrosHealthdMetricSamplerTest() override {
    ash::cros_healthd::FakeCrosHealthd::Shutdown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
};

class CrosHealthdMetricSamplerTbtTest
    : public CrosHealthdMetricSamplerTest,
      public testing::WithParamInterface<TbtTestCase> {};

class CrosHealthdMetricSamplerMemoryEncryptionTest
    : public CrosHealthdMetricSamplerTest,
      public testing::WithParamInterface<MemoryEncryptionTestCase> {};

TEST_F(CrosHealthdMetricSamplerTest, TestUsbTelemetryMultipleEntries) {
  // Max value for 8-bit unsigned integer
  constexpr uint8_t kClassId = 255;
  constexpr uint8_t kSubclassId = 1;
  // Max value for 16-bit unsigned integer
  constexpr uint16_t kVendorId = 65535;
  constexpr uint16_t kProductId = 1;
  constexpr char kVendorName[] = "VendorName";
  constexpr char kProductName[] = "ProductName";
  constexpr char kFirmwareVersion[] = "FirmwareVersion";

  constexpr uint8_t kClassIdSecond = 1;
  constexpr uint8_t kSubclassIdSecond = 255;
  constexpr uint16_t kVendorIdSecond = 1;
  constexpr uint16_t kProductIdSecond = 65535;
  constexpr char kVendorNameSecond[] = "VendorNameSecond";
  constexpr char kProductNameSecond[] = "ProductNameSecond";
  constexpr int kExpectedUsbTelemetrySize = 2;
  constexpr int kIndexOfFirstUsbTelemetry = 0;
  constexpr int kIndexOfSecondUsbTelemetry = 1;

  cros_healthd::BusDevicePtr usb_device_first = cros_healthd::BusDevice::New();
  usb_device_first->vendor_name = kVendorName;
  usb_device_first->product_name = kProductName;
  usb_device_first->bus_info =
      cros_healthd::BusInfo::NewUsbBusInfo(cros_healthd::UsbBusInfo::New(
          kClassId, kSubclassId, /*protocol_id=*/0, kVendorId, kProductId,
          /*interfaces = */
          std::vector<cros_healthd::UsbBusInterfaceInfoPtr>(),
          cros_healthd::FwupdFirmwareVersionInfo::New(
              kFirmwareVersion, cros_healthd::FwupdVersionFormat::kPlain)));

  cros_healthd::BusDevicePtr usb_device_second = cros_healthd::BusDevice::New();
  usb_device_second->vendor_name = kVendorNameSecond;
  usb_device_second->product_name = kProductNameSecond;
  // Omit firmware version this time since it's an optional mojo field
  usb_device_second->bus_info =
      cros_healthd::BusInfo::NewUsbBusInfo(cros_healthd::UsbBusInfo::New(
          kClassIdSecond, kSubclassIdSecond, /*protocol_id=*/0, kVendorIdSecond,
          kProductIdSecond,
          /*interfaces = */
          std::vector<cros_healthd::UsbBusInterfaceInfoPtr>()));

  std::vector<cros_healthd::BusDevicePtr> usb_devices;
  usb_devices.push_back(std::move(usb_device_first));
  usb_devices.push_back(std::move(usb_device_second));

  const absl::optional<MetricData> optional_result =
      CollectData(CreateUsbBusResult(std::move(usb_devices)),
                  cros_healthd::ProbeCategoryEnum::kBus,
                  CrosHealthdMetricSampler::MetricType::kTelemetry);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_telemetry_data());
  ASSERT_TRUE(result.telemetry_data().has_peripherals_telemetry());
  ASSERT_EQ(
      result.telemetry_data().peripherals_telemetry().usb_telemetry_size(),
      kExpectedUsbTelemetrySize);

  UsbTelemetry usb_telemetry_first =
      result.telemetry_data().peripherals_telemetry().usb_telemetry(
          kIndexOfFirstUsbTelemetry);
  UsbTelemetry usb_telemetry_second =
      result.telemetry_data().peripherals_telemetry().usb_telemetry(
          kIndexOfSecondUsbTelemetry);

  EXPECT_EQ(usb_telemetry_first.class_id(), kClassId);
  EXPECT_EQ(usb_telemetry_first.subclass_id(), kSubclassId);
  EXPECT_EQ(usb_telemetry_first.vid(), kVendorId);
  EXPECT_EQ(usb_telemetry_first.pid(), kProductId);
  EXPECT_EQ(usb_telemetry_first.name(), kProductName);
  EXPECT_EQ(usb_telemetry_first.vendor(), kVendorName);
  EXPECT_TRUE(usb_telemetry_first.has_firmware_version());
  EXPECT_EQ(usb_telemetry_first.firmware_version(), kFirmwareVersion);

  EXPECT_EQ(usb_telemetry_second.class_id(), kClassIdSecond);
  EXPECT_EQ(usb_telemetry_second.subclass_id(), kSubclassIdSecond);
  EXPECT_EQ(usb_telemetry_second.vid(), kVendorIdSecond);
  EXPECT_EQ(usb_telemetry_second.pid(), kProductIdSecond);
  EXPECT_EQ(usb_telemetry_second.name(), kProductNameSecond);
  EXPECT_EQ(usb_telemetry_second.vendor(), kVendorNameSecond);
  // Firmware version shouldn't exist in telemetry when it doesn't exist in bus
  // result
  EXPECT_FALSE(usb_telemetry_second.has_firmware_version());
}

TEST_F(CrosHealthdMetricSamplerTest, TestUsbTelemetry) {
  // Max value for 8-bit unsigned integer
  constexpr uint8_t kClassId = 255;
  constexpr uint8_t kSubclassId = 1;
  // Max value for 16-bit unsigned integer
  constexpr uint16_t kVendorId = 65535;
  constexpr uint16_t kProductId = 1;
  constexpr char kVendorName[] = "VendorName";
  constexpr char kProductName[] = "ProductName";
  constexpr char kFirmwareVersion[] = "FirmwareVersion";
  constexpr int kExpectedUsbTelemetrySize = 1;
  constexpr int kIndexOfUsbTelemetry = 0;

  cros_healthd::BusDevicePtr usb_device = cros_healthd::BusDevice::New();
  usb_device->vendor_name = kVendorName;
  usb_device->product_name = kProductName;
  usb_device->bus_info =
      cros_healthd::BusInfo::NewUsbBusInfo(cros_healthd::UsbBusInfo::New(
          kClassId, kSubclassId, /*protocol_id=*/0, kVendorId, kProductId,
          /*interfaces = */
          std::vector<cros_healthd::UsbBusInterfaceInfoPtr>(),
          cros_healthd::FwupdFirmwareVersionInfo::New(
              kFirmwareVersion, cros_healthd::FwupdVersionFormat::kPlain)));

  std::vector<cros_healthd::BusDevicePtr> usb_devices;
  usb_devices.push_back(std::move(usb_device));

  const absl::optional<MetricData> optional_result =
      CollectData(CreateUsbBusResult(std::move(usb_devices)),
                  cros_healthd::ProbeCategoryEnum::kBus,
                  CrosHealthdMetricSampler::MetricType::kTelemetry);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_telemetry_data());
  ASSERT_EQ(
      result.telemetry_data().peripherals_telemetry().usb_telemetry_size(),
      kExpectedUsbTelemetrySize);

  UsbTelemetry usb_telemetry =
      result.telemetry_data().peripherals_telemetry().usb_telemetry(
          kIndexOfUsbTelemetry);

  EXPECT_EQ(usb_telemetry.class_id(), kClassId);
  EXPECT_EQ(usb_telemetry.subclass_id(), kSubclassId);
  EXPECT_EQ(usb_telemetry.vid(), kVendorId);
  EXPECT_EQ(usb_telemetry.pid(), kProductId);
  EXPECT_EQ(usb_telemetry.name(), kProductName);
  EXPECT_EQ(usb_telemetry.vendor(), kVendorName);
  EXPECT_EQ(usb_telemetry.firmware_version(), kFirmwareVersion);
}

TEST_P(CrosHealthdMetricSamplerMemoryEncryptionTest,
       TestMemoryEncryptionReporting) {
  const MemoryEncryptionTestCase& test_case = GetParam();
  const absl::optional<MetricData> optional_result = CollectData(
      CreateMemoryResult(CreateMemoryEncryptionInfo(
          test_case.healthd_encryption_state, test_case.max_keys,
          test_case.key_length, test_case.healthd_encryption_algorithm)),
      cros_healthd::ProbeCategoryEnum::kMemory,
      CrosHealthdMetricSampler::MetricType::kInfo);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_memory_info());
  ASSERT_TRUE(result.info_data().memory_info().has_tme_info());

  const auto& tme_info = result.info_data().memory_info().tme_info();
  EXPECT_EQ(tme_info.encryption_state(), test_case.reporting_encryption_state);
  EXPECT_EQ(tme_info.encryption_algorithm(),
            test_case.reporting_encryption_algorithm);
  EXPECT_EQ(tme_info.max_keys(), test_case.max_keys);
  EXPECT_EQ(tme_info.key_length(), test_case.key_length);
}

TEST_P(CrosHealthdMetricSamplerTbtTest, TestTbtSecurityLevels) {
  const TbtTestCase& test_case = GetParam();
  const absl::optional<MetricData> optional_result =
      CollectData(CreateThunderboltBusResult(test_case.healthd_security_levels),
                  cros_healthd::ProbeCategoryEnum::kBus,
                  CrosHealthdMetricSampler::MetricType::kInfo);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_bus_device_info());
  ASSERT_EQ(test_case.healthd_security_levels.size(),
            result.info_data().bus_device_info().thunderbolt_info_size());
  for (int i = 0; i < test_case.healthd_security_levels.size(); i++) {
    EXPECT_EQ(result.info_data()
                  .bus_device_info()
                  .thunderbolt_info(i)
                  .security_level(),
              test_case.reporting_security_levels[i]);
  }
}

TEST_F(CrosHealthdMetricSamplerTest, TestKeylockerConfigured) {
  const absl::optional<MetricData> optional_result =
      CollectData(CreateCpuResult(CreateKeylockerInfo(true)),
                  cros_healthd::ProbeCategoryEnum::kCpu,
                  CrosHealthdMetricSampler::MetricType::kInfo);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_cpu_info());
  ASSERT_TRUE(result.info_data().cpu_info().has_keylocker_info());
  EXPECT_TRUE(result.info_data().cpu_info().keylocker_info().configured());
  EXPECT_TRUE(result.info_data().cpu_info().keylocker_info().supported());
}

TEST_F(CrosHealthdMetricSamplerTest, TestKeylockerUnconfigured) {
  const absl::optional<MetricData> optional_result =
      CollectData(CreateCpuResult(CreateKeylockerInfo(false)),
                  cros_healthd::ProbeCategoryEnum::kCpu,
                  CrosHealthdMetricSampler::MetricType::kInfo);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_cpu_info());
  ASSERT_TRUE(result.info_data().cpu_info().has_keylocker_info());
  EXPECT_FALSE(result.info_data().cpu_info().keylocker_info().configured());
  EXPECT_TRUE(result.info_data().cpu_info().keylocker_info().supported());
}

TEST_F(CrosHealthdMetricSamplerTest, TestKeylockerUnsupported) {
  const absl::optional<MetricData> optional_result = CollectData(
      CreateCpuResult(nullptr), cros_healthd::ProbeCategoryEnum::kCpu,
      CrosHealthdMetricSampler::MetricType::kInfo);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_cpu_info());
  ASSERT_TRUE(result.info_data().cpu_info().has_keylocker_info());
  EXPECT_FALSE(result.info_data().cpu_info().keylocker_info().configured());
  EXPECT_FALSE(result.info_data().cpu_info().keylocker_info().supported());
}

TEST_F(CrosHealthdMetricSamplerTest, TestMojomError) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->cpu_result =
      cros_healthd::CpuResult::NewError(cros_healthd::ProbeError::New(
          cros_healthd::ErrorType::kFileReadError, ""));
  const absl::optional<MetricData> cpu_data = CollectData(
      std::move(telemetry_info), cros_healthd::ProbeCategoryEnum::kCpu,
      CrosHealthdMetricSampler::MetricType::kInfo);
  EXPECT_FALSE(cpu_data.has_value());

  telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->bus_result =
      cros_healthd::BusResult::NewError(cros_healthd::ProbeError::New(
          cros_healthd::ErrorType::kFileReadError, ""));
  const absl::optional<MetricData> bus_data = CollectData(
      std::move(telemetry_info), cros_healthd::ProbeCategoryEnum::kCpu,
      CrosHealthdMetricSampler::MetricType::kInfo);

  EXPECT_FALSE(bus_data.has_value());

  telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->audio_result =
      cros_healthd::AudioResult::NewError(cros_healthd::ProbeError::New(
          cros_healthd::ErrorType::kFileReadError, ""));
  const absl::optional<MetricData> audio_data = CollectData(
      std::move(telemetry_info), cros_healthd::ProbeCategoryEnum::kAudio,
      CrosHealthdMetricSampler::MetricType::kTelemetry);
  EXPECT_FALSE(audio_data.has_value());

  telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->boot_performance_result =
      cros_healthd::BootPerformanceResult::NewError(
          cros_healthd::ProbeError::New(cros_healthd::ErrorType::kFileReadError,
                                        ""));
  const absl::optional<MetricData> boot_performance_data =
      CollectData(std::move(telemetry_info),
                  cros_healthd::ProbeCategoryEnum::kBootPerformance,
                  CrosHealthdMetricSampler::MetricType::kTelemetry);
  EXPECT_FALSE(boot_performance_data.has_value());

  telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->input_result =
      cros_healthd::InputResult::NewError(cros_healthd::ProbeError::New(
          cros_healthd::ErrorType::kFileReadError, ""));
  const absl::optional<MetricData> input_data = CollectData(
      std::move(telemetry_info), cros_healthd::ProbeCategoryEnum::kInput,
      CrosHealthdMetricSampler::MetricType::kInfo);
  EXPECT_FALSE(input_data.has_value());

  telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->display_result =
      cros_healthd::DisplayResult::NewError(cros_healthd::ProbeError::New(
          cros_healthd::ErrorType::kFileReadError, ""));
  const absl::optional<MetricData> display_info_data = CollectData(
      std::move(telemetry_info), cros_healthd::ProbeCategoryEnum::kDisplay,
      CrosHealthdMetricSampler::MetricType::kInfo);
  EXPECT_FALSE(display_info_data.has_value());

  telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->display_result =
      cros_healthd::DisplayResult::NewError(cros_healthd::ProbeError::New(
          cros_healthd::ErrorType::kFileReadError, ""));
  const absl::optional<MetricData> display_telemetry_data = CollectData(
      std::move(telemetry_info), cros_healthd::ProbeCategoryEnum::kDisplay,
      CrosHealthdMetricSampler::MetricType::kTelemetry);
  EXPECT_FALSE(display_telemetry_data.has_value());
}

TEST_F(CrosHealthdMetricSamplerTest, TestAudioNormalTest) {
  const absl::optional<MetricData> optional_result = CollectData(
      CreateAudioResult(CreateAudioInfo(
          /*output_mute=*/true,
          /*input_mute=*/true, /*output_volume=*/25,
          /*output_device_name=*/"airpods",
          /*input_gain=*/50, /*input_device_name=*/"airpods", /*underruns=*/2,
          /*severe_underruns=*/2)),
      cros_healthd::ProbeCategoryEnum::kAudio,
      CrosHealthdMetricSampler::MetricType::kTelemetry);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_telemetry_data());
  ASSERT_TRUE(result.telemetry_data().has_audio_telemetry());
  ASSERT_TRUE(result.telemetry_data().audio_telemetry().output_mute());
  ASSERT_THAT(result.telemetry_data().audio_telemetry().output_volume(),
              Eq(25));
}

TEST_F(CrosHealthdMetricSamplerTest, TestAudioEmptyTest) {
  const absl::optional<MetricData> optional_result = CollectData(
      CreateAudioResult(CreateAudioInfo(
          /*output_mute=*/false,
          /*input_mute=*/false, /*output_volume=*/0,
          /*output_device_name=*/"",
          /*input_gain=*/0, /*input_device_name=*/"", /*underruns=*/0,
          /*severe_underruns=*/0)),
      cros_healthd::ProbeCategoryEnum::kAudio,
      CrosHealthdMetricSampler::MetricType::kTelemetry);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_telemetry_data());
  ASSERT_TRUE(result.telemetry_data().has_audio_telemetry());
  ASSERT_FALSE(result.telemetry_data().audio_telemetry().output_mute());
  ASSERT_FALSE(result.telemetry_data().audio_telemetry().input_mute());
  ASSERT_THAT(result.telemetry_data().audio_telemetry().output_volume(), Eq(0));
}

TEST_F(CrosHealthdMetricSamplerTest, BootPerformanceCommonBehavior) {
  const absl::optional<MetricData> optional_result =
      CollectData(CreateBootPerformanceResult(
                      kBootUpSeconds, kBootUpTimestampSeconds, kShutdownSeconds,
                      kShutdownTimestampSeconds, kShutdownReason),
                  cros_healthd::ProbeCategoryEnum::kBootPerformance,
                  CrosHealthdMetricSampler::MetricType::kTelemetry);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_telemetry_data());
  ASSERT_TRUE(result.telemetry_data().has_boot_performance_telemetry());
  ASSERT_THAT(
      result.telemetry_data().boot_performance_telemetry().boot_up_seconds(),
      Eq(kBootUpSeconds));
  ASSERT_THAT(result.telemetry_data()
                  .boot_performance_telemetry()
                  .boot_up_timestamp_seconds(),
              Eq(kBootUpTimestampSeconds));
  ASSERT_THAT(
      result.telemetry_data().boot_performance_telemetry().shutdown_seconds(),
      Eq(kShutdownSeconds));
  ASSERT_THAT(result.telemetry_data()
                  .boot_performance_telemetry()
                  .shutdown_timestamp_seconds(),
              Eq(kShutdownTimestampSeconds));
  EXPECT_EQ(
      result.telemetry_data().boot_performance_telemetry().shutdown_reason(),
      kShutdownReason);
}

TEST_F(CrosHealthdMetricSamplerTest, BootPerformanceShutdownReasonNA) {
  const absl::optional<MetricData> optional_result =
      CollectData(CreateBootPerformanceResult(
                      kBootUpSeconds, kBootUpTimestampSeconds, kShutdownSeconds,
                      kShutdownTimestampSeconds, kShutdownReasonNotApplicable),
                  cros_healthd::ProbeCategoryEnum::kBootPerformance,
                  CrosHealthdMetricSampler::MetricType::kTelemetry);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_telemetry_data());
  ASSERT_TRUE(result.telemetry_data().has_boot_performance_telemetry());
  ASSERT_THAT(
      result.telemetry_data().boot_performance_telemetry().boot_up_seconds(),
      Eq(kBootUpSeconds));
  ASSERT_THAT(result.telemetry_data()
                  .boot_performance_telemetry()
                  .boot_up_timestamp_seconds(),
              Eq(kBootUpTimestampSeconds));
  EXPECT_FALSE(result.telemetry_data()
                   .boot_performance_telemetry()
                   .has_shutdown_seconds());
  EXPECT_FALSE(result.telemetry_data()
                   .boot_performance_telemetry()
                   .has_shutdown_timestamp_seconds());
  EXPECT_EQ(
      result.telemetry_data().boot_performance_telemetry().shutdown_reason(),
      kShutdownReasonNotApplicable);
}

TEST_F(CrosHealthdMetricSamplerTest, TestTouchScreenInfoInternalSingle) {
  constexpr char kSampleLibrary[] = "SampleLibrary";
  constexpr char kSampleDevice[] = "SampleDevice";
  constexpr int kTouchPoints = 10;

  auto input_device = cros_healthd::TouchscreenDevice::New(
      cros_healthd::InputDevice::New(
          kSampleDevice, cros_healthd::InputDevice_ConnectionType::kInternal,
          /*physical_location*/ "", /*is_enabled*/ true),
      kTouchPoints, /*has_stylus*/ true,
      /*has_stylus_garage_switch*/ false);

  std::vector<cros_healthd::TouchscreenDevicePtr> touchscreen_devices;
  touchscreen_devices.push_back(std::move(input_device));

  const absl::optional<MetricData> optional_result = CollectData(
      CreateInputInfo(kSampleLibrary, std::move(touchscreen_devices)),
      cros_healthd::ProbeCategoryEnum::kInput,
      CrosHealthdMetricSampler::MetricType::kInfo);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_touch_screen_info());
  ASSERT_TRUE(result.info_data().touch_screen_info().has_library_name());
  EXPECT_THAT(result.info_data().touch_screen_info().library_name(),
              StrEq(kSampleLibrary));
  ASSERT_EQ(
      result.info_data().touch_screen_info().touch_screen_devices().size(), 1);
  EXPECT_THAT(result.info_data()
                  .touch_screen_info()
                  .touch_screen_devices(0)
                  .display_name(),
              StrEq(kSampleDevice));
  EXPECT_THAT(result.info_data()
                  .touch_screen_info()
                  .touch_screen_devices(0)
                  .touch_points(),
              Eq(kTouchPoints));
  EXPECT_TRUE(result.info_data()
                  .touch_screen_info()
                  .touch_screen_devices(0)
                  .has_stylus());
}

TEST_F(CrosHealthdMetricSamplerTest, TestTouchScreenInfoInternalMultiple) {
  constexpr char kSampleLibrary[] = "SampleLibrary";
  constexpr char kSampleDevice[] = "SampleDevice";
  constexpr char kSampleDevice2[] = "SampleDevice2";
  constexpr int kTouchPoints = 10;
  constexpr int kTouchPoints2 = 5;

  auto input_device_first = cros_healthd::TouchscreenDevice::New(
      cros_healthd::InputDevice::New(
          kSampleDevice, cros_healthd::InputDevice_ConnectionType::kInternal,
          /*physical_location*/ "", /*is_enabled*/ true),
      kTouchPoints, /*has_stylus*/ true,
      /*has_stylus_garage_switch*/ false);

  auto input_device_second = cros_healthd::TouchscreenDevice::New(
      cros_healthd::InputDevice::New(
          kSampleDevice2, cros_healthd::InputDevice_ConnectionType::kInternal,
          /*physical_location*/ "", /*is_enabled*/ true),
      kTouchPoints2, /*has_stylus*/ false,
      /*has_stylus_garage_switch*/ false);

  std::vector<cros_healthd::TouchscreenDevicePtr> touchscreen_devices;
  touchscreen_devices.push_back(std::move(input_device_first));
  touchscreen_devices.push_back(std::move(input_device_second));

  const absl::optional<MetricData> optional_result = CollectData(
      CreateInputInfo(kSampleLibrary, std::move(touchscreen_devices)),
      cros_healthd::ProbeCategoryEnum::kInput,
      CrosHealthdMetricSampler::MetricType::kInfo);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_touch_screen_info());
  ASSERT_TRUE(result.info_data().touch_screen_info().has_library_name());
  EXPECT_THAT(result.info_data().touch_screen_info().library_name(),
              StrEq(kSampleLibrary));
  ASSERT_EQ(
      result.info_data().touch_screen_info().touch_screen_devices().size(), 2);
  EXPECT_THAT(result.info_data()
                  .touch_screen_info()
                  .touch_screen_devices(0)
                  .display_name(),
              StrEq(kSampleDevice));
  EXPECT_THAT(result.info_data()
                  .touch_screen_info()
                  .touch_screen_devices(0)
                  .touch_points(),
              Eq(kTouchPoints));
  EXPECT_TRUE(result.info_data()
                  .touch_screen_info()
                  .touch_screen_devices(0)
                  .has_stylus());

  EXPECT_THAT(result.info_data()
                  .touch_screen_info()
                  .touch_screen_devices(1)
                  .display_name(),
              StrEq(kSampleDevice2));
  EXPECT_THAT(result.info_data()
                  .touch_screen_info()
                  .touch_screen_devices(1)
                  .touch_points(),
              Eq(kTouchPoints2));
  EXPECT_FALSE(result.info_data()
                   .touch_screen_info()
                   .touch_screen_devices(1)
                   .has_stylus());
}

TEST_F(CrosHealthdMetricSamplerTest, TestTouchScreenInfoExternal) {
  auto input_device = cros_healthd::TouchscreenDevice::New(
      cros_healthd::InputDevice::New(
          "SampleDevice", cros_healthd::InputDevice_ConnectionType::kUSB,
          /*physical_location*/ "", /*is_enabled*/ true),
      /*touch_points*/ 5, /*has_stylus*/ true,
      /*has_stylus_garage_switch*/ false);

  std::vector<cros_healthd::TouchscreenDevicePtr> touchscreen_devices;
  touchscreen_devices.push_back(std::move(input_device));

  const absl::optional<MetricData> optional_result = CollectData(
      CreateInputInfo("SampleLibrary", std::move(touchscreen_devices)),
      cros_healthd::ProbeCategoryEnum::kInput,
      CrosHealthdMetricSampler::MetricType::kInfo);

  ASSERT_FALSE(optional_result.has_value());
}

TEST_F(CrosHealthdMetricSamplerTest, TestTouchScreenInfoDisabled) {
  auto input_device = cros_healthd::TouchscreenDevice::New(
      cros_healthd::InputDevice::New(
          "SampleDevice", cros_healthd::InputDevice_ConnectionType::kInternal,
          /*physical_location*/ "", /*is_enabled*/ false),
      /*touch_points*/ 5, /*has_stylus*/ true,
      /*has_stylus_garage_switch*/ false);

  std::vector<cros_healthd::TouchscreenDevicePtr> touchscreen_devices;
  touchscreen_devices.push_back(std::move(input_device));

  const absl::optional<MetricData> optional_result = CollectData(
      CreateInputInfo("SampleLibrary", std::move(touchscreen_devices)),
      cros_healthd::ProbeCategoryEnum::kInput,
      CrosHealthdMetricSampler::MetricType::kInfo);

  ASSERT_FALSE(optional_result.has_value());
}

TEST_F(CrosHealthdMetricSamplerTest, TestPrivacyScreenNormalTest) {
  const absl::optional<MetricData> optional_result =
      CollectData(CreatePrivacyScreenResult(/*privacy_screen_supported*/ true),
                  cros_healthd::ProbeCategoryEnum::kDisplay,
                  CrosHealthdMetricSampler::MetricType::kInfo);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_privacy_screen_info());
  ASSERT_TRUE(result.info_data().privacy_screen_info().supported());
}

TEST_F(CrosHealthdMetricSamplerTest, TestDisplayInfoOnlyInternalDisplay) {
  bool kPrivacyScreenSupported = true;
  auto kDisplayWidth = 1080;
  auto kDisplayHeight = 27282;
  constexpr char kDisplayManufacture[] = "Samsung";
  auto kDisplayManufactureYear = 2020;
  auto kDisplayModelId = 54321;
  constexpr char kDisplayName[] = "Internal display";

  const absl::optional<MetricData> optional_result = CollectData(
      CreateDisplayResult(CreateEmbeddedDisplay(
                              kPrivacyScreenSupported, kDisplayWidth,
                              kDisplayHeight, /*resolution_horizontal*/ 1000,
                              /*resolution_vertical*/ 500, /*refresh_rate*/ 100,
                              kDisplayManufacture, kDisplayModelId,
                              kDisplayManufactureYear, kDisplayName),
                          std::vector<cros_healthd::ExternalDisplayInfoPtr>()),
      cros_healthd::ProbeCategoryEnum::kDisplay,
      CrosHealthdMetricSampler::MetricType::kInfo);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_display_info());
  ASSERT_EQ(result.info_data().display_info().display_device_size(), 1);

  ASSERT_TRUE(result.info_data().has_privacy_screen_info());
  ASSERT_TRUE(result.info_data().privacy_screen_info().supported());

  auto internal_display = result.info_data().display_info().display_device(0);
  EXPECT_EQ(internal_display.display_name(), kDisplayName);
  EXPECT_EQ(internal_display.manufacturer(), kDisplayManufacture);
  EXPECT_EQ(internal_display.display_width(), kDisplayWidth);
  EXPECT_EQ(internal_display.display_height(), kDisplayHeight);
  EXPECT_EQ(internal_display.model_id(), kDisplayModelId);
  EXPECT_EQ(internal_display.manufacture_year(), kDisplayManufactureYear);
}

TEST_F(CrosHealthdMetricSamplerTest, TestDisplayInfoMultipleDisplays) {
  bool kPrivacyScreenSupported = false;
  auto kDisplayWidth = 1080;
  auto kDisplayHeight = 27282;
  constexpr char kDisplayManufacture[] = "Samsung";
  auto kDisplayManufactureYear = 2020;
  auto kDisplayModelId = 54321;
  constexpr char kDisplayName[] = "Internal display";

  std::vector<cros_healthd::ExternalDisplayInfoPtr> external_displays;
  external_displays.push_back(CreateExternalDisplay(
      kDisplayWidth, kDisplayHeight, /*resolution_horizontal*/ 1000,
      /*resolution_vertical*/ 500, /*refresh_rate*/ 100, kDisplayManufacture,
      kDisplayModelId, kDisplayManufactureYear, kDisplayName));
  external_displays.push_back(CreateExternalDisplay(
      kDisplayWidth, kDisplayHeight, /*resolution_horizontal*/ 1000,
      /*resolution_vertical*/ 500, /*refresh_rate*/ 100, kDisplayManufacture,
      kDisplayModelId, kDisplayManufactureYear, kDisplayName));

  const absl::optional<MetricData> optional_result = CollectData(
      CreateDisplayResult(CreateEmbeddedDisplay(
                              kPrivacyScreenSupported, kDisplayWidth,
                              kDisplayHeight, /*resolution_horizontal*/ 1000,
                              /*resolution_vertical*/ 500, /*refresh_rate*/ 100,
                              kDisplayManufacture, kDisplayModelId,
                              kDisplayManufactureYear, kDisplayName),
                          std::move(external_displays)),
      cros_healthd::ProbeCategoryEnum::kDisplay,
      CrosHealthdMetricSampler::MetricType::kInfo);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_display_info());
  ASSERT_EQ(result.info_data().display_info().display_device_size(), 3);

  ASSERT_TRUE(result.info_data().has_privacy_screen_info());
  ASSERT_FALSE(result.info_data().privacy_screen_info().supported());

  auto internal_display = result.info_data().display_info().display_device(0);
  EXPECT_EQ(internal_display.display_name(), kDisplayName);
  EXPECT_EQ(internal_display.manufacturer(), kDisplayManufacture);
  EXPECT_EQ(internal_display.display_width(), kDisplayWidth);
  EXPECT_EQ(internal_display.display_height(), kDisplayHeight);
  EXPECT_EQ(internal_display.model_id(), kDisplayModelId);
  EXPECT_EQ(internal_display.manufacture_year(), kDisplayManufactureYear);

  auto external_display_1 = result.info_data().display_info().display_device(1);
  EXPECT_EQ(external_display_1.display_name(), kDisplayName);
  EXPECT_EQ(external_display_1.manufacturer(), kDisplayManufacture);
  EXPECT_EQ(external_display_1.display_width(), kDisplayWidth);
  EXPECT_EQ(external_display_1.display_height(), kDisplayHeight);
  EXPECT_EQ(external_display_1.model_id(), kDisplayModelId);
  EXPECT_EQ(external_display_1.manufacture_year(), kDisplayManufactureYear);

  auto external_display_2 = result.info_data().display_info().display_device(2);
  EXPECT_EQ(external_display_2.display_name(), kDisplayName);
  EXPECT_EQ(external_display_2.manufacturer(), kDisplayManufacture);
  EXPECT_EQ(external_display_2.display_width(), kDisplayWidth);
  EXPECT_EQ(external_display_2.display_height(), kDisplayHeight);
  EXPECT_EQ(external_display_2.model_id(), kDisplayModelId);
  EXPECT_EQ(external_display_2.manufacture_year(), kDisplayManufactureYear);
}

TEST_F(CrosHealthdMetricSamplerTest, TestDisplayTelemetryOnlyInternalDisplay) {
  auto kResolutionHorizontal = 1080;
  auto kResolutionVertical = 27282;
  auto kRefreshRate = 54321;
  constexpr char kDisplayName[] = "Internal display";

  const absl::optional<MetricData> optional_result = CollectData(
      CreateDisplayResult(CreateEmbeddedDisplay(
                              /*privacy_screen_supported*/ false,
                              /*display_width*/ 1000,
                              /*display_height*/ 900, kResolutionHorizontal,
                              kResolutionVertical, kRefreshRate,
                              /*manufacturer*/ "Samsung",
                              /*model_id*/ 100,
                              /*manufacture_year*/ 2020, kDisplayName),
                          std::vector<cros_healthd::ExternalDisplayInfoPtr>()),
      cros_healthd::ProbeCategoryEnum::kDisplay,
      CrosHealthdMetricSampler::MetricType::kTelemetry);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_telemetry_data());
  ASSERT_TRUE(result.telemetry_data().has_displays_telemetry());
  ASSERT_EQ(result.telemetry_data().displays_telemetry().display_status_size(),
            1);

  auto internal_display =
      result.telemetry_data().displays_telemetry().display_status(0);
  EXPECT_EQ(internal_display.display_name(), kDisplayName);
  EXPECT_EQ(internal_display.resolution_horizontal(), kResolutionHorizontal);
  EXPECT_EQ(internal_display.resolution_vertical(), kResolutionVertical);
  EXPECT_EQ(internal_display.refresh_rate(), kRefreshRate);
}

TEST_F(CrosHealthdMetricSamplerTest, TestDisplayTelemetryMultipleDisplays) {
  auto kResolutionHorizontal = 1080;
  auto kResolutionVertical = 27282;
  auto kRefreshRate = 54321;
  constexpr char kDisplayName[] = "Internal display";

  std::vector<cros_healthd::ExternalDisplayInfoPtr> external_displays;
  external_displays.push_back(CreateExternalDisplay(
      /*display_width*/ 1000,
      /*display_height*/ 900, kResolutionHorizontal, kResolutionVertical,
      kRefreshRate,
      /*manufacturer*/ "Samsung",
      /*model_id*/ 100,
      /*manufacture_year*/ 2020, kDisplayName));
  external_displays.push_back(CreateExternalDisplay(
      /*display_width*/ 1000,
      /*display_height*/ 900, kResolutionHorizontal, kResolutionVertical,
      kRefreshRate,
      /*manufacturer*/ "Samsung",
      /*model_id*/ 100,
      /*manufacture_year*/ 2020, kDisplayName));

  const absl::optional<MetricData> optional_result = CollectData(
      CreateDisplayResult(CreateEmbeddedDisplay(
                              /*privacy_screen_supported*/ false,
                              /*display_width*/ 1000,
                              /*display_height*/ 900, kResolutionHorizontal,
                              kResolutionVertical, kRefreshRate,
                              /*manufacturer*/ "Samsung",
                              /*model_id*/ 100,
                              /*manufacture_year*/ 2020, kDisplayName),
                          std::move(external_displays)),
      cros_healthd::ProbeCategoryEnum::kDisplay,
      CrosHealthdMetricSampler::MetricType::kTelemetry);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_telemetry_data());
  ASSERT_TRUE(result.telemetry_data().has_displays_telemetry());
  ASSERT_EQ(result.telemetry_data().displays_telemetry().display_status_size(),
            3);

  auto internal_display =
      result.telemetry_data().displays_telemetry().display_status(0);
  EXPECT_EQ(internal_display.display_name(), kDisplayName);
  EXPECT_EQ(internal_display.resolution_horizontal(), kResolutionHorizontal);
  EXPECT_EQ(internal_display.resolution_vertical(), kResolutionVertical);
  EXPECT_EQ(internal_display.refresh_rate(), kRefreshRate);

  auto external_display_1 =
      result.telemetry_data().displays_telemetry().display_status(1);
  EXPECT_EQ(external_display_1.display_name(), kDisplayName);
  EXPECT_EQ(external_display_1.resolution_horizontal(), kResolutionHorizontal);
  EXPECT_EQ(external_display_1.resolution_vertical(), kResolutionVertical);
  EXPECT_EQ(external_display_1.refresh_rate(), kRefreshRate);

  auto external_display_2 =
      result.telemetry_data().displays_telemetry().display_status(2);
  EXPECT_EQ(external_display_2.display_name(), kDisplayName);
  EXPECT_EQ(external_display_2.resolution_horizontal(), kResolutionHorizontal);
  EXPECT_EQ(external_display_2.resolution_vertical(), kResolutionVertical);
  EXPECT_EQ(external_display_2.refresh_rate(), kRefreshRate);
}

INSTANTIATE_TEST_SUITE_P(
    CrosHealthdMetricSamplerTbtTests,
    CrosHealthdMetricSamplerTbtTest,
    testing::ValuesIn<TbtTestCase>({
        {"TbtSecurityNoneLevel",
         std::vector<cros_healthd::ThunderboltSecurityLevel>{
             cros_healthd::ThunderboltSecurityLevel::kNone},
         std::vector<reporting::ThunderboltSecurityLevel>{
             THUNDERBOLT_SECURITY_NONE_LEVEL}},
        {"TbtSecurityUserLevel",
         std::vector<cros_healthd::ThunderboltSecurityLevel>{
             cros_healthd::ThunderboltSecurityLevel::kUserLevel},
         std::vector<reporting::ThunderboltSecurityLevel>{
             THUNDERBOLT_SECURITY_USER_LEVEL}},
        {"TbtSecuritySecureLevel",
         std::vector<cros_healthd::ThunderboltSecurityLevel>{
             cros_healthd::ThunderboltSecurityLevel::kSecureLevel},
         std::vector<reporting::ThunderboltSecurityLevel>{
             THUNDERBOLT_SECURITY_SECURE_LEVEL}},
        {"TbtSecurityDpOnlyLevel",
         std::vector<cros_healthd::ThunderboltSecurityLevel>{
             cros_healthd::ThunderboltSecurityLevel::kDpOnlyLevel},
         std::vector<reporting::ThunderboltSecurityLevel>{
             THUNDERBOLT_SECURITY_DP_ONLY_LEVEL}},
        {"TbtSecurityUsbOnlyLevel",
         std::vector<cros_healthd::ThunderboltSecurityLevel>{
             cros_healthd::ThunderboltSecurityLevel::kUsbOnlyLevel},
         std::vector<reporting::ThunderboltSecurityLevel>{
             THUNDERBOLT_SECURITY_USB_ONLY_LEVEL}},
        {"TbtSecurityNoPcieLevel",
         std::vector<cros_healthd::ThunderboltSecurityLevel>{
             cros_healthd::ThunderboltSecurityLevel::kNoPcieLevel},
         std::vector<reporting::ThunderboltSecurityLevel>{
             THUNDERBOLT_SECURITY_NO_PCIE_LEVEL}},
        {"TbtMultipleControllers",
         std::vector<cros_healthd::ThunderboltSecurityLevel>{
             cros_healthd::ThunderboltSecurityLevel::kNoPcieLevel,
             cros_healthd::ThunderboltSecurityLevel::kUsbOnlyLevel},
         std::vector<reporting::ThunderboltSecurityLevel>{
             THUNDERBOLT_SECURITY_NO_PCIE_LEVEL,
             THUNDERBOLT_SECURITY_USB_ONLY_LEVEL}},
    }),
    [](const testing::TestParamInfo<CrosHealthdMetricSamplerTbtTest::ParamType>&
           info) { return info.param.test_name; });

INSTANTIATE_TEST_SUITE_P(
    CrosHealthdMetricSamplerMemoryEncryptionTests,
    CrosHealthdMetricSamplerMemoryEncryptionTest,
    testing::ValuesIn<MemoryEncryptionTestCase>({
        {"UnknownEncryptionState", cros_healthd::EncryptionState::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_STATE_UNKNOWN,
         cros_healthd::CryptoAlgorithm::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_ALGORITHM_UNKNOWN, 0, 0},
        {"DisabledEncryptionState",
         cros_healthd::EncryptionState::kEncryptionDisabled,
         ::reporting::MEMORY_ENCRYPTION_STATE_DISABLED,
         cros_healthd::CryptoAlgorithm::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_ALGORITHM_UNKNOWN, 0, 0},
        {"TmeEncryptionState", cros_healthd::EncryptionState::kTmeEnabled,
         ::reporting::MEMORY_ENCRYPTION_STATE_TME,
         cros_healthd::CryptoAlgorithm::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_ALGORITHM_UNKNOWN, 0, 0},
        {"MktmeEncryptionState", cros_healthd::EncryptionState::kMktmeEnabled,
         ::reporting::MEMORY_ENCRYPTION_STATE_MKTME,
         cros_healthd::CryptoAlgorithm::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_ALGORITHM_UNKNOWN, 0, 0},
        {"UnkownEncryptionAlgorithm", cros_healthd::EncryptionState::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_STATE_UNKNOWN,
         cros_healthd::CryptoAlgorithm::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_ALGORITHM_UNKNOWN, 0, 0},
        {"AesXts128EncryptionAlgorithm",
         cros_healthd::EncryptionState::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_STATE_UNKNOWN,
         cros_healthd::CryptoAlgorithm::kAesXts128,
         ::reporting::MEMORY_ENCRYPTION_ALGORITHM_AES_XTS_128, 0, 0},
        {"AesXts256EncryptionAlgorithm",
         cros_healthd::EncryptionState::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_STATE_UNKNOWN,
         cros_healthd::CryptoAlgorithm::kAesXts256,
         ::reporting::MEMORY_ENCRYPTION_ALGORITHM_AES_XTS_256, 0, 0},
        {"KeyValuesSet", cros_healthd::EncryptionState::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_STATE_UNKNOWN,
         cros_healthd::CryptoAlgorithm::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_ALGORITHM_UNKNOWN, kTmeMaxKeys,
         kTmeKeysLength},
    }),
    [](const testing::TestParamInfo<
        CrosHealthdMetricSamplerMemoryEncryptionTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace reporting::test

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_info_metric_sampler_test_utils.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_audio_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_boot_performance_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_bus_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_cpu_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_display_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_input_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_memory_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_psr_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_sampler_handler.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting::test {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Property;
using ::testing::StrEq;
namespace cros_healthd = ::ash::cros_healthd::mojom;

// Child of `CrosHealthdPsrSamplerHandler` that sets wait time between retries
// to zero to prevent time out in unit tests. This is less intrusive
// to production code than adding a method `SetWaitTimeForTest` to
// `CrosHealthdPsrSamplerHandler`. Also allows setting an action before retrying
// for testing first-time failure scenarios.
class CrosHealthdPsrSamplerHandlerForTest
    : public CrosHealthdPsrSamplerHandler {
 public:
  static constexpr uint32_t kUptimeSeconds = 1u;
  static constexpr uint32_t kS5Counter = 2u;
  static constexpr uint32_t kS4Counter = 3u;
  static constexpr uint32_t kS3Counter = 4u;

  CrosHealthdPsrSamplerHandlerForTest() {
    ON_CALL(*this, Retry(_, _))
        .WillByDefault(
            [this](OptionalMetricCallback callback, size_t num_retries_left) {
              this->before_retry_action_.Run(num_retries_left);
              this->CrosHealthdPsrSamplerHandler::Retry(std::move(callback),
                                                        num_retries_left);
            });
    wait_time_ = base::TimeDelta();
  }
  CrosHealthdPsrSamplerHandlerForTest(
      const CrosHealthdPsrSamplerHandlerForTest&) = delete;
  CrosHealthdPsrSamplerHandlerForTest& operator=(
      const CrosHealthdPsrSamplerHandlerForTest&) = delete;
  ~CrosHealthdPsrSamplerHandlerForTest() override = default;

  MOCK_METHOD(void, Retry, (OptionalMetricCallback, size_t), (const override));

  // Set the changes before retry.
  void SetActionBeforeRetry(
      base::RepeatingCallback<void(size_t /*num_retries_left*/)> action) {
    before_retry_action_ = std::move(action);
  }

 private:
  base::RepeatingCallback<void(size_t /*num_retries_left*/)>
      before_retry_action_{base::DoNothing()};
};

struct TbtTestCase {
  std::string test_name;
  std::vector<cros_healthd::ThunderboltSecurityLevel> healthd_security_levels;
  std::vector<reporting::ThunderboltSecurityLevel> reporting_security_levels;
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

cros_healthd::TelemetryInfoPtr CreatePrivacyScreenResult(bool supported) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->display_result = cros_healthd::DisplayResult::NewDisplayInfo(
      cros_healthd::DisplayInfo::New(cros_healthd::EmbeddedDisplayInfo::New(
          supported, /*privacy_screen_enabled*/ false)));
  return telemetry_info;
}

std::optional<MetricData> CollectData(
    std::unique_ptr<CrosHealthdSamplerHandler> info_handler,
    cros_healthd::TelemetryInfoPtr telemetry_info,
    cros_healthd::ProbeCategoryEnum probe_category,
    CrosHealthdSamplerHandler::MetricType metric_type) {
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
  CrosHealthdMetricSampler sampler(std::move(info_handler), probe_category);
  test::TestEvent<std::optional<MetricData>> metric_collect_event;

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
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
};

class CrosHealthdMetricSamplerTbtTest
    : public CrosHealthdMetricSamplerTest,
      public testing::WithParamInterface<TbtTestCase> {};

class CrosHealthdMetricSamplerMemoryInfoTest
    : public CrosHealthdMetricSamplerTest,
      public testing::WithParamInterface<MemoryInfoTestCase> {};

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

  const auto optional_result =
      CollectData(std::make_unique<CrosHealthdBusSamplerHandler>(
                      CrosHealthdSamplerHandler::MetricType::kTelemetry),
                  CreateUsbBusResult(std::move(usb_devices)),
                  cros_healthd::ProbeCategoryEnum::kBus,
                  CrosHealthdSamplerHandler::MetricType::kTelemetry);

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

  const auto optional_result =
      CollectData(std::make_unique<CrosHealthdBusSamplerHandler>(
                      CrosHealthdSamplerHandler::MetricType::kTelemetry),
                  CreateUsbBusResult(std::move(usb_devices)),
                  cros_healthd::ProbeCategoryEnum::kBus,
                  CrosHealthdSamplerHandler::MetricType::kTelemetry);

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

TEST_F(CrosHealthdMetricSamplerTest, TestRuntimeCountersTelemetryNoPsrInfo) {
  auto handler = std::make_unique<CrosHealthdPsrSamplerHandlerForTest>();
  EXPECT_CALL(*handler, Retry(_, 0u)).Times(1);

  const auto optional_result = CollectData(
      std::move(handler), CreateSystemResult(CreateSystemInfoWithPsr(nullptr)),
      cros_healthd::ProbeCategoryEnum::kSystem,
      CrosHealthdSamplerHandler::MetricType::kTelemetry);

  EXPECT_FALSE(optional_result.has_value());
}

TEST_F(CrosHealthdMetricSamplerTest,
       TestRuntimeCountersTelemetryErrorGettingPsrInfo) {
  auto handler = std::make_unique<CrosHealthdPsrSamplerHandlerForTest>();
  EXPECT_CALL(*handler, Retry(_, 0u)).Times(1);

  const auto optional_result =
      CollectData(std::move(handler), CreateSystemResultWithError(),
                  cros_healthd::ProbeCategoryEnum::kSystem,
                  CrosHealthdSamplerHandler::MetricType::kTelemetry);

  EXPECT_FALSE(optional_result.has_value());
}

TEST_F(CrosHealthdMetricSamplerTest,
       TestRuntimeCountersTelemetryPsrUnsupported) {
  auto handler = std::make_unique<CrosHealthdPsrSamplerHandlerForTest>();
  EXPECT_CALL(*handler, Retry(_, 0u)).Times(1);

  const auto optional_result =
      CollectData(std::move(handler),
                  CreateSystemResult(CreateSystemInfoWithPsrUnsupported()),
                  cros_healthd::ProbeCategoryEnum::kSystem,
                  CrosHealthdSamplerHandler::MetricType::kTelemetry);

  EXPECT_FALSE(optional_result.has_value());
}

TEST_F(CrosHealthdMetricSamplerTest,
       TestRuntimeCountersTelemetryPsrNotStarted) {
  auto handler = std::make_unique<CrosHealthdPsrSamplerHandlerForTest>();
  EXPECT_CALL(*handler, Retry(_, 0u)).Times(1);

  const auto optional_result =
      CollectData(std::move(handler),
                  CreateSystemResult(CreateSystemInfoWithPsrLogState(
                      cros_healthd::PsrInfo::LogState::kNotStarted)),
                  cros_healthd::ProbeCategoryEnum::kSystem,
                  CrosHealthdSamplerHandler::MetricType::kTelemetry);

  EXPECT_FALSE(optional_result.has_value());
}

TEST_F(CrosHealthdMetricSamplerTest, TestRuntimeCountersTelemetryPsrStopped) {
  auto handler = std::make_unique<CrosHealthdPsrSamplerHandlerForTest>();
  EXPECT_CALL(*handler, Retry(_, 0u)).Times(1);

  const auto optional_result =
      CollectData(std::move(handler),
                  CreateSystemResult(CreateSystemInfoWithPsrLogState(
                      cros_healthd::PsrInfo::LogState::kStopped)),
                  cros_healthd::ProbeCategoryEnum::kSystem,
                  CrosHealthdSamplerHandler::MetricType::kTelemetry);

  EXPECT_FALSE(optional_result.has_value());
}

TEST_F(CrosHealthdMetricSamplerTest,
       TestRuntimeCountersTelemetryPsrSupportedRunning) {
  auto handler = std::make_unique<CrosHealthdPsrSamplerHandlerForTest>();
  EXPECT_CALL(*handler, Retry(_, 0u)).Times(0);
  const auto optional_result =
      CollectData(std::move(handler),
                  CreateSystemResult(CreateSystemInfoWithPsrSupportedRunning(
                      CrosHealthdPsrSamplerHandlerForTest::kUptimeSeconds,
                      CrosHealthdPsrSamplerHandlerForTest::kS5Counter,
                      CrosHealthdPsrSamplerHandlerForTest::kS4Counter,
                      CrosHealthdPsrSamplerHandlerForTest::kS3Counter)),
                  cros_healthd::ProbeCategoryEnum::kSystem,
                  CrosHealthdSamplerHandler::MetricType::kTelemetry);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();
  ASSERT_TRUE(result.has_telemetry_data());
  ASSERT_TRUE(result.telemetry_data().has_runtime_counters_telemetry());

  const auto& runtime_counters_telemetry =
      result.telemetry_data().runtime_counters_telemetry();
  EXPECT_THAT(
      runtime_counters_telemetry,
      AllOf(
          Property(&reporting::RuntimeCountersTelemetry::uptime_runtime_seconds,
                   Eq(static_cast<int64_t>(
                       CrosHealthdPsrSamplerHandlerForTest::kUptimeSeconds))),
          Property(&reporting::RuntimeCountersTelemetry::counter_enter_sleep,
                   Eq(static_cast<int64_t>(
                       CrosHealthdPsrSamplerHandlerForTest::kS3Counter))),
          Property(
              &reporting::RuntimeCountersTelemetry::counter_enter_hibernation,
              Eq(static_cast<int64_t>(
                  CrosHealthdPsrSamplerHandlerForTest::kS4Counter))),
          Property(&reporting::RuntimeCountersTelemetry::counter_enter_poweroff,
                   Eq(static_cast<int64_t>(
                       CrosHealthdPsrSamplerHandlerForTest::kS5Counter)))));
}

TEST_F(CrosHealthdMetricSamplerTest,
       TestRuntimeCountersTelemetryFirstTimeFailsSecondTimeSucceeds) {
  auto handler = std::make_unique<CrosHealthdPsrSamplerHandlerForTest>();
  EXPECT_CALL(*handler, Retry(_, 0u)).Times(1);
  handler->SetActionBeforeRetry(
      base::BindRepeating([](size_t num_retries_left) {
        // Before retry, set healthd mock to return a successful result.
        auto system_result =
            CreateSystemResult(CreateSystemInfoWithPsrSupportedRunning(
                CrosHealthdPsrSamplerHandlerForTest::kUptimeSeconds,
                CrosHealthdPsrSamplerHandlerForTest::kS5Counter,
                CrosHealthdPsrSamplerHandlerForTest::kS4Counter,
                CrosHealthdPsrSamplerHandlerForTest::kS3Counter));

        ash::cros_healthd::FakeCrosHealthd::Get()
            ->SetProbeTelemetryInfoResponseForTesting(system_result);
      }));
  const auto optional_result = CollectData(
      std::move(handler),
      // Initially let healthd return an erroneous PSR-unsupported result.
      CreateSystemResult(CreateSystemInfoWithPsrUnsupported()),
      cros_healthd::ProbeCategoryEnum::kSystem,
      CrosHealthdSamplerHandler::MetricType::kTelemetry);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();
  ASSERT_TRUE(result.has_telemetry_data());
  ASSERT_TRUE(result.telemetry_data().has_runtime_counters_telemetry());

  const auto& runtime_counters_telemetry =
      result.telemetry_data().runtime_counters_telemetry();
  EXPECT_THAT(
      runtime_counters_telemetry,
      AllOf(
          Property(&reporting::RuntimeCountersTelemetry::uptime_runtime_seconds,
                   Eq(static_cast<int64_t>(
                       CrosHealthdPsrSamplerHandlerForTest::kUptimeSeconds))),
          Property(&reporting::RuntimeCountersTelemetry::counter_enter_sleep,
                   Eq(static_cast<int64_t>(
                       CrosHealthdPsrSamplerHandlerForTest::kS3Counter))),
          Property(
              &reporting::RuntimeCountersTelemetry::counter_enter_hibernation,
              Eq(static_cast<int64_t>(
                  CrosHealthdPsrSamplerHandlerForTest::kS4Counter))),
          Property(&reporting::RuntimeCountersTelemetry::counter_enter_poweroff,
                   Eq(static_cast<int64_t>(
                       CrosHealthdPsrSamplerHandlerForTest::kS5Counter)))));
}

TEST_P(CrosHealthdMetricSamplerMemoryInfoTest, TestMemoryInfoReporting) {
  const auto& test_case = GetParam();
  const auto optional_result = CollectData(
      std::make_unique<CrosHealthdMemorySamplerHandler>(),
      CreateMemoryResult(CreateMemoryEncryptionInfo(
          test_case.healthd_encryption_state, test_case.max_keys,
          test_case.key_length, test_case.healthd_encryption_algorithm)),
      cros_healthd::ProbeCategoryEnum::kMemory,
      CrosHealthdSamplerHandler::MetricType::kInfo);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();
  AssertMemoryInfo(result, test_case);
}

TEST_P(CrosHealthdMetricSamplerTbtTest, TestTbtSecurityLevels) {
  const TbtTestCase& test_case = GetParam();
  const auto optional_result =
      CollectData(std::make_unique<CrosHealthdBusSamplerHandler>(
                      CrosHealthdSamplerHandler::MetricType::kInfo),
                  CreateThunderboltBusResult(test_case.healthd_security_levels),
                  cros_healthd::ProbeCategoryEnum::kBus,
                  CrosHealthdSamplerHandler::MetricType::kInfo);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_bus_device_info());
  ASSERT_EQ(static_cast<int>(test_case.healthd_security_levels.size()),
            result.info_data().bus_device_info().thunderbolt_info_size());
  for (size_t i = 0; i < test_case.healthd_security_levels.size(); i++) {
    EXPECT_EQ(result.info_data()
                  .bus_device_info()
                  .thunderbolt_info(i)
                  .security_level(),
              test_case.reporting_security_levels[i]);
  }
}

TEST_F(CrosHealthdMetricSamplerTest, TestKeylockerConfigured) {
  const auto optional_result =
      CollectData(std::make_unique<CrosHealthdCpuSamplerHandler>(),
                  CreateCpuResult(CreateKeylockerInfo(true)),
                  cros_healthd::ProbeCategoryEnum::kCpu,
                  CrosHealthdSamplerHandler::MetricType::kInfo);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_cpu_info());
  ASSERT_TRUE(result.info_data().cpu_info().has_keylocker_info());
  EXPECT_TRUE(result.info_data().cpu_info().keylocker_info().configured());
  EXPECT_TRUE(result.info_data().cpu_info().keylocker_info().supported());
}

TEST_F(CrosHealthdMetricSamplerTest, TestKeylockerUnconfigured) {
  const auto optional_result =
      CollectData(std::make_unique<CrosHealthdCpuSamplerHandler>(),
                  CreateCpuResult(CreateKeylockerInfo(false)),
                  cros_healthd::ProbeCategoryEnum::kCpu,
                  CrosHealthdSamplerHandler::MetricType::kInfo);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_cpu_info());
  ASSERT_TRUE(result.info_data().cpu_info().has_keylocker_info());
  EXPECT_FALSE(result.info_data().cpu_info().keylocker_info().configured());
  EXPECT_TRUE(result.info_data().cpu_info().keylocker_info().supported());
}

TEST_F(CrosHealthdMetricSamplerTest, TestKeylockerUnsupported) {
  const auto optional_result = CollectData(
      std::make_unique<CrosHealthdCpuSamplerHandler>(),
      CreateCpuResult(nullptr), cros_healthd::ProbeCategoryEnum::kCpu,
      CrosHealthdSamplerHandler::MetricType::kInfo);

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
  const std::optional<MetricData> cpu_data = CollectData(
      std::make_unique<CrosHealthdCpuSamplerHandler>(),
      std::move(telemetry_info), cros_healthd::ProbeCategoryEnum::kCpu,
      CrosHealthdSamplerHandler::MetricType::kInfo);
  EXPECT_FALSE(cpu_data.has_value());

  telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->bus_result =
      cros_healthd::BusResult::NewError(cros_healthd::ProbeError::New(
          cros_healthd::ErrorType::kFileReadError, ""));
  const std::optional<MetricData> bus_data = CollectData(
      std::make_unique<CrosHealthdBusSamplerHandler>(
          CrosHealthdSamplerHandler::MetricType::kInfo),
      std::move(telemetry_info), cros_healthd::ProbeCategoryEnum::kBus,
      CrosHealthdSamplerHandler::MetricType::kInfo);

  EXPECT_FALSE(bus_data.has_value());

  telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->audio_result =
      cros_healthd::AudioResult::NewError(cros_healthd::ProbeError::New(
          cros_healthd::ErrorType::kFileReadError, ""));
  const std::optional<MetricData> audio_data = CollectData(
      std::make_unique<CrosHealthdAudioSamplerHandler>(),
      std::move(telemetry_info), cros_healthd::ProbeCategoryEnum::kAudio,
      CrosHealthdSamplerHandler::MetricType::kTelemetry);
  EXPECT_FALSE(audio_data.has_value());

  telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->boot_performance_result =
      cros_healthd::BootPerformanceResult::NewError(
          cros_healthd::ProbeError::New(cros_healthd::ErrorType::kFileReadError,
                                        ""));
  const std::optional<MetricData> boot_performance_data =
      CollectData(std::make_unique<CrosHealthdBootPerformanceSamplerHandler>(),
                  std::move(telemetry_info),
                  cros_healthd::ProbeCategoryEnum::kBootPerformance,
                  CrosHealthdSamplerHandler::MetricType::kTelemetry);
  EXPECT_FALSE(boot_performance_data.has_value());

  telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->input_result =
      cros_healthd::InputResult::NewError(cros_healthd::ProbeError::New(
          cros_healthd::ErrorType::kFileReadError, ""));
  const std::optional<MetricData> input_data = CollectData(
      std::make_unique<CrosHealthdInputSamplerHandler>(),
      std::move(telemetry_info), cros_healthd::ProbeCategoryEnum::kInput,
      CrosHealthdSamplerHandler::MetricType::kInfo);
  EXPECT_FALSE(input_data.has_value());

  telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->display_result =
      cros_healthd::DisplayResult::NewError(cros_healthd::ProbeError::New(
          cros_healthd::ErrorType::kFileReadError, ""));
  const std::optional<MetricData> display_info_data = CollectData(
      std::make_unique<CrosHealthdDisplaySamplerHandler>(
          CrosHealthdSamplerHandler::MetricType::kInfo),
      std::move(telemetry_info), cros_healthd::ProbeCategoryEnum::kDisplay,
      CrosHealthdSamplerHandler::MetricType::kInfo);
  EXPECT_FALSE(display_info_data.has_value());

  telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->display_result =
      cros_healthd::DisplayResult::NewError(cros_healthd::ProbeError::New(
          cros_healthd::ErrorType::kFileReadError, ""));
  const std::optional<MetricData> display_telemetry_data = CollectData(
      std::make_unique<CrosHealthdDisplaySamplerHandler>(
          CrosHealthdSamplerHandler::MetricType::kTelemetry),
      std::move(telemetry_info), cros_healthd::ProbeCategoryEnum::kDisplay,
      CrosHealthdSamplerHandler::MetricType::kTelemetry);
  EXPECT_FALSE(display_telemetry_data.has_value());
}

TEST_F(CrosHealthdMetricSamplerTest, TestAudioNormalTest) {
  const auto optional_result = CollectData(
      std::make_unique<CrosHealthdAudioSamplerHandler>(),
      CreateAudioResult(CreateAudioInfo(
          /*output_mute=*/true,
          /*input_mute=*/true, /*output_volume=*/25,
          /*output_device_name=*/"airpods",
          /*input_gain=*/50, /*input_device_name=*/"airpods", /*underruns=*/2,
          /*severe_underruns=*/2)),
      cros_healthd::ProbeCategoryEnum::kAudio,
      CrosHealthdSamplerHandler::MetricType::kTelemetry);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_telemetry_data());
  ASSERT_TRUE(result.telemetry_data().has_audio_telemetry());
  ASSERT_TRUE(result.telemetry_data().audio_telemetry().output_mute());
  ASSERT_THAT(result.telemetry_data().audio_telemetry().output_volume(),
              Eq(25));
}

TEST_F(CrosHealthdMetricSamplerTest, TestAudioEmptyTest) {
  const auto optional_result = CollectData(
      std::make_unique<CrosHealthdAudioSamplerHandler>(),
      CreateAudioResult(CreateAudioInfo(
          /*output_mute=*/false,
          /*input_mute=*/false, /*output_volume=*/0,
          /*output_device_name=*/"",
          /*input_gain=*/0, /*input_device_name=*/"", /*underruns=*/0,
          /*severe_underruns=*/0)),
      cros_healthd::ProbeCategoryEnum::kAudio,
      CrosHealthdSamplerHandler::MetricType::kTelemetry);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_telemetry_data());
  ASSERT_TRUE(result.telemetry_data().has_audio_telemetry());
  ASSERT_FALSE(result.telemetry_data().audio_telemetry().output_mute());
  ASSERT_FALSE(result.telemetry_data().audio_telemetry().input_mute());
  ASSERT_THAT(result.telemetry_data().audio_telemetry().output_volume(), Eq(0));
}

TEST_F(CrosHealthdMetricSamplerTest, BootPerformanceCommonBehavior) {
  const auto optional_result =
      CollectData(std::make_unique<CrosHealthdBootPerformanceSamplerHandler>(),
                  CreateBootPerformanceResult(
                      kBootUpSeconds, kBootUpTimestampSeconds, kShutdownSeconds,
                      kShutdownTimestampSeconds, kShutdownReason),
                  cros_healthd::ProbeCategoryEnum::kBootPerformance,
                  CrosHealthdSamplerHandler::MetricType::kTelemetry);

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
  const auto optional_result =
      CollectData(std::make_unique<CrosHealthdBootPerformanceSamplerHandler>(),
                  CreateBootPerformanceResult(
                      kBootUpSeconds, kBootUpTimestampSeconds, kShutdownSeconds,
                      kShutdownTimestampSeconds, kShutdownReasonNotApplicable),
                  cros_healthd::ProbeCategoryEnum::kBootPerformance,
                  CrosHealthdSamplerHandler::MetricType::kTelemetry);

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
  static constexpr char kSampleLibrary[] = "SampleLibrary";
  static constexpr char kSampleDevice[] = "SampleDevice";
  static constexpr int kTouchPoints = 10;

  auto input_device = cros_healthd::TouchscreenDevice::New(
      cros_healthd::InputDevice::New(
          kSampleDevice, cros_healthd::InputDevice_ConnectionType::kInternal,
          /*physical_location*/ "", /*is_enabled*/ true),
      kTouchPoints, /*has_stylus*/ true,
      /*has_stylus_garage_switch*/ false);

  std::vector<cros_healthd::TouchscreenDevicePtr> touchscreen_devices;
  touchscreen_devices.push_back(std::move(input_device));

  const auto optional_result = CollectData(
      std::make_unique<CrosHealthdInputSamplerHandler>(),
      CreateInputResult(kSampleLibrary, std::move(touchscreen_devices)),
      cros_healthd::ProbeCategoryEnum::kInput,
      CrosHealthdSamplerHandler::MetricType::kInfo);

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
  static constexpr char kSampleLibrary[] = "SampleLibrary";
  static constexpr char kSampleDevice[] = "SampleDevice";
  static constexpr char kSampleDevice2[] = "SampleDevice2";
  static constexpr int kTouchPoints = 10;
  static constexpr int kTouchPoints2 = 5;

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

  const auto optional_result = CollectData(
      std::make_unique<CrosHealthdInputSamplerHandler>(),
      CreateInputResult(kSampleLibrary, std::move(touchscreen_devices)),
      cros_healthd::ProbeCategoryEnum::kInput,
      CrosHealthdSamplerHandler::MetricType::kInfo);

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

  const auto optional_result = CollectData(
      std::make_unique<CrosHealthdInputSamplerHandler>(),
      CreateInputResult("SampleLibrary", std::move(touchscreen_devices)),
      cros_healthd::ProbeCategoryEnum::kInput,
      CrosHealthdSamplerHandler::MetricType::kInfo);

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

  const auto optional_result = CollectData(
      std::make_unique<CrosHealthdInputSamplerHandler>(),
      CreateInputResult("SampleLibrary", std::move(touchscreen_devices)),
      cros_healthd::ProbeCategoryEnum::kInput,
      CrosHealthdSamplerHandler::MetricType::kInfo);

  ASSERT_FALSE(optional_result.has_value());
}

TEST_F(CrosHealthdMetricSamplerTest, TestPrivacyScreenNormalTest) {
  const auto optional_result =
      CollectData(std::make_unique<CrosHealthdDisplaySamplerHandler>(
                      CrosHealthdSamplerHandler::MetricType::kInfo),
                  CreatePrivacyScreenResult(/*privacy_screen_supported*/ true),
                  cros_healthd::ProbeCategoryEnum::kDisplay,
                  CrosHealthdSamplerHandler::MetricType::kInfo);

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_privacy_screen_info());
  ASSERT_TRUE(result.info_data().privacy_screen_info().supported());
}

TEST_F(CrosHealthdMetricSamplerTest, TestDisplayInfoOnlyInternalDisplay) {
  static constexpr bool kPrivacyScreenSupported = true;
  static constexpr int kDisplayWidth = 1080;
  static constexpr int kDisplayHeight = 27282;
  static constexpr char kDisplayManufacture[] = "Samsung";
  static constexpr int kDisplayManufactureYear = 2020;
  static constexpr int kDisplayModelId = 54321;
  static constexpr char kDisplayName[] = "Internal display";

  const auto optional_result = CollectData(
      std::make_unique<CrosHealthdDisplaySamplerHandler>(
          CrosHealthdSamplerHandler::MetricType::kInfo),
      CreateDisplayResult(CreateEmbeddedDisplay(
                              kPrivacyScreenSupported, kDisplayWidth,
                              kDisplayHeight, /*resolution_horizontal*/ 1000,
                              /*resolution_vertical*/ 500, /*refresh_rate*/ 100,
                              kDisplayManufacture, kDisplayModelId,
                              kDisplayManufactureYear, kDisplayName),
                          std::vector<cros_healthd::ExternalDisplayInfoPtr>()),
      cros_healthd::ProbeCategoryEnum::kDisplay,
      CrosHealthdSamplerHandler::MetricType::kInfo);

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
  static constexpr bool kPrivacyScreenSupported = false;
  static constexpr int kDisplayWidth = 1080;
  static constexpr int kDisplayHeight = 27282;
  static constexpr char kDisplayManufacture[] = "Samsung";
  static constexpr int kDisplayManufactureYear = 2020;
  static constexpr int kDisplayModelId = 54321;
  static constexpr char kExternalDisplayName[] = "External display";
  static constexpr char kInternalDisplayName[] = "Internal display";

  // Create display results
  std::vector<cros_healthd::ExternalDisplayInfoPtr> external_displays;
  external_displays.push_back(CreateExternalDisplay(
      kDisplayWidth, kDisplayHeight, /*resolution_horizontal*/ 1000,
      /*resolution_vertical*/ 500, /*refresh_rate*/ 100, kDisplayManufacture,
      kDisplayModelId, kDisplayManufactureYear, kExternalDisplayName));
  external_displays.push_back(CreateExternalDisplay(
      kDisplayWidth, kDisplayHeight, /*resolution_horizontal*/ 1000,
      /*resolution_vertical*/ 500, /*refresh_rate*/ 100, kDisplayManufacture,
      kDisplayModelId, kDisplayManufactureYear, kExternalDisplayName));
  const auto optional_result = CollectData(
      std::make_unique<CrosHealthdDisplaySamplerHandler>(
          CrosHealthdSamplerHandler::MetricType::kInfo),
      CreateDisplayResult(CreateEmbeddedDisplay(
                              kPrivacyScreenSupported, kDisplayWidth,
                              kDisplayHeight, /*resolution_horizontal*/ 1000,
                              /*resolution_vertical*/ 500, /*refresh_rate*/ 100,
                              kDisplayManufacture, kDisplayModelId,
                              kDisplayManufactureYear, kInternalDisplayName),
                          std::move(external_displays)),
      cros_healthd::ProbeCategoryEnum::kDisplay,
      CrosHealthdSamplerHandler::MetricType::kInfo);

  // assertions
  ASSERT_TRUE(optional_result.has_value());
  const MetricData& result = optional_result.value();

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_display_info());
  ASSERT_EQ(result.info_data().display_info().display_device_size(), 3);

  ASSERT_TRUE(result.info_data().has_privacy_screen_info());
  ASSERT_FALSE(result.info_data().privacy_screen_info().supported());

  auto internal_display = result.info_data().display_info().display_device(0);
  EXPECT_EQ(internal_display.display_name(), kInternalDisplayName);
  EXPECT_EQ(internal_display.manufacturer(), kDisplayManufacture);
  EXPECT_EQ(internal_display.display_width(), kDisplayWidth);
  EXPECT_EQ(internal_display.display_height(), kDisplayHeight);
  EXPECT_EQ(internal_display.model_id(), kDisplayModelId);
  EXPECT_EQ(internal_display.manufacture_year(), kDisplayManufactureYear);

  auto external_display_1 = result.info_data().display_info().display_device(1);
  EXPECT_EQ(external_display_1.display_name(), kExternalDisplayName);
  EXPECT_EQ(external_display_1.manufacturer(), kDisplayManufacture);
  EXPECT_EQ(external_display_1.display_width(), kDisplayWidth);
  EXPECT_EQ(external_display_1.display_height(), kDisplayHeight);
  EXPECT_EQ(external_display_1.model_id(), kDisplayModelId);
  EXPECT_EQ(external_display_1.manufacture_year(), kDisplayManufactureYear);

  auto external_display_2 = result.info_data().display_info().display_device(2);
  EXPECT_EQ(external_display_2.display_name(), kExternalDisplayName);
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

  const auto optional_result = CollectData(
      std::make_unique<CrosHealthdDisplaySamplerHandler>(
          CrosHealthdSamplerHandler::MetricType::kTelemetry),
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
      CrosHealthdSamplerHandler::MetricType::kTelemetry);

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
  EXPECT_TRUE(internal_display.is_internal());
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

  const auto optional_result = CollectData(
      std::make_unique<CrosHealthdDisplaySamplerHandler>(
          CrosHealthdSamplerHandler::MetricType::kTelemetry),
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
      CrosHealthdSamplerHandler::MetricType::kTelemetry);

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
  EXPECT_TRUE(internal_display.is_internal());

  auto external_display_1 =
      result.telemetry_data().displays_telemetry().display_status(1);
  EXPECT_EQ(external_display_1.display_name(), kDisplayName);
  EXPECT_EQ(external_display_1.resolution_horizontal(), kResolutionHorizontal);
  EXPECT_EQ(external_display_1.resolution_vertical(), kResolutionVertical);
  EXPECT_EQ(external_display_1.refresh_rate(), kRefreshRate);
  EXPECT_FALSE(external_display_1.is_internal());

  auto external_display_2 =
      result.telemetry_data().displays_telemetry().display_status(2);
  EXPECT_EQ(external_display_2.display_name(), kDisplayName);
  EXPECT_EQ(external_display_2.resolution_horizontal(), kResolutionHorizontal);
  EXPECT_EQ(external_display_2.resolution_vertical(), kResolutionVertical);
  EXPECT_EQ(external_display_2.refresh_rate(), kRefreshRate);
  EXPECT_FALSE(external_display_2.is_internal());
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
    CrosHealthdMetricSamplerMemoryInfoTests,
    CrosHealthdMetricSamplerMemoryInfoTest,
    testing::ValuesIn<MemoryInfoTestCase>({
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
        CrosHealthdMetricSamplerMemoryInfoTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace reporting::test

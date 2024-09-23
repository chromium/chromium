// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <optional>

#include "base/functional/bind.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_info_metric_sampler_test_utils.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/mock_clock.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace reporting {
namespace {

namespace cros_healthd = ::ash::cros_healthd::mojom;

using ::chromeos::MissiveClientTestObserver;
using ::reporting::Destination;
using ::reporting::MetricData;
using ::reporting::Priority;
using ::reporting::Record;
using ::testing::Eq;
using ::testing::StrEq;

// Is the given record about info metric? If yes, return the underlying
// MetricData object.
std::optional<MetricData> IsRecordInfo(const Record& record) {
  if (record.destination() != Destination::INFO_METRIC) {
    return std::nullopt;
  }

  MetricData record_data;
  EXPECT_TRUE(record_data.ParseFromString(record.data()));
  EXPECT_TRUE(record_data.has_info_data());
  return record_data;
}

void AssertRecordData(Priority priority, const Record& record) {
  EXPECT_THAT(priority, Eq(Priority::SLOW_BATCH));
  EXPECT_THAT(record.destination(), Eq(Destination::INFO_METRIC));
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));
}

}  // namespace

// ---- Bus ----

class BusInfoSamplerBrowserTest : public policy::DevicePolicyCrosBrowserTest {
 public:
  BusInfoSamplerBrowserTest(const BusInfoSamplerBrowserTest&) = delete;
  BusInfoSamplerBrowserTest& operator=(const BusInfoSamplerBrowserTest&) =
      delete;

 protected:
  BusInfoSamplerBrowserTest() { test::MockClock::Get(); }
  ~BusInfoSamplerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ::ash::kReportDeviceSecurityStatus, true);
  }

  // Is the given record about Bus info metric?
  static bool IsRecordBusInfo(const Record& record) {
    auto record_data = IsRecordInfo(record);
    return record_data.has_value() &&
           record_data.value().info_data().has_bus_device_info();
  }

 private:
  ::ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(BusInfoSamplerBrowserTest, Thunderbolt) {
  static constexpr std::array<::reporting::ThunderboltSecurityLevel, 2>
      kErpSecurityLevels = {::reporting::ThunderboltSecurityLevel::
                                THUNDERBOLT_SECURITY_NONE_LEVEL,
                            ::reporting::ThunderboltSecurityLevel::
                                THUNDERBOLT_SECURITY_SECURE_LEVEL};
  const std::vector<cros_healthd::ThunderboltSecurityLevel>
      kHealthdSecurityLevels = {
          cros_healthd::ThunderboltSecurityLevel::kNone,
          cros_healthd::ThunderboltSecurityLevel::kSecureLevel};
  auto thunderbolt_bus_result =
      ::reporting::test::CreateThunderboltBusResult(kHealthdSecurityLevels);
  ::ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(thunderbolt_bus_result);
  MissiveClientTestObserver observer(base::BindRepeating(&IsRecordBusInfo));
  test::MockClock::Get().Advance(metrics::kInitialCollectionDelay);
  auto [priority, record] = observer.GetNextEnqueuedRecord();
  AssertRecordData(priority, record);
  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));
  EXPECT_TRUE(metric_data.has_timestamp_ms());
  ASSERT_TRUE(metric_data.has_info_data());
  const auto& info_data = metric_data.info_data();
  ASSERT_THAT(
      static_cast<size_t>(info_data.bus_device_info().thunderbolt_info_size()),
      Eq(kErpSecurityLevels.size()));
  for (size_t i = 0; i < kErpSecurityLevels.size(); ++i) {
    EXPECT_THAT(
        info_data.bus_device_info().thunderbolt_info(i).security_level(),
        Eq(kErpSecurityLevels[i]));
  }
}

// ---- CPU ----

class CpuInfoSamplerBrowserTest : public policy::DevicePolicyCrosBrowserTest {
 public:
  CpuInfoSamplerBrowserTest(const CpuInfoSamplerBrowserTest&) = delete;
  CpuInfoSamplerBrowserTest& operator=(const CpuInfoSamplerBrowserTest&) =
      delete;

 protected:
  CpuInfoSamplerBrowserTest() { test::MockClock::Get(); }
  ~CpuInfoSamplerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ::ash::kReportDeviceCpuInfo, true);
  }

  // Is the given record about CPU info metric?
  static bool IsRecordCpuInfo(const Record& record) {
    auto record_data = IsRecordInfo(record);
    return record_data.has_value() &&
           record_data.value().info_data().has_cpu_info();
  }

 private:
  ::ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(CpuInfoSamplerBrowserTest, KeylockerUnsupported) {
  auto cpu_result = ::reporting::test::CreateCpuResult(nullptr);
  ::ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(cpu_result);
  MissiveClientTestObserver observer(base::BindRepeating(&IsRecordCpuInfo));
  test::MockClock::Get().Advance(metrics::kInitialCollectionDelay);
  auto [priority, record] = observer.GetNextEnqueuedRecord();
  AssertRecordData(priority, record);
  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));
  EXPECT_TRUE(metric_data.has_timestamp_ms());
  ASSERT_TRUE(metric_data.has_info_data());
  const auto& info_data = metric_data.info_data();
  ASSERT_TRUE(info_data.cpu_info().has_keylocker_info());
  EXPECT_FALSE(info_data.cpu_info().keylocker_info().configured());
  EXPECT_FALSE(info_data.cpu_info().keylocker_info().supported());
}

IN_PROC_BROWSER_TEST_F(CpuInfoSamplerBrowserTest, KeylockerConfigured) {
  auto cpu_result = ::reporting::test::CreateCpuResult(
      ::reporting::test::CreateKeylockerInfo(true));
  ::ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(cpu_result);
  MissiveClientTestObserver observer(base::BindRepeating(&IsRecordCpuInfo));
  test::MockClock::Get().Advance(metrics::kInitialCollectionDelay);
  auto [priority, record] = observer.GetNextEnqueuedRecord();
  AssertRecordData(priority, record);
  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));
  EXPECT_TRUE(metric_data.has_timestamp_ms());
  ASSERT_TRUE(metric_data.has_info_data());
  const auto& info_data = metric_data.info_data();
  ASSERT_TRUE(info_data.cpu_info().has_keylocker_info());
  EXPECT_TRUE(info_data.cpu_info().keylocker_info().configured());
  EXPECT_TRUE(info_data.cpu_info().keylocker_info().supported());
}

// ---- Memory ----

// Memory constants.
static constexpr int64_t kTmeMaxKeys = 2;
static constexpr int64_t kTmeKeysLength = 4;

class MemoryInfoSamplerBrowserTest
    : public policy::DevicePolicyCrosBrowserTest,
      public testing::WithParamInterface<
          ::reporting::test::MemoryInfoTestCase> {
 public:
  MemoryInfoSamplerBrowserTest(const MemoryInfoSamplerBrowserTest&) = delete;
  MemoryInfoSamplerBrowserTest& operator=(const MemoryInfoSamplerBrowserTest&) =
      delete;

 protected:
  MemoryInfoSamplerBrowserTest() { test::MockClock::Get(); }
  ~MemoryInfoSamplerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ::ash::kReportDeviceMemoryInfo, true);
  }

  // Is the given record about memory info metric?
  static bool IsRecordMemoryInfo(const Record& record) {
    auto record_data = IsRecordInfo(record);
    return record_data.has_value() &&
           record_data.value().info_data().has_memory_info();
  }

  static void AssertMemoryInfo(MissiveClientTestObserver* observer) {
    auto [priority, record] = observer->GetNextEnqueuedRecord();
    AssertRecordData(priority, record);
    MetricData metric_data;
    ASSERT_TRUE(metric_data.ParseFromString(record.data()));
    EXPECT_TRUE(metric_data.has_timestamp_ms());
    ASSERT_TRUE(metric_data.has_info_data());
    ::reporting::test::AssertMemoryInfo(metric_data, GetParam());
  }

 private:
  ::ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_P(MemoryInfoSamplerBrowserTest, ReportMemoryInfo) {
  const auto& test_case = GetParam();
  auto memory_result = ::reporting::test::CreateMemoryResult(
      ::reporting::test::CreateMemoryEncryptionInfo(
          test_case.healthd_encryption_state, test_case.max_keys,
          test_case.key_length, test_case.healthd_encryption_algorithm));

  ::ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(memory_result);
  MissiveClientTestObserver observer(base::BindRepeating(&IsRecordMemoryInfo));
  test::MockClock::Get().Advance(metrics::kInitialCollectionDelay);
  AssertMemoryInfo(&observer);
}

INSTANTIATE_TEST_SUITE_P(
    MemoryInfoSamplerBrowserTests,
    MemoryInfoSamplerBrowserTest,
    testing::ValuesIn<::reporting::test::MemoryInfoTestCase>({
        {"UnknownEncryptionState", cros_healthd::EncryptionState::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_STATE_UNKNOWN,
         cros_healthd::CryptoAlgorithm::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_ALGORITHM_UNKNOWN, 0, 0},
        {"KeyValuesSet", cros_healthd::EncryptionState::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_STATE_UNKNOWN,
         cros_healthd::CryptoAlgorithm::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_ALGORITHM_UNKNOWN, kTmeMaxKeys,
         kTmeKeysLength},
    }),
    [](const testing::TestParamInfo<MemoryInfoSamplerBrowserTest::ParamType>&
           info) { return info.param.test_name; });

// ---- Input ----

class InputInfoSamplerBrowserTest : public policy::DevicePolicyCrosBrowserTest {
 public:
  InputInfoSamplerBrowserTest(const InputInfoSamplerBrowserTest&) = delete;
  InputInfoSamplerBrowserTest& operator=(const InputInfoSamplerBrowserTest&) =
      delete;

 protected:
  InputInfoSamplerBrowserTest() { test::MockClock::Get(); }
  ~InputInfoSamplerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ::ash::kReportDeviceGraphicsStatus, true);
  }

  // Is the given record about touchscreen info metric?
  static bool IsRecordTouchScreenInfo(const Record& record) {
    auto record_data = IsRecordInfo(record);
    return record_data.has_value() &&
           record_data.value().info_data().has_touch_screen_info();
  }

 private:
  ::ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(InputInfoSamplerBrowserTest,
                       TouchScreenMultipleInternal) {
  static constexpr char kSampleLibrary[] = "SampleLibrary";
  static constexpr char kSampleDevice[] = "SampleDevice";
  static constexpr char kSampleDevice2[] = "SampleDevice2";
  static constexpr int kTouchPoints = 10;
  static constexpr int kTouchPoints2 = 5;

  // Create the test result.
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
  auto input_result = ::reporting::test::CreateInputResult(
      kSampleLibrary, std::move(touchscreen_devices));
  ::ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(input_result);
  MissiveClientTestObserver observer(
      base::BindRepeating(&IsRecordTouchScreenInfo));
  test::MockClock::Get().Advance(metrics::kInitialCollectionDelay);

  // Assertions
  auto [priority, record] = observer.GetNextEnqueuedRecord();
  AssertRecordData(priority, record);
  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));
  EXPECT_TRUE(metric_data.has_timestamp_ms());
  ASSERT_TRUE(metric_data.has_info_data());
  const auto& info_data = metric_data.info_data();
  ASSERT_TRUE(info_data.has_touch_screen_info());
  ASSERT_TRUE(info_data.touch_screen_info().has_library_name());
  EXPECT_THAT(info_data.touch_screen_info().library_name(),
              StrEq(kSampleLibrary));
  ASSERT_EQ(info_data.touch_screen_info().touch_screen_devices().size(), 2);
  EXPECT_THAT(
      info_data.touch_screen_info().touch_screen_devices(0).display_name(),
      StrEq(kSampleDevice));
  EXPECT_THAT(
      info_data.touch_screen_info().touch_screen_devices(0).touch_points(),
      Eq(kTouchPoints));
  EXPECT_TRUE(
      info_data.touch_screen_info().touch_screen_devices(0).has_stylus());
  EXPECT_THAT(
      info_data.touch_screen_info().touch_screen_devices(1).display_name(),
      StrEq(kSampleDevice2));
  EXPECT_THAT(
      info_data.touch_screen_info().touch_screen_devices(1).touch_points(),
      Eq(kTouchPoints2));
  EXPECT_FALSE(
      info_data.touch_screen_info().touch_screen_devices(1).has_stylus());
}

// ---- Display ----

class DisplayInfoSamplerBrowserTest
    : public policy::DevicePolicyCrosBrowserTest {
 public:
  DisplayInfoSamplerBrowserTest(const DisplayInfoSamplerBrowserTest&) = delete;
  DisplayInfoSamplerBrowserTest& operator=(
      const DisplayInfoSamplerBrowserTest&) = delete;

 protected:
  DisplayInfoSamplerBrowserTest() { test::MockClock::Get(); }
  ~DisplayInfoSamplerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ::ash::kReportDeviceGraphicsStatus, true);
  }

  // Is the given record about display info metric?
  static bool IsRecordDisplayInfo(const Record& record) {
    auto record_data = IsRecordInfo(record);
    return record_data.has_value() &&
           record_data.value().info_data().has_display_info();
  }

 private:
  ::ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(DisplayInfoSamplerBrowserTest, MultipleDisplays) {
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
  external_displays.push_back(::reporting::test::CreateExternalDisplay(
      kDisplayWidth, kDisplayHeight, /*resolution_horizontal*/ 1000,
      /*resolution_vertical*/ 500, /*refresh_rate*/ 100, kDisplayManufacture,
      kDisplayModelId, kDisplayManufactureYear, kExternalDisplayName));
  external_displays.push_back(::reporting::test::CreateExternalDisplay(
      kDisplayWidth, kDisplayHeight, /*resolution_horizontal*/ 1000,
      /*resolution_vertical*/ 500, /*refresh_rate*/ 100, kDisplayManufacture,
      kDisplayModelId, kDisplayManufactureYear, kExternalDisplayName));
  auto display_result = ::reporting::test::CreateDisplayResult(
      ::reporting::test::CreateEmbeddedDisplay(
          kPrivacyScreenSupported, kDisplayWidth, kDisplayHeight,
          /*resolution_horizontal*/ 1000,
          /*resolution_vertical*/ 500, /*refresh_rate*/ 100,
          kDisplayManufacture, kDisplayModelId, kDisplayManufactureYear,
          kInternalDisplayName),
      std::move(external_displays));
  ::ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(display_result);
  MissiveClientTestObserver observer(base::BindRepeating(&IsRecordDisplayInfo));
  test::MockClock::Get().Advance(metrics::kInitialCollectionDelay);

  // assertions
  auto [priority, record] = observer.GetNextEnqueuedRecord();
  AssertRecordData(priority, record);
  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));
  EXPECT_TRUE(metric_data.has_timestamp_ms());
  ASSERT_TRUE(metric_data.has_info_data());
  const auto& info_data = metric_data.info_data();
  ASSERT_TRUE(info_data.has_display_info());
  ASSERT_EQ(info_data.display_info().display_device_size(), 3);

  ASSERT_TRUE(info_data.has_privacy_screen_info());
  ASSERT_FALSE(info_data.privacy_screen_info().supported());

  auto internal_display = info_data.display_info().display_device(0);
  EXPECT_EQ(internal_display.display_name(), kInternalDisplayName);
  EXPECT_EQ(internal_display.manufacturer(), kDisplayManufacture);
  EXPECT_EQ(internal_display.display_width(), kDisplayWidth);
  EXPECT_EQ(internal_display.display_height(), kDisplayHeight);
  EXPECT_EQ(internal_display.model_id(), kDisplayModelId);
  EXPECT_EQ(internal_display.manufacture_year(), kDisplayManufactureYear);

  auto external_display_1 = info_data.display_info().display_device(1);
  EXPECT_EQ(external_display_1.display_name(), kExternalDisplayName);
  EXPECT_EQ(external_display_1.manufacturer(), kDisplayManufacture);
  EXPECT_EQ(external_display_1.display_width(), kDisplayWidth);
  EXPECT_EQ(external_display_1.display_height(), kDisplayHeight);
  EXPECT_EQ(external_display_1.model_id(), kDisplayModelId);
  EXPECT_EQ(external_display_1.manufacture_year(), kDisplayManufactureYear);

  auto external_display_2 = info_data.display_info().display_device(2);
  EXPECT_EQ(external_display_2.display_name(), kExternalDisplayName);
  EXPECT_EQ(external_display_2.manufacturer(), kDisplayManufacture);
  EXPECT_EQ(external_display_2.display_width(), kDisplayWidth);
  EXPECT_EQ(external_display_2.display_height(), kDisplayHeight);
  EXPECT_EQ(external_display_2.model_id(), kDisplayModelId);
  EXPECT_EQ(external_display_2.manufacture_year(), kDisplayManufactureYear);
}
}  // namespace reporting

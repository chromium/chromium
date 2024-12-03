// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/system/system_data_provider.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ash/system/diagnostics/diagnostics_log_controller.h"
#include "ash/system/diagnostics/fake_diagnostics_browser_delegate.h"
#include "ash/system/diagnostics/telemetry_log.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/diagnostics_ui/backend/common/histogram_util.h"
#include "ash/webui/diagnostics_ui/backend/system/cpu_usage_data.h"
#include "ash/webui/diagnostics_ui/backend/system/power_manager_client_conversions.h"
#include "ash/webui/diagnostics_ui/mojom/system_data_provider.mojom-forward.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-forward.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-shared.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash::diagnostics {

namespace {

namespace healthd_mojom = cros_healthd::mojom;

constexpr char kSystemDataError[] = "ChromeOS.DiagnosticsUi.Error.System";
constexpr char kBatteryDataError[] = "ChromeOS.DiagnosticsUi.Error.Battery";

constexpr char kProbeErrorBatteryInfo[] =
    "ChromeOS.DiagnosticsUi.Error.CrosHealthdProbeError.BatteryInfo";
constexpr char kProbeErrorCpuInfo[] =
    "ChromeOS.DiagnosticsUi.Error.CrosHealthdProbeError.CpuInfo";
constexpr char kProbeErrorMemoryInfo[] =
    "ChromeOS.DiagnosticsUi.Error.CrosHealthdProbeError.MemoryInfo";
constexpr char kProbeErrorSystemInfo[] =
    "ChromeOS.DiagnosticsUi.Error.CrosHealthdProbeError.SystemInfo";

void SetProbeTelemetryInfoResponse(healthd_mojom::BatteryInfoPtr battery_info,
                                   healthd_mojom::CpuInfoPtr cpu_info,
                                   healthd_mojom::MemoryInfoPtr memory_info,
                                   healthd_mojom::SystemInfoPtr system_info) {
  auto info = healthd_mojom::TelemetryInfo::New();
  if (system_info) {
    info->system_result =
        healthd_mojom::SystemResult::NewSystemInfo(std::move(system_info));
  }
  if (battery_info) {
    info->battery_result =
        healthd_mojom::BatteryResult::NewBatteryInfo(std::move(battery_info));
  }
  if (memory_info) {
    info->memory_result =
        healthd_mojom::MemoryResult::NewMemoryInfo(std::move(memory_info));
  }
  if (cpu_info) {
    info->cpu_result =
        healthd_mojom::CpuResult::NewCpuInfo(std::move(cpu_info));
  }

  cros_healthd::FakeCrosHealthd::Get()->SetProbeTelemetryInfoResponseForTesting(
      info);
}

void SetCrosHealthdSystemInfoResponse(const std::string& board_name,
                                      const std::string& marketing_name,
                                      const std::string& cpu_model,
                                      uint32_t total_memory_kib,
                                      uint16_t cpu_threads_count,
                                      uint32_t cpu_max_clock_speed_khz,
                                      bool has_battery,
                                      const std::string& milestone_version,
                                      const std::string& build_number,
                                      const std::string& patch_number) {
  // System info
  auto os_info = healthd_mojom::OsInfo::New();
  os_info->code_name = board_name;
  os_info->marketing_name = marketing_name;
  os_info->os_version = healthd_mojom::OsVersion::New(
      milestone_version, build_number, patch_number, "unittest-channel");
  auto system_info = healthd_mojom::SystemInfo::New();
  system_info->os_info = std::move(os_info);

  // Battery info
  auto battery_info = has_battery ? healthd_mojom::BatteryInfo::New() : nullptr;

  // Memory info
  auto memory_info = healthd_mojom::MemoryInfo::New();
  memory_info->total_memory_kib = total_memory_kib;

  // CPU info
  auto cpu_info = healthd_mojom::CpuInfo::New();
  auto physical_cpu_info = healthd_mojom::PhysicalCpuInfo::New();
  auto logical_cpu_info = healthd_mojom::LogicalCpuInfo::New();
  logical_cpu_info->max_clock_speed_khz = cpu_max_clock_speed_khz;
  physical_cpu_info->logical_cpus.push_back(std::move(logical_cpu_info));
  physical_cpu_info->model_name = cpu_model;
  cpu_info->num_total_threads = cpu_threads_count;
  cpu_info->physical_cpus.emplace_back(std::move(physical_cpu_info));

  SetProbeTelemetryInfoResponse(std::move(battery_info), std::move(cpu_info),
                                std::move(memory_info), std::move(system_info));
}

// Constructs a BatteryInfoPtr. If |temperature| = 0, it will be omitted from
// the response to simulate an empty temperature field.
healthd_mojom::BatteryInfoPtr CreateCrosHealthdBatteryInfoResponse(
    int64_t cycle_count,
    double voltage_now,
    const std::string& vendor,
    const std::string& serial_number,
    double charge_full_design,
    double charge_full,
    double voltage_min_design,
    const std::string& model_name,
    double charge_now,
    double current_now,
    const std::string& technology,
    const std::string& status,
    const std::optional<std::string>& manufacture_date,
    uint64_t temperature) {
  healthd_mojom::NullableUint64Ptr temp_value_ptr(
      healthd_mojom::NullableUint64::New());
  if (temperature != 0) {
    temp_value_ptr->value = temperature;
  }
  auto battery_info = healthd_mojom::BatteryInfo::New(
      cycle_count, voltage_now, vendor, serial_number, charge_full_design,
      charge_full, voltage_min_design, model_name, charge_now, current_now,
      technology, status, manufacture_date, std::move(temp_value_ptr));
  return battery_info;
}

healthd_mojom::BatteryInfoPtr CreateCrosHealthdBatteryInfoResponse(
    const std::string& vendor,
    double charge_full_design) {
  return CreateCrosHealthdBatteryInfoResponse(
      /*cycle_count=*/0,
      /*voltage_now=*/0,
      /*vendor=*/vendor,
      /*serial_number=*/"",
      /*charge_full_design=*/charge_full_design,
      /*charge_full=*/0,
      /*voltage_min_design=*/0,
      /*model_name=*/"",
      /*charge_now=*/0,
      /*current_now=*/0,
      /*technology=*/"",
      /*status=*/"",
      /*manufacture_date=*/std::nullopt,
      /*temperature=*/0);
}

healthd_mojom::BatteryInfoPtr CreateCrosHealthdBatteryChargeStatusResponse(
    double charge_now,
    double current_now) {
  return CreateCrosHealthdBatteryInfoResponse(
      /*cycle_count=*/0,
      /*voltage_now=*/0,
      /*vendor=*/"",
      /*serial_number=*/"",
      /*charge_full_design=*/0,
      /*charge_full=*/0,
      /*voltage_min_design=*/0,
      /*model_name=*/"",
      /*charge_now=*/charge_now,
      /*current_now=*/current_now,
      /*technology=*/"",
      /*status=*/"",
      /*manufacture_date=*/std::nullopt,
      /*temperature=*/0);
}

healthd_mojom::BatteryInfoPtr CreateCrosHealthdBatteryHealthResponse(
    double charge_full_now,
    double charge_full_design,
    int32_t cycle_count) {
  return CreateCrosHealthdBatteryInfoResponse(
      /*cycle_count=*/cycle_count,
      /*voltage_now=*/0,
      /*vendor=*/"",
      /*serial_number=*/"",
      /*charge_full_design=*/charge_full_design,
      /*charge_full=*/charge_full_now,
      /*voltage_min_design=*/0,
      /*model_name=*/"",
      /*charge_now=*/0,
      /*current_now=*/0,
      /*technology=*/"",
      /*status=*/"",
      /*manufacture_date=*/std::nullopt,
      /*temperature=*/0);
}

healthd_mojom::ProbeErrorPtr CreateProbeError(
    healthd_mojom::ErrorType error_type) {
  auto probe_error = healthd_mojom::ProbeError::New();
  probe_error->type = error_type;
  probe_error->msg = "probe error";
  return probe_error;
}

void SetCrosHealthdBatteryInfoResponse(const std::string& vendor,
                                       double charge_full_design) {
  healthd_mojom::BatteryInfoPtr battery_info =
      CreateCrosHealthdBatteryInfoResponse(vendor, charge_full_design);
  SetProbeTelemetryInfoResponse(std::move(battery_info), /*cpu_info=*/nullptr,
                                /*memory_info=*/nullptr,
                                /*system_info=*/nullptr);
}

void SetCrosHealthdBatteryChargeStatusResponse(double charge_now,
                                               double current_now) {
  healthd_mojom::BatteryInfoPtr battery_info =
      CreateCrosHealthdBatteryChargeStatusResponse(charge_now, current_now);
  SetProbeTelemetryInfoResponse(std::move(battery_info), /*cpu_info=*/nullptr,
                                /*memory_info=*/nullptr,
                                /*system_info=*/nullptr);
}

void SetCrosHealthdBatteryHealthResponse(double charge_full_now,
                                         double charge_full_design,
                                         int32_t cycle_count) {
  healthd_mojom::BatteryInfoPtr battery_info =
      CreateCrosHealthdBatteryHealthResponse(charge_full_now,
                                             charge_full_design, cycle_count);
  SetProbeTelemetryInfoResponse(std::move(battery_info), /*cpu_info=*/nullptr,
                                /*memory_info=*/nullptr,
                                /*system_info=*/nullptr);
}

void SetCrosHealthdMemoryUsageResponse(uint32_t total_memory_kib,
                                       uint32_t free_memory_kib,
                                       uint32_t available_memory_kib) {
  healthd_mojom::MemoryInfoPtr memory_info = healthd_mojom::MemoryInfo::New(
      total_memory_kib, free_memory_kib, available_memory_kib,
      /*page_faults_since_last_boot=*/0);
  SetProbeTelemetryInfoResponse(/*battery_info=*/nullptr, /*cpu_info=*/nullptr,
                                /*memory_info=*/std::move(memory_info),
                                /*system_info=*/nullptr);
}

void SetCrosHealthdCpuResponse(
    const std::vector<CpuUsageData>& usage_data,
    const std::vector<int32_t>& cpu_temps,
    const std::vector<uint32_t>& scaled_cpu_clock_speed) {
  auto cpu_info_ptr = healthd_mojom::CpuInfo::New();
  auto physical_cpu_info_ptr = healthd_mojom::PhysicalCpuInfo::New();

  DCHECK_EQ(usage_data.size(), scaled_cpu_clock_speed.size());
  for (size_t i = 0; i < usage_data.size(); ++i) {
    const auto& data = usage_data[i];
    auto logical_cpu_info_ptr = healthd_mojom::LogicalCpuInfo::New();

    logical_cpu_info_ptr->user_time_user_hz = data.GetUserTime();
    logical_cpu_info_ptr->system_time_user_hz = data.GetSystemTime();
    logical_cpu_info_ptr->idle_time_user_hz = data.GetIdleTime();

    logical_cpu_info_ptr->scaling_current_frequency_khz =
        scaled_cpu_clock_speed[i];

    physical_cpu_info_ptr->logical_cpus.emplace_back(
        std::move(logical_cpu_info_ptr));
  }

  cpu_info_ptr->physical_cpus.push_back(std::move(physical_cpu_info_ptr));
  for (const auto& cpu_temp : cpu_temps) {
    auto cpu_temp_channel_ptr = healthd_mojom::CpuTemperatureChannel::New();
    cpu_temp_channel_ptr->temperature_celsius = cpu_temp;
    cpu_info_ptr->temperature_channels.emplace_back(
        std::move(cpu_temp_channel_ptr));
  }

  SetProbeTelemetryInfoResponse(/*battery_info=*/nullptr,
                                std::move(cpu_info_ptr),
                                /*memory_info=*/nullptr,
                                /*system_info=*/nullptr);
}

// Sets the CpuUsage response on cros_healthd. |usage_data| should contain one
// entry for each logical cpu.
void SetCrosHealthdCpuUsageResponse(
    const std::vector<CpuUsageData>& usage_data) {
  // Use fake temp and scaled clock speed data since none was supplied.
  const std::vector<uint32_t> scaled_clock_speeds(usage_data.size(), 10000);
  SetCrosHealthdCpuResponse(usage_data, {50}, scaled_clock_speeds);
}

void SetCrosHealthdCpuTemperatureResponse(
    const std::vector<int32_t>& cpu_temps) {
  // Use fake usage_data and scaled clock speed data since none was supplied.
  SetCrosHealthdCpuResponse({CpuUsageData(1000, 1000, 1000)}, cpu_temps,
                            {10000});
}

void SetCrosHealthdCpuScalingResponse(const std::vector<uint32_t>& cpu_speeds) {
  // Use fake temp and usage_data data since none was supplied.
  const std::vector<CpuUsageData> usage_data(cpu_speeds.size(),
                                             CpuUsageData(1000, 1000, 1000));
  SetCrosHealthdCpuResponse(usage_data, {50}, cpu_speeds);
}

bool AreValidPowerTimes(int64_t time_to_full, int64_t time_to_empty) {
  // Exactly one of |time_to_full| or |time_to_empty| must be zero. The other
  // can be a positive integer to represent the time to charge/discharge or -1
  // to represent that the time is being calculated.
  return (time_to_empty == 0 && (time_to_full > 0 || time_to_full == -1)) ||
         (time_to_full == 0 && (time_to_empty > 0 || time_to_empty == -1));
}

power_manager::PowerSupplyProperties ConstructPowerSupplyProperties(
    power_manager::PowerSupplyProperties::ExternalPower power_source,
    power_manager::PowerSupplyProperties::BatteryState battery_state,
    bool is_calculating_battery_time,
    int64_t time_to_full,
    int64_t time_to_empty) {
  power_manager::PowerSupplyProperties props;
  props.set_external_power(power_source);
  props.set_battery_state(battery_state);

  if (battery_state ==
      power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT) {
    // Leave |time_to_full| and |time_to_empty| unset.
    return props;
  }

  DCHECK(AreValidPowerTimes(time_to_full, time_to_empty));

  props.set_is_calculating_battery_time(is_calculating_battery_time);
  props.set_battery_time_to_full_sec(time_to_full);
  props.set_battery_time_to_empty_sec(time_to_empty);

  return props;
}

// Sets the PowerSupplyProperties on FakePowerManagerClient. Calling this
// method immediately notifies PowerManagerClient observers. One of
// |time_to_full| or |time_to_empty| must be either -1 or a positive number.
// The other must be 0. If |battery_state| is NOT_PRESENT, both |time_to_full|
// and |time_to_empty| will be left unset.
void SetPowerManagerProperties(
    power_manager::PowerSupplyProperties::ExternalPower power_source,
    power_manager::PowerSupplyProperties::BatteryState battery_state,
    bool is_calculating_battery_time,
    int64_t time_to_full,
    int64_t time_to_empty) {
  power_manager::PowerSupplyProperties props = ConstructPowerSupplyProperties(
      power_source, battery_state, is_calculating_battery_time, time_to_full,
      time_to_empty);
  chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(props);
}

healthd_mojom::SystemInfoPtr GetDefaultSystemInfoPtr() {
  auto os_info = healthd_mojom::OsInfo::New();
  os_info->code_name = "board_name";
  os_info->marketing_name = "marketing_name";
  os_info->os_version =
      healthd_mojom::OsVersion::New("M99", "1234", "5.6", "unittest-channel");
  auto system_info = healthd_mojom::SystemInfo::New();
  system_info->os_info = std::move(os_info);

  return system_info;
}

void VerifyChargeStatusResult(
    const mojom::BatteryChargeStatusPtr& update,
    double charge_now,
    double current_now,
    power_manager::PowerSupplyProperties::ExternalPower power_source,
    power_manager::PowerSupplyProperties::BatteryState battery_state,
    bool is_calculating_battery_time,
    int64_t time_to_full,
    int64_t time_to_empty) {
  const uint32_t expected_charge_now_milliamp_hours = charge_now * 1000;
  const int32_t expected_current_now_milliamps = current_now * 1000;
  mojom::ExternalPowerSource expected_power_source =
      ConvertPowerSourceFromProto(power_source);
  mojom::BatteryState expected_battery_state =
      ConvertBatteryStateFromProto(battery_state);

  EXPECT_EQ(expected_charge_now_milliamp_hours,
            update->charge_now_milliamp_hours);
  EXPECT_EQ(expected_current_now_milliamps, update->current_now_milliamps);
  EXPECT_EQ(expected_power_source, update->power_adapter_status);
  EXPECT_EQ(expected_battery_state, update->battery_state);

  if (expected_battery_state == mojom::BatteryState::kFull) {
    EXPECT_EQ(std::u16string(), update->power_time);
    return;
  }

  DCHECK(AreValidPowerTimes(time_to_full, time_to_empty));

  const power_manager::PowerSupplyProperties props =
      ConstructPowerSupplyProperties(power_source, battery_state,
                                     is_calculating_battery_time, time_to_full,
                                     time_to_empty);
  std::u16string expected_power_time =
      ConstructPowerTime(expected_battery_state, props);

  EXPECT_EQ(expected_power_time, update->power_time);
}

void VerifyHealthResult(const mojom::BatteryHealthPtr& update,
                        double charge_full_now,
                        double charge_full_design,
                        int32_t expected_cycle_count) {
  const int32_t expected_charge_full_now_milliamp_hours =
      charge_full_now * 1000;
  const int32_t expected_charge_full_design_milliamp_hours =
      charge_full_design * 1000;
  const int8_t expected_battery_wear_percentage =
      100 * expected_charge_full_now_milliamp_hours /
      expected_charge_full_design_milliamp_hours;

  EXPECT_EQ(expected_charge_full_now_milliamp_hours,
            update->charge_full_now_milliamp_hours);
  EXPECT_EQ(expected_charge_full_design_milliamp_hours,
            update->charge_full_design_milliamp_hours);
  EXPECT_EQ(expected_cycle_count, update->cycle_count);
  EXPECT_EQ(expected_battery_wear_percentage, update->battery_wear_percentage);
}

void VerifyMemoryUsageResult(const mojom::MemoryUsagePtr& update,
                             uint32_t expected_total_memory_kib,
                             uint32_t expected_free_memory_kib,
                             uint32_t expected_available_memory_kib) {
  EXPECT_EQ(expected_total_memory_kib, update->total_memory_kib);
  EXPECT_EQ(expected_free_memory_kib, update->free_memory_kib);
  EXPECT_EQ(expected_available_memory_kib, update->available_memory_kib);
}

void VerifyCpuUsageResult(const mojom::CpuUsagePtr& update,
                          uint8_t expected_percent_user,
                          uint8_t expected_percent_system,
                          uint8_t expected_percent_free) {
  EXPECT_EQ(expected_percent_user, update->percent_usage_user);
  EXPECT_EQ(expected_percent_system, update->percent_usage_system);
  EXPECT_EQ(expected_percent_free, update->percent_usage_free);
}

void VerifyCpuTempResult(const mojom::CpuUsagePtr& update,
                         uint32_t expected_average_temp) {
  EXPECT_EQ(expected_average_temp, update->average_cpu_temp_celsius);
}

void VerifyCpuScalingResult(const mojom::CpuUsagePtr& update,
                            uint32_t expected_scaled_speed) {
  EXPECT_EQ(expected_scaled_speed, update->scaling_current_frequency_khz);
}

void VerifySystemDataErrorBucketCounts(
    const base::HistogramTester& tester,
    size_t expected_no_data_error,
    size_t expected_not_a_number_error,
    size_t expected_expectation_not_met_error) {
  tester.ExpectBucketCount(kSystemDataError, metrics::DataError::kNoData,
                           expected_no_data_error);
  tester.ExpectBucketCount(kSystemDataError, metrics::DataError::kNotANumber,
                           expected_not_a_number_error);
  tester.ExpectBucketCount(kSystemDataError,
                           metrics::DataError::kExpectationNotMet,
                           expected_expectation_not_met_error);
}

void VerifyBatteryDataErrorBucketCounts(
    const base::HistogramTester& tester,
    size_t expected_no_data_error,
    size_t expected_not_a_number_error,
    size_t expected_expectation_not_met_error) {
  tester.ExpectBucketCount(kBatteryDataError, metrics::DataError::kNoData,
                           expected_no_data_error);
  tester.ExpectBucketCount(kBatteryDataError, metrics::DataError::kNotANumber,
                           expected_not_a_number_error);
  tester.ExpectBucketCount(kBatteryDataError,
                           metrics::DataError::kExpectationNotMet,
                           expected_expectation_not_met_error);
}

void VerifyProbeErrorBucketCounts(const base::HistogramTester& tester,
                                  const std::string& metric_name,
                                  size_t expected_unknown_error,
                                  size_t expected_parse_error,
                                  size_t expected_service_unavailable,
                                  size_t expected_system_utility_error,
                                  size_t expected_file_read_error) {
  tester.ExpectBucketCount(metric_name, healthd_mojom::ErrorType::kUnknown,
                           expected_unknown_error);
  tester.ExpectBucketCount(metric_name, healthd_mojom::ErrorType::kParseError,
                           expected_parse_error);
  tester.ExpectBucketCount(metric_name,
                           healthd_mojom::ErrorType::kServiceUnavailable,
                           expected_service_unavailable);
  tester.ExpectBucketCount(metric_name,
                           healthd_mojom::ErrorType::kSystemUtilityError,
                           expected_system_utility_error);
  tester.ExpectBucketCount(metric_name,
                           healthd_mojom::ErrorType::kFileReadError,
                           expected_file_read_error);
}

}  // namespace

struct FakeBatteryChargeStatusObserver
    : public mojom::BatteryChargeStatusObserver {
  // mojom::BatteryChargeStatusObserver
  void OnBatteryChargeStatusUpdated(
      mojom::BatteryChargeStatusPtr status_ptr) override {
    updates.push_back(std::move(status_ptr));
  }

  // Tracks calls to OnBatteryChargeStatusUpdated. Each call adds an element to
  // the vector.
  std::vector<mojom::BatteryChargeStatusPtr> updates;

  mojo::Receiver<mojom::BatteryChargeStatusObserver> receiver{this};
};

struct FakeBatteryHealthObserver : public mojom::BatteryHealthObserver {
  // mojom::BatteryHealthObserver
  void OnBatteryHealthUpdated(mojom::BatteryHealthPtr status_ptr) override {
    updates.push_back(std::move(status_ptr));
  }

  // Tracks calls to OnBatteryHealthUpdated. Each call adds an element to
  // the vector.
  std::vector<mojom::BatteryHealthPtr> updates;

  mojo::Receiver<mojom::BatteryHealthObserver> receiver{this};
};

struct FakeMemoryUsageObserver : public mojom::MemoryUsageObserver {
  // mojom::MemoryUsageObserver
  void OnMemoryUsageUpdated(mojom::MemoryUsagePtr status_ptr) override {
    updates.push_back(std::move(status_ptr));
  }

  // Tracks calls to OnMemoryUsageUpdated. Each call adds an element to
  // the vector.
  std::vector<mojom::MemoryUsagePtr> updates;

  mojo::Receiver<mojom::MemoryUsageObserver> receiver{this};
};

struct FakeCpuUsageObserver : public mojom::CpuUsageObserver {
  // mojom::CpuUsageObserver
  void OnCpuUsageUpdated(mojom::CpuUsagePtr status_ptr) override {
    updates.push_back(std::move(status_ptr));
  }

  // Tracks calls to OnCpuUsageUpdated. Each call adds an element to
  // the vector.
  std::vector<mojom::CpuUsagePtr> updates;

  mojo::Receiver<mojom::CpuUsageObserver> receiver{this};
};

class SystemDataProviderTest : public AshTestBase {
 public:
  SystemDataProviderTest() = default;

  SystemDataProviderTest(const SystemDataProviderTest&) = delete;
  SystemDataProviderTest& operator=(const SystemDataProviderTest&) = delete;

  ~SystemDataProviderTest() override = default;

  void SetUp() override {
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();
    base::RunLoop().RunUntilIdle();

    cros_healthd::FakeCrosHealthd::Initialize();
    system_data_provider_ = std::make_unique<SystemDataProvider>();
    DiagnosticsLogController::Initialize(
        std::make_unique<FakeDiagnosticsBrowserDelegate>());
  }

  void TearDown() override {
    system_data_provider_.reset();
    cros_healthd::FakeCrosHealthd::Shutdown();
    base::RunLoop().RunUntilIdle();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<SystemDataProvider> system_data_provider_;

 private:
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
};

TEST_F(SystemDataProviderTest, GetSystemInfo) {
  const std::string expected_board_name = "board_name";
  const std::string expected_marketing_name = "marketing_name";
  const std::string expected_cpu_model = "cpu_model";
  const uint32_t expected_total_memory_kib = 1234;
  const uint16_t expected_cpu_threads_count = 5678;
  const uint32_t expected_cpu_max_clock_speed_khz = 91011;
  const bool expected_has_battery = true;
  const std::string expected_milestone_version = "M99";
  const std::string expected_build_number = "1234";
  const std::string expected_patch_number = "5.6";

  SetCrosHealthdSystemInfoResponse(
      expected_board_name, expected_marketing_name, expected_cpu_model,
      expected_total_memory_kib, expected_cpu_threads_count,
      expected_cpu_max_clock_speed_khz, expected_has_battery,
      expected_milestone_version, expected_build_number, expected_patch_number);

  base::RunLoop run_loop;
  system_data_provider_->GetSystemInfo(
      base::BindLambdaForTesting([&](mojom::SystemInfoPtr ptr) {
        ASSERT_TRUE(ptr);
        EXPECT_EQ(expected_board_name, ptr->board_name);
        EXPECT_EQ(expected_marketing_name, ptr->marketing_name);
        EXPECT_EQ(expected_cpu_model, ptr->cpu_model_name);
        EXPECT_EQ(expected_total_memory_kib, ptr->total_memory_kib);
        EXPECT_EQ(expected_cpu_threads_count, ptr->cpu_threads_count);
        EXPECT_EQ(expected_cpu_max_clock_speed_khz,
                  ptr->cpu_max_clock_speed_khz);
        EXPECT_EQ(expected_milestone_version,
                  ptr->version_info->milestone_version);
        EXPECT_EQ(expected_has_battery, ptr->device_capabilities->has_battery);

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SystemDataProviderTest, NoBattery) {
  const std::string expected_board_name = "board_name";
  const std::string expected_marketing_name = "marketing_name";
  const std::string expected_cpu_model = "cpu_model";
  const uint32_t expected_total_memory_kib = 1234;
  const uint16_t expected_cpu_threads_count = 5678;
  const uint32_t expected_cpu_max_clock_speed_khz = 91011;
  const bool expected_has_battery = false;
  const std::string expected_milestone_version = "M99";
  const std::string expected_build_number = "1234";
  const std::string expected_patch_number = "5.6";

  SetCrosHealthdSystemInfoResponse(
      expected_board_name, expected_marketing_name, expected_cpu_model,
      expected_total_memory_kib, expected_cpu_threads_count,
      expected_cpu_max_clock_speed_khz, expected_has_battery,
      expected_milestone_version, expected_build_number, expected_patch_number);

  base::RunLoop run_loop;
  system_data_provider_->GetSystemInfo(
      base::BindLambdaForTesting([&](mojom::SystemInfoPtr ptr) {
        ASSERT_TRUE(ptr);
        EXPECT_EQ(expected_board_name, ptr->board_name);
        EXPECT_EQ(expected_marketing_name, ptr->marketing_name);
        EXPECT_EQ(expected_cpu_model, ptr->cpu_model_name);
        EXPECT_EQ(expected_total_memory_kib, ptr->total_memory_kib);
        EXPECT_EQ(expected_cpu_threads_count, ptr->cpu_threads_count);
        EXPECT_EQ(expected_cpu_max_clock_speed_khz,
                  ptr->cpu_max_clock_speed_khz);
        EXPECT_EQ(expected_milestone_version,
                  ptr->version_info->milestone_version);
        EXPECT_EQ(expected_has_battery, ptr->device_capabilities->has_battery);

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SystemDataProviderTest, BatteryInfo) {
  const std::string expected_manufacturer = "manufacturer";
  const double charge_full_amp_hours = 25;

  SetCrosHealthdBatteryInfoResponse(expected_manufacturer,
                                    charge_full_amp_hours);

  const uint32_t expected_charge_full_design_milliamp_hours =
      charge_full_amp_hours * 1000;

  base::RunLoop run_loop;
  system_data_provider_->GetBatteryInfo(
      base::BindLambdaForTesting([&](mojom::BatteryInfoPtr ptr) {
        ASSERT_TRUE(ptr);
        EXPECT_EQ(expected_manufacturer, ptr->manufacturer);
        EXPECT_EQ(expected_charge_full_design_milliamp_hours,
                  ptr->charge_full_design_milliamp_hours);

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SystemDataProviderTest, BatteryChargeStatusObserver) {
  // Setup Timer
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  system_data_provider_->SetBatteryChargeStatusTimerForTesting(
      std::move(timer));

  // Setup initial data
  const double charge_now_amp_hours = 20;
  const double current_now_amps = 2;
  const auto power_source =
      power_manager::PowerSupplyProperties_ExternalPower_AC;
  const auto battery_state =
      power_manager::PowerSupplyProperties_BatteryState_CHARGING;
  const bool is_calculating_battery_time = false;
  const int64_t time_to_full_secs = 1000;
  const int64_t time_to_empty_secs = 0;

  SetCrosHealthdBatteryChargeStatusResponse(charge_now_amp_hours,
                                            current_now_amps);
  SetPowerManagerProperties(power_source, battery_state,
                            is_calculating_battery_time, time_to_full_secs,
                            time_to_empty_secs);

  // Registering as an observer should trigger one update.
  FakeBatteryChargeStatusObserver charge_status_observer;
  system_data_provider_->ObserveBatteryChargeStatus(
      charge_status_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, charge_status_observer.updates.size());
  VerifyChargeStatusResult(charge_status_observer.updates[0],
                           charge_now_amp_hours, current_now_amps, power_source,
                           battery_state, is_calculating_battery_time,
                           time_to_full_secs, time_to_empty_secs);

  // Firing the timer should trigger another.
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, charge_status_observer.updates.size());
  VerifyChargeStatusResult(charge_status_observer.updates[0],
                           charge_now_amp_hours, current_now_amps, power_source,
                           battery_state, is_calculating_battery_time,
                           time_to_full_secs, time_to_empty_secs);

  // Updating the PowerManagerClient Properties should trigger yet another.
  const int64_t new_time_to_full_secs = time_to_full_secs - 10;
  SetPowerManagerProperties(
      power_manager::PowerSupplyProperties_ExternalPower_AC,
      power_manager::PowerSupplyProperties_BatteryState_CHARGING,
      is_calculating_battery_time, new_time_to_full_secs, time_to_empty_secs);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3u, charge_status_observer.updates.size());
  VerifyChargeStatusResult(charge_status_observer.updates[0],
                           charge_now_amp_hours, current_now_amps, power_source,
                           battery_state, is_calculating_battery_time,
                           new_time_to_full_secs, time_to_empty_secs);
}

TEST_F(SystemDataProviderTest, BatteryHealthObserver) {
  // Setup Timer
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  system_data_provider_->SetBatteryHealthTimerForTesting(std::move(timer));

  // Setup initial data
  const double charge_full_now = 20;
  const double charge_full_design = 26;
  const int32_t cycle_count = 500;

  SetCrosHealthdBatteryHealthResponse(charge_full_now, charge_full_design,
                                      cycle_count);

  // Registering as an observer should trigger one update.
  FakeBatteryHealthObserver health_observer;
  system_data_provider_->ObserveBatteryHealth(
      health_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, health_observer.updates.size());
  VerifyHealthResult(health_observer.updates[0], charge_full_now,
                     charge_full_design, cycle_count);

  // Firing the timer should trigger another.
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, health_observer.updates.size());
  VerifyHealthResult(health_observer.updates[1], charge_full_now,
                     charge_full_design, cycle_count);

  // Updating the information in Croshealthd does not trigger an update until
  // the timer fires
  const int32_t new_cycle_count = cycle_count + 1;
  SetCrosHealthdBatteryHealthResponse(charge_full_now, charge_full_design,
                                      new_cycle_count);

  EXPECT_EQ(2u, health_observer.updates.size());

  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3u, health_observer.updates.size());
  VerifyHealthResult(health_observer.updates[2], charge_full_now,
                     charge_full_design, new_cycle_count);
}

TEST_F(SystemDataProviderTest, MemoryUsageObserver) {
  // Setup Timer
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  system_data_provider_->SetMemoryUsageTimerForTesting(std::move(timer));

  // Setup initial data
  const uint32_t total_memory_kib = 10000;
  const uint32_t free_memory_kib = 2000;
  const uint32_t available_memory_kib = 4000;

  SetCrosHealthdMemoryUsageResponse(total_memory_kib, free_memory_kib,
                                    available_memory_kib);

  // Registering as an observer should trigger one update.
  FakeMemoryUsageObserver memory_usage_observer;
  system_data_provider_->ObserveMemoryUsage(
      memory_usage_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, memory_usage_observer.updates.size());
  VerifyMemoryUsageResult(memory_usage_observer.updates[0], total_memory_kib,
                          free_memory_kib, available_memory_kib);

  // Firing the timer should trigger another.
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, memory_usage_observer.updates.size());
  VerifyMemoryUsageResult(memory_usage_observer.updates[1], total_memory_kib,
                          free_memory_kib, available_memory_kib);

  // Updating the information in Croshealthd does not trigger an update until
  // the timer fires
  const uint32_t new_available_memory_kib = available_memory_kib + 1000;
  SetCrosHealthdMemoryUsageResponse(total_memory_kib, free_memory_kib,
                                    new_available_memory_kib);

  EXPECT_EQ(2u, memory_usage_observer.updates.size());

  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3u, memory_usage_observer.updates.size());
  VerifyMemoryUsageResult(memory_usage_observer.updates[2], total_memory_kib,
                          free_memory_kib, new_available_memory_kib);
}

TEST_F(SystemDataProviderTest, CpuUsageObserverOneProcessor) {
  // Setup Timer
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  system_data_provider_->SetCpuUsageTimerForTesting(std::move(timer));

  // Setup initial data
  CpuUsageData core_1(1000, 1000, 1000);

  SetCrosHealthdCpuUsageResponse({core_1});

  // Registering as an observer should trigger one update.
  FakeCpuUsageObserver cpu_usage_observer;
  system_data_provider_->ObserveCpuUsage(
      cpu_usage_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // There should be one update with no percentages since we could not calculate
  // a delta yet.
  EXPECT_EQ(1u, cpu_usage_observer.updates.size());
  VerifyCpuUsageResult(cpu_usage_observer.updates[0],
                       /*expected_percent_user=*/0,
                       /*expected_percent_system=*/0,
                       /*expected_percent_free=*/0);

  CpuUsageData delta(3000, 2500, 4500);

  SetCrosHealthdCpuUsageResponse({core_1 + delta});

  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, cpu_usage_observer.updates.size());
  VerifyCpuUsageResult(cpu_usage_observer.updates[1],
                       delta.GetUserTime() * 100 / delta.GetTotalTime(),
                       delta.GetSystemTime() * 100 / delta.GetTotalTime(),
                       delta.GetIdleTime() * 100 / delta.GetTotalTime());
}

TEST_F(SystemDataProviderTest, CpuUsageObserverTwoProcessor) {
  // Setup Timer
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  system_data_provider_->SetCpuUsageTimerForTesting(std::move(timer));

  // Setup initial data
  CpuUsageData core_1(1000, 1000, 1000);

  CpuUsageData core_2(2000, 2000, 2000);

  SetCrosHealthdCpuUsageResponse({core_1, core_2});

  // Registering as an observer should trigger one update.
  FakeCpuUsageObserver cpu_usage_observer;
  system_data_provider_->ObserveCpuUsage(
      cpu_usage_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // There should be one update with no percentages since we could not calculate
  // a delta yet.
  EXPECT_EQ(1u, cpu_usage_observer.updates.size());
  VerifyCpuUsageResult(cpu_usage_observer.updates[0],
                       /*expected_percent_user=*/0,
                       /*expected_percent_system=*/0,
                       /*expected_percent_free=*/0);

  CpuUsageData core_1_delta(3000, 2500, 4500);

  CpuUsageData core_2_delta(1000, 5500, 3500);

  SetCrosHealthdCpuUsageResponse(
      {core_1 + core_1_delta, core_2 + core_2_delta});

  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  // The result should be the averages of the times from the two cores.
  const int64_t expected_percent_user = 20;
  const int64_t expected_percent_system = 40;
  const int64_t expected_percent_free = 40;

  EXPECT_EQ(2u, cpu_usage_observer.updates.size());
  VerifyCpuUsageResult(cpu_usage_observer.updates[1], expected_percent_user,
                       expected_percent_system, expected_percent_free);
}

TEST_F(SystemDataProviderTest, CpuUsageObserverTemp) {
  // Setup Timer
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  system_data_provider_->SetCpuUsageTimerForTesting(std::move(timer));

  // Setup initial data
  int temp_1 = 40;
  int temp_2 = 50;
  int temp_3 = 15;

  SetCrosHealthdCpuTemperatureResponse({temp_1, temp_2, temp_3});

  // Registering as an observer should trigger one update.
  FakeCpuUsageObserver cpu_usage_observer;
  system_data_provider_->ObserveCpuUsage(
      cpu_usage_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, cpu_usage_observer.updates.size());
  VerifyCpuTempResult(cpu_usage_observer.updates[0],
                      /*expected_average_temp=*/35);

  temp_1 = 20;
  temp_2 = 25;
  temp_3 = 45;

  SetCrosHealthdCpuTemperatureResponse({temp_1, temp_2, temp_3});

  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, cpu_usage_observer.updates.size());
  VerifyCpuTempResult(cpu_usage_observer.updates[1],
                      /*expected_average_temp=*/30);

  temp_1 = 20;
  temp_2 = 26;
  temp_3 = 46;

  SetCrosHealthdCpuTemperatureResponse({temp_1, temp_2, temp_3});

  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3u, cpu_usage_observer.updates.size());
  // Integer division so `expected_average_temp` should still be 30.
  VerifyCpuTempResult(cpu_usage_observer.updates[2],
                      /*expected_average_temp=*/30);
}

TEST_F(SystemDataProviderTest, CpuUsageScaledClock) {
  // Setup Timer
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  system_data_provider_->SetCpuUsageTimerForTesting(std::move(timer));

  // Setup initial data
  uint32_t core_1_speed = 4000;
  uint32_t core_2_speed = 5000;

  SetCrosHealthdCpuScalingResponse({core_1_speed, core_2_speed});

  // Registering as an observer should trigger one update.
  FakeCpuUsageObserver cpu_usage_observer;
  system_data_provider_->ObserveCpuUsage(
      cpu_usage_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, cpu_usage_observer.updates.size());
  VerifyCpuScalingResult(cpu_usage_observer.updates[0],
                         /*expected_scaled_speed=*/4500);

  core_1_speed = 2000;
  core_2_speed = 2000;

  SetCrosHealthdCpuScalingResponse({core_1_speed, core_2_speed});

  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, cpu_usage_observer.updates.size());
  VerifyCpuScalingResult(cpu_usage_observer.updates[1],
                         /*expected_scaled_speed=*/2000);

  core_1_speed = 2000;
  core_2_speed = 2001;

  SetCrosHealthdCpuScalingResponse({core_1_speed, core_2_speed});

  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3u, cpu_usage_observer.updates.size());
  // Integer division so `expected_scaled_speed` should still be 2000.
  VerifyCpuScalingResult(cpu_usage_observer.updates[2],
                         /*expected_scaled_speed=*/2000);
}

TEST_F(SystemDataProviderTest, GetSystemInfoLogs) {
  DiagnosticsLogController::Get()->SetTelemetryLogForTesting(
      std::make_unique<TelemetryLog>());

  const std::string expected_board_name = "board_name";
  const std::string expected_marketing_name = "marketing_name";
  const std::string expected_cpu_model = "cpu_model";
  const uint32_t expected_total_memory_kib = 1234;
  const uint16_t expected_cpu_threads_count = 5678;
  const uint32_t expected_cpu_max_clock_speed_khz = 91011;
  const bool expected_has_battery = true;
  const std::string expected_milestone_version = "M99";
  const std::string expected_build_number = "1234";
  const std::string expected_patch_number = "5.6";

  const std::string expected_full_version = expected_milestone_version + '.' +
                                            expected_build_number + '.' +
                                            expected_patch_number;

  SetCrosHealthdSystemInfoResponse(
      expected_board_name, expected_marketing_name, expected_cpu_model,
      expected_total_memory_kib, expected_cpu_threads_count,
      expected_cpu_max_clock_speed_khz, expected_has_battery,
      expected_milestone_version, expected_build_number, expected_patch_number);

  base::RunLoop run_loop;
  system_data_provider_->GetSystemInfo(
      base::BindLambdaForTesting([&](mojom::SystemInfoPtr ptr) {
        ASSERT_TRUE(ptr);
        EXPECT_EQ(expected_board_name, ptr->board_name);
        EXPECT_EQ(expected_marketing_name, ptr->marketing_name);
        EXPECT_EQ(expected_cpu_model, ptr->cpu_model_name);
        EXPECT_EQ(expected_total_memory_kib, ptr->total_memory_kib);
        EXPECT_EQ(expected_cpu_threads_count, ptr->cpu_threads_count);
        EXPECT_EQ(expected_cpu_max_clock_speed_khz,
                  ptr->cpu_max_clock_speed_khz);
        EXPECT_EQ(expected_milestone_version,
                  ptr->version_info->milestone_version);
        EXPECT_EQ(expected_has_battery, ptr->device_capabilities->has_battery);

        run_loop.Quit();
      }));
  run_loop.Run();

  // Check the contents of the telemetry log
  const std::vector<std::string> log_contents = base::SplitString(
      DiagnosticsLogController::Get()->GetTelemetryLog().GetContents(), "\n",
      base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  // Expect one title line and 9 content lines.
  EXPECT_EQ(10u, log_contents.size());
  const std::string expected_snapshot_time_prefix = "Snapshot Time: ";
  EXPECT_GT(log_contents[1].size(), expected_snapshot_time_prefix.size());
  EXPECT_TRUE(base::StartsWith(log_contents[1], expected_snapshot_time_prefix));
  EXPECT_EQ("Board Name: " + expected_board_name, log_contents[2]);
  EXPECT_EQ("Marketing Name: " + expected_marketing_name, log_contents[3]);
  EXPECT_EQ("CpuModel Name: " + expected_cpu_model, log_contents[4]);
  EXPECT_EQ(
      "Total Memory (kib): " + base::NumberToString(expected_total_memory_kib),
      log_contents[5]);
  EXPECT_EQ(
      "Thread Count:  " + base::NumberToString(expected_cpu_threads_count),
      log_contents[6]);
  EXPECT_EQ("Cpu Max Clock Speed (kHz):  " +
                base::NumberToString(expected_cpu_max_clock_speed_khz),
            log_contents[7]);
  EXPECT_EQ("Version: " + expected_full_version, log_contents[8]);
  EXPECT_EQ("Has Battery: true", log_contents[9]);
}

TEST_F(SystemDataProviderTest, ResetReceiverOnBindInterface) {
  // This test simulates a user refreshing the WebUI page. The receiver should
  // be reset before binding the new receiver. Otherwise we would get a DCHECK
  // error from mojo::Receiver
  mojo::Remote<mojom::SystemDataProvider> remote;
  system_data_provider_->BindInterface(remote.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();

  remote.reset();

  system_data_provider_->BindInterface(remote.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();
}

TEST_F(SystemDataProviderTest, BatteryInfoPtrDataValidation) {
  // Setup Timer
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  system_data_provider_->SetBatteryHealthTimerForTesting(std::move(timer));

  const std::string vendor = "fake_vendor";
  healthd_mojom::BatteryInfoPtr battery_info_all_zero =
      CreateCrosHealthdBatteryInfoResponse(vendor, /*charge_full_design*/ 0);
  SetProbeTelemetryInfoResponse(std::move(battery_info_all_zero),
                                /*cpu_info=*/nullptr,
                                /*memory_info=*/nullptr,
                                /*system_info=*/nullptr);
  // Registering as an observer should trigger one update.
  FakeBatteryHealthObserver health_observer;
  system_data_provider_->ObserveBatteryHealth(
      health_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1ul, health_observer.updates.size());
  auto* battery_health_one = health_observer.updates[0].get();
  EXPECT_EQ(0, battery_health_one->battery_wear_percentage);
  EXPECT_FALSE(isnan(battery_health_one->battery_wear_percentage));
  EXPECT_FALSE(isnan(battery_health_one->charge_full_now_milliamp_hours));
  EXPECT_FALSE(isnan(battery_health_one->charge_full_design_milliamp_hours));

  healthd_mojom::BatteryInfoPtr battery_info_not_a_number =
      CreateCrosHealthdBatteryInfoResponse(vendor, /*charge_full_design*/ NAN);
  SetProbeTelemetryInfoResponse(std::move(battery_info_not_a_number),
                                /*cpu_info=*/nullptr,
                                /*memory_info=*/nullptr,
                                /*system_info=*/nullptr);
  // Trigger timer to update data.
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2ul, health_observer.updates.size());
  auto* battery_health_two = health_observer.updates[0].get();
  EXPECT_EQ(0, battery_health_two->battery_wear_percentage);
  EXPECT_FALSE(isnan(battery_health_two->battery_wear_percentage));
  EXPECT_FALSE(isnan(battery_health_two->charge_full_now_milliamp_hours));
  EXPECT_FALSE(isnan(battery_health_two->charge_full_design_milliamp_hours));

  healthd_mojom::BatteryInfoPtr battery_info_charge_full_nan =
      CreateCrosHealthdBatteryInfoResponse(vendor, /*charge_full_design*/ 1);
  battery_info_charge_full_nan->charge_full = NAN;
  SetProbeTelemetryInfoResponse(std::move(battery_info_charge_full_nan),
                                /*cpu_info=*/nullptr,
                                /*memory_info=*/nullptr,
                                /*system_info=*/nullptr);

  // Trigger timer to update data.
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3ul, health_observer.updates.size());
  auto* battery_health_three = health_observer.updates[2].get();
  EXPECT_EQ(0, battery_health_three->battery_wear_percentage);
  EXPECT_FALSE(isnan(battery_health_three->battery_wear_percentage));
  EXPECT_FALSE(isnan(battery_health_three->charge_full_now_milliamp_hours));
  EXPECT_FALSE(isnan(battery_health_three->charge_full_design_milliamp_hours));
}

TEST_F(SystemDataProviderTest, CpuUsagePtrDataValidation) {
  // Setup Timer
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  system_data_provider_->SetCpuUsageTimerForTesting(std::move(timer));

  FakeCpuUsageObserver cpu_usage_observer;
  system_data_provider_->ObserveCpuUsage(
      cpu_usage_observer.receiver.BindNewPipeAndPassRemote());

  // Simulate receiving a nullptr for CpuInfo.
  SetProbeTelemetryInfoResponse(/*battery_info=*/nullptr, nullptr,
                                /*memory_info=*/nullptr,
                                /*system_info=*/nullptr);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, cpu_usage_observer.updates.size());
  EXPECT_EQ(0u, cpu_usage_observer.updates[0]->average_cpu_temp_celsius);
  EXPECT_EQ(0u, cpu_usage_observer.updates[0]->scaling_current_frequency_khz);

  // Simulate receiving a CpuInfo with no data set.
  healthd_mojom::CpuInfoPtr cpu_info_no_data = healthd_mojom::CpuInfo::New();
  SetProbeTelemetryInfoResponse(/*battery_info=*/nullptr,
                                std::move(cpu_info_no_data),
                                /*memory_info=*/nullptr,
                                /*system_info=*/nullptr);

  // Trigger timer to update data.
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, cpu_usage_observer.updates.size());
  EXPECT_EQ(0u, cpu_usage_observer.updates[1]->average_cpu_temp_celsius);
  EXPECT_EQ(0u, cpu_usage_observer.updates[1]->scaling_current_frequency_khz);

  // Simulate receiving a CpuInfo with and empty temperature channel and empty
  // logical cpu.
  std::vector<healthd_mojom::PhysicalCpuInfoPtr> physical_cpus;
  physical_cpus.emplace_back(healthd_mojom::PhysicalCpuInfo::New());
  std::vector<healthd_mojom::CpuTemperatureChannelPtr> temperature_channels;
  healthd_mojom::CpuInfoPtr cpu_info_no_temperatures =
      healthd_mojom::CpuInfo::New(/*num_total_threads=*/0u,
                                  healthd_mojom::CpuArchitectureEnum::kUnknown,
                                  std::move(physical_cpus),
                                  std::move(temperature_channels), nullptr);
  SetProbeTelemetryInfoResponse(/*battery_info=*/nullptr,
                                std::move(cpu_info_no_temperatures),
                                /*memory_info=*/nullptr,
                                /*system_info=*/nullptr);

  // Trigger timer to update data.
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3u, cpu_usage_observer.updates.size());
  EXPECT_EQ(0u, cpu_usage_observer.updates[2]->average_cpu_temp_celsius);
  EXPECT_EQ(0u, cpu_usage_observer.updates[2]->scaling_current_frequency_khz);
}

// Validate expected metric NoData error triggered when request for SystemInfo
// returns no system info data.
TEST_F(SystemDataProviderTest, RecordSystemDataError_NoSystemInfo) {
  base::HistogramTester histogram_tester;
  VerifySystemDataErrorBucketCounts(histogram_tester,
                                    /*expected_no_data_error=*/0,
                                    /*expected_not_a_number_error=*/0,
                                    /*expected_expectation_not_met_error=*/0);

  SetProbeTelemetryInfoResponse(/*battery_info=*/nullptr,
                                /*cpu_info=*/nullptr,
                                /*memory_info=*/nullptr,
                                /*system_info=*/nullptr);

  base::RunLoop run_loop;
  system_data_provider_->GetSystemInfo(
      base::BindLambdaForTesting([&](mojom::SystemInfoPtr ptr) {
        EXPECT_TRUE(ptr);
        VerifySystemDataErrorBucketCounts(
            histogram_tester,
            /*expected_no_data_error=*/1,
            /*expected_not_a_number_error=*/0,
            /*expected_expectation_not_met_error=*/0);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Validate expected metric NoData error triggered when request for SystemInfo
// returns no cpu info data.
TEST_F(SystemDataProviderTest, RecordSystemDataError_NoCpuInfo) {
  base::HistogramTester histogram_tester;
  VerifySystemDataErrorBucketCounts(histogram_tester,
                                    /*expected_no_data_error=*/0,
                                    /*expected_not_a_number_error=*/0,
                                    /*expected_expectation_not_met_error=*/0);

  // System info.
  auto system_info = GetDefaultSystemInfoPtr();

  // Battery info.
  auto battery_info = healthd_mojom::BatteryInfo::New();

  // Memory info.
  auto memory_info = healthd_mojom::MemoryInfo::New();
  memory_info->total_memory_kib = 1234;

  SetProbeTelemetryInfoResponse(std::move(battery_info),
                                /*cpu_info=*/nullptr, std::move(memory_info),
                                std::move(system_info));

  base::RunLoop run_loop;
  system_data_provider_->GetSystemInfo(
      base::BindLambdaForTesting([&](mojom::SystemInfoPtr ptr) {
        EXPECT_TRUE(ptr);
        VerifySystemDataErrorBucketCounts(
            histogram_tester,
            /*expected_no_data_error=*/1,
            /*expected_not_a_number_error=*/0,
            /*expected_expectation_not_met_error=*/0);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SystemDataProviderTest, RecordSystemDataError_NoPhysicalCpuInfo) {
  base::HistogramTester histogram_tester;
  VerifySystemDataErrorBucketCounts(histogram_tester,
                                    /*expected_no_data_error=*/0,
                                    /*expected_not_a_number_error=*/0,
                                    /*expected_expectation_not_met_error=*/0);

  // System info.
  auto system_info = GetDefaultSystemInfoPtr();

  // Battery info.
  auto battery_info = healthd_mojom::BatteryInfo::New();

  // Memory info.
  auto memory_info = healthd_mojom::MemoryInfo::New();
  memory_info->total_memory_kib = 1234;

  // CPU info.
  auto cpu_info = healthd_mojom::CpuInfo::New();
  auto logical_cpu_info = healthd_mojom::LogicalCpuInfo::New();
  logical_cpu_info->max_clock_speed_khz = 91011;
  cpu_info->num_total_threads = 5678;

  SetProbeTelemetryInfoResponse(std::move(battery_info), std::move(cpu_info),
                                std::move(memory_info), std::move(system_info));

  base::RunLoop run_loop;
  system_data_provider_->GetSystemInfo(
      base::BindLambdaForTesting([&](mojom::SystemInfoPtr ptr) {
        EXPECT_TRUE(ptr);
        VerifySystemDataErrorBucketCounts(
            histogram_tester,
            /*expected_no_data_error=*/0,
            /*expected_not_a_number_error=*/0,
            /*expected_expectation_not_met_error=*/1);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Validate expected metric ExpectationNotMet error triggered when logical cpu
// info is empty.
TEST_F(SystemDataProviderTest, RecordSystemDataError_NoLogicalCpuInfo) {
  base::HistogramTester histogram_tester;
  VerifySystemDataErrorBucketCounts(histogram_tester,
                                    /*expected_no_data_error=*/0,
                                    /*expected_not_a_number_error=*/0,
                                    /*expected_expectation_not_met_error=*/0);

  // System info.
  auto system_info = GetDefaultSystemInfoPtr();

  // Battery info.
  auto battery_info = healthd_mojom::BatteryInfo::New();

  // Memory info.
  auto memory_info = healthd_mojom::MemoryInfo::New();
  memory_info->total_memory_kib = 1234;

  // CPU info.
  auto cpu_info = healthd_mojom::CpuInfo::New();
  auto physical_cpu_info = healthd_mojom::PhysicalCpuInfo::New();
  physical_cpu_info->model_name = "cpu_model";
  cpu_info->num_total_threads = 5678;
  cpu_info->physical_cpus.emplace_back(std::move(physical_cpu_info));

  SetProbeTelemetryInfoResponse(std::move(battery_info), std::move(cpu_info),
                                std::move(memory_info), std::move(system_info));

  base::RunLoop run_loop;
  system_data_provider_->GetSystemInfo(
      base::BindLambdaForTesting([&](mojom::SystemInfoPtr ptr) {
        EXPECT_TRUE(ptr);
        VerifySystemDataErrorBucketCounts(
            histogram_tester,
            /*expected_no_data_error=*/0,
            /*expected_not_a_number_error=*/0,
            /*expected_expectation_not_met_error=*/1);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Validate expected metric ExpectationNotMet error triggered when cpu usage
// delta is zero.
TEST_F(SystemDataProviderTest, RecordSystemDataError_DeltaZero) {
  base::HistogramTester histogram_tester;
  VerifySystemDataErrorBucketCounts(histogram_tester,
                                    /*expected_no_data_error=*/0,
                                    /*expected_not_a_number_error=*/0,
                                    /*expected_expectation_not_met_error=*/0);

  // Setup Timer
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  system_data_provider_->SetCpuUsageTimerForTesting(std::move(timer));

  // Setup initial data
  CpuUsageData core_1(100, 1000, 1000);

  SetCrosHealthdCpuUsageResponse({core_1});

  // Registering as an observer should trigger one update.
  FakeCpuUsageObserver cpu_usage_observer;
  system_data_provider_->ObserveCpuUsage(
      cpu_usage_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  VerifySystemDataErrorBucketCounts(histogram_tester,
                                    /*expected_no_data_error=*/0,
                                    /*expected_not_a_number_error=*/0,
                                    /*expected_expectation_not_met_error=*/1);
}

// Validate expected metric triggered when request for BatteryInfo returns a
// ProbeError.
TEST_F(SystemDataProviderTest, RecordProbeError_BatteryInfo) {
  base::HistogramTester histogram_tester;
  VerifyProbeErrorBucketCounts(histogram_tester, kProbeErrorBatteryInfo,
                               /*expected_unknown_error=*/0,
                               /*expected_parse_error=*/0,
                               /*expected_service_unavailable=*/0,
                               /*expected_system_utility_error=*/0,
                               /*expected_file_read_error=*/0);

  auto info = healthd_mojom::TelemetryInfo::New();
  auto battery_result = healthd_mojom::BatteryResult::NewError(
      CreateProbeError(healthd_mojom::ErrorType::kParseError));
  info->battery_result = std::move(battery_result);
  cros_healthd::FakeCrosHealthd::Get()->SetProbeTelemetryInfoResponseForTesting(
      info);
  base::RunLoop run_loop;

  system_data_provider_->GetBatteryInfo(
      base::BindLambdaForTesting([&](mojom::BatteryInfoPtr ptr) {
        EXPECT_TRUE(ptr);
        VerifyProbeErrorBucketCounts(histogram_tester, kProbeErrorBatteryInfo,
                                     /*expected_unknown_error=*/0,
                                     /*expected_parse_error=*/1,
                                     /*expected_service_unavailable=*/0,
                                     /*expected_system_utility_error=*/0,
                                     /*expected_file_read_error=*/0);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Validate expected metric NoData error triggered when request for SystemInfo
// returns no battery info data.
TEST_F(SystemDataProviderTest, RecordBatteryDataError_BatteryInfoNoDataError) {
  base::HistogramTester histogram_tester;
  VerifyBatteryDataErrorBucketCounts(histogram_tester,
                                     /*expected_no_data_error=*/0,
                                     /*expected_not_a_number_error=*/0,
                                     /*expected_expectation_not_met_error=*/0);

  // Intentionally do not set any battery info.

  base::RunLoop run_loop;
  system_data_provider_->GetBatteryInfo(
      base::BindLambdaForTesting([&](mojom::BatteryInfoPtr ptr) {
        ASSERT_TRUE(ptr);
        VerifyBatteryDataErrorBucketCounts(
            histogram_tester,
            /*expected_no_data_error=*/1,
            /*expected_not_a_number_error=*/0,
            /*expected_expectation_not_met_error=*/0);

        run_loop.Quit();
      }));
  run_loop.Run();
}

// Validate expected metric triggered when request for CpuInfo returns a
// ProbeError.
TEST_F(SystemDataProviderTest, RecordProbeError_CpuInfo) {
  base::HistogramTester histogram_tester;
  // Setup Timer
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  system_data_provider_->SetCpuUsageTimerForTesting(std::move(timer));

  FakeCpuUsageObserver cpu_usage_observer;
  system_data_provider_->ObserveCpuUsage(
      cpu_usage_observer.receiver.BindNewPipeAndPassRemote());

  VerifyProbeErrorBucketCounts(histogram_tester, kProbeErrorCpuInfo,
                               /*expected_unknown_error=*/0,
                               /*expected_parse_error=*/0,
                               /*expected_service_unavailable=*/0,
                               /*expected_system_utility_error=*/0,
                               /*expected_file_read_error=*/0);
  auto info = healthd_mojom::TelemetryInfo::New();
  cros_healthd::FakeCrosHealthd::Get()->SetProbeTelemetryInfoResponseForTesting(
      info);
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  VerifyProbeErrorBucketCounts(
      histogram_tester, kProbeErrorCpuInfo, /*expected_unknown_error=*/0,
      /*expected_parse_error=*/0, /*expected_service_unavailable=*/0,
      /*expected_system_utility_error=*/0, /*expected_file_read_error=*/0);

  auto cpu_result = healthd_mojom::CpuResult::NewError(
      CreateProbeError(healthd_mojom::ErrorType::kFileReadError));
  info->cpu_result = std::move(cpu_result);
  cros_healthd::FakeCrosHealthd::Get()->SetProbeTelemetryInfoResponseForTesting(
      info);
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  VerifyProbeErrorBucketCounts(histogram_tester, kProbeErrorCpuInfo,
                               /*expected_unknown_error=*/0,
                               /*expected_parse_error=*/0,
                               /*expected_service_unavailable=*/0,
                               /*expected_system_utility_error=*/0,
                               /*expected_file_read_error=*/1);
}

// Validate expected metric triggered when request for MemoryInfo returns a
// ProbeError.
TEST_F(SystemDataProviderTest, RecordProbeError_MemoryInfo) {
  base::HistogramTester histogram_tester;
  VerifyProbeErrorBucketCounts(histogram_tester, kProbeErrorMemoryInfo,
                               /*expected_unknown_error=*/0,
                               /*expected_parse_error=*/0,
                               /*expected_service_unavailable=*/0,
                               /*expected_system_utility_error=*/0,
                               /*expected_file_read_error=*/0);

  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  system_data_provider_->SetMemoryUsageTimerForTesting(std::move(timer));
  FakeMemoryUsageObserver observer;
  system_data_provider_->ObserveMemoryUsage(
      observer.receiver.BindNewPipeAndPassRemote());
  // Clear any pending observerations from initializing.
  base::RunLoop().RunUntilIdle();
  VerifyProbeErrorBucketCounts(histogram_tester, kProbeErrorMemoryInfo,
                               /*expected_unknown_error=*/0,
                               /*expected_parse_error=*/0,
                               /*expected_service_unavailable=*/0,
                               /*expected_system_utility_error=*/0,
                               /*expected_file_read_error=*/0);

  auto info = healthd_mojom::TelemetryInfo::New();
  auto memory_result = healthd_mojom::MemoryResult::NewError(
      CreateProbeError(healthd_mojom::ErrorType::kSystemUtilityError));
  info->memory_result = std::move(memory_result);
  cros_healthd::FakeCrosHealthd::Get()->SetProbeTelemetryInfoResponseForTesting(
      info);

  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  VerifyProbeErrorBucketCounts(histogram_tester, kProbeErrorMemoryInfo,
                               /*expected_unknown_error=*/0,
                               /*expected_parse_error=*/0,
                               /*expected_service_unavailable=*/0,
                               /*expected_system_utility_error=*/1,
                               /*expected_file_read_error=*/0);
}

// Validate expected metric triggered when request for SystemInfo returns a
// ProbeError.
TEST_F(SystemDataProviderTest, RecordProbeError_SystemInfo) {
  base::HistogramTester histogram_tester;
  VerifyProbeErrorBucketCounts(
      histogram_tester, kProbeErrorSystemInfo, /*expected_unknown_error=*/0,
      /*expected_parse_error=*/0, /*expected_service_unavailable=*/0,
      /*expected_system_utility_error=*/0, /*expected_file_read_error=*/0);

  auto system_result = healthd_mojom::SystemResult::NewError(
      CreateProbeError(healthd_mojom::ErrorType::kServiceUnavailable));
  auto info = healthd_mojom::TelemetryInfo::New();
  info->system_result = std::move(system_result);
  cros_healthd::FakeCrosHealthd::Get()->SetProbeTelemetryInfoResponseForTesting(
      info);

  base::RunLoop run_loop;
  system_data_provider_->GetSystemInfo(
      base::BindLambdaForTesting([&](mojom::SystemInfoPtr ptr) {
        ASSERT_TRUE(ptr);
        VerifyProbeErrorBucketCounts(histogram_tester, kProbeErrorSystemInfo,
                                     /*expected_unknown_error=*/0,
                                     /*expected_parse_error=*/0,
                                     /*expected_service_unavailable=*/1,
                                     /*expected_system_utility_error=*/0,
                                     /*expected_file_read_error=*/0);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Validate expected metric ExpectationNotMet error triggered when
// charge_full_design value from battery_info is zero.
TEST_F(SystemDataProviderTest, RecordBatteryDataError_ChargeFullDesignZero) {
  base::HistogramTester histogram_tester;
  VerifyBatteryDataErrorBucketCounts(histogram_tester,
                                     /*expected_no_data_error=*/0,
                                     /*expected_not_a_number_error=*/0,
                                     /*expected_expectation_not_met_error=*/0);

  const std::string vendor = "fake_vendor";
  healthd_mojom::BatteryInfoPtr battery_info_all_zero =
      CreateCrosHealthdBatteryInfoResponse(vendor, /*charge_full_design*/ 0);
  SetProbeTelemetryInfoResponse(std::move(battery_info_all_zero),
                                /*cpu_info=*/nullptr,
                                /*memory_info=*/nullptr,
                                /*system_info=*/nullptr);

  base::RunLoop run_loop;
  system_data_provider_->GetBatteryInfo(
      base::BindLambdaForTesting([&](mojom::BatteryInfoPtr ptr) {
        ASSERT_TRUE(ptr);
        VerifyBatteryDataErrorBucketCounts(
            histogram_tester,
            /*expected_no_data_error=*/0,
            /*expected_not_a_number_error=*/0,
            /*expected_expectation_not_met_error=*/1);

        run_loop.Quit();
      }));
  run_loop.Run();
}

// Validate expected metric ExpectationNotMet error triggered when
// charge_full value from battery_info is zero.
TEST_F(SystemDataProviderTest, RecordBatteryDataError_ChargeFullZero) {
  base::HistogramTester histogram_tester;
  VerifyBatteryDataErrorBucketCounts(histogram_tester,
                                     /*expected_no_data_error=*/0,
                                     /*expected_not_a_number_error=*/0,
                                     /*expected_expectation_not_met_error=*/0);

  const std::string vendor = "fake_vendor";
  healthd_mojom::BatteryInfoPtr battery_info_charge_full_zero =
      CreateCrosHealthdBatteryInfoResponse(vendor, /*charge_full_design*/ 1);
  SetProbeTelemetryInfoResponse(std::move(battery_info_charge_full_zero),
                                /*cpu_info=*/nullptr,
                                /*memory_info=*/nullptr,
                                /*system_info=*/nullptr);

  // Registering as an observer should trigger one update.
  FakeBatteryHealthObserver health_observer;
  system_data_provider_->ObserveBatteryHealth(
      health_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  VerifyBatteryDataErrorBucketCounts(histogram_tester,
                                     /*expected_no_data_error=*/0,
                                     /*expected_not_a_number_error=*/0,
                                     /*expected_expectation_not_met_error=*/1);
}

// Validate expected metric ExpectationNotMet error triggered when
// has battery info mismatch from two sources.
TEST_F(SystemDataProviderTest, RecordBatteryDataError_HasBatteryInfoMismatch) {
  base::HistogramTester histogram_tester;
  VerifyBatteryDataErrorBucketCounts(histogram_tester,
                                     /*expected_no_data_error=*/0,
                                     /*expected_not_a_number_error=*/0,
                                     /*expected_expectation_not_met_error=*/0);

  // Registering as an observer should trigger one update.
  FakeBatteryChargeStatusObserver charge_status_observer;
  system_data_provider_->ObserveBatteryChargeStatus(
      charge_status_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  VerifyBatteryDataErrorBucketCounts(histogram_tester,
                                     /*expected_no_data_error=*/0,
                                     /*expected_not_a_number_error=*/0,
                                     /*expected_expectation_not_met_error=*/1);
}

// Validate expected metric NoData error triggered when battery charge status
// returns null.
TEST_F(SystemDataProviderTest, RecordBatteryDataError_ChargeStatusNull) {
  base::HistogramTester histogram_tester;
  VerifyBatteryDataErrorBucketCounts(histogram_tester,
                                     /*expected_no_data_error=*/0,
                                     /*expected_not_a_number_error=*/0,
                                     /*expected_expectation_not_met_error=*/0);

  std::nullopt_t props = std::nullopt;
  chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(props);

  // Registering as an observer should trigger one update.
  FakeBatteryChargeStatusObserver charge_status_observer;
  system_data_provider_->ObserveBatteryChargeStatus(
      charge_status_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  VerifyBatteryDataErrorBucketCounts(histogram_tester,
                                     /*expected_no_data_error=*/1,
                                     /*expected_not_a_number_error=*/0,
                                     /*expected_expectation_not_met_error=*/0);
}

}  // namespace ash::diagnostics

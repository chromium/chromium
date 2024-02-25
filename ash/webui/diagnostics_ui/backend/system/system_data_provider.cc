// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/system/system_data_provider.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/system/diagnostics/diagnostics_log_controller.h"
#include "ash/system/diagnostics/telemetry_log.h"
#include "ash/webui/diagnostics_ui/backend/common/histogram_util.h"
#include "ash/webui/diagnostics_ui/backend/system/cros_healthd_helpers.h"
#include "ash/webui/diagnostics_ui/backend/system/power_manager_client_conversions.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace ash::diagnostics {

namespace {

namespace healthd = cros_healthd::mojom;
using PhysicalCpuInfos = std::vector<healthd::PhysicalCpuInfoPtr>;
using PowerSupplyProperties = power_manager::PowerSupplyProperties;
using ProbeCategories = healthd::ProbeCategoryEnum;

constexpr int kBatteryHealthRefreshIntervalInSeconds = 60;
constexpr int kChargeStatusRefreshIntervalInSeconds = 15;
constexpr int kCpuUsageRefreshIntervalInSeconds = 1;
constexpr int kMemoryUsageRefreshIntervalInSeconds = 10;
constexpr int kMilliampsInAnAmp = 1000;

void PopulateBoardName(const healthd::SystemInfo& system_info,
                       mojom::SystemInfo& out_system_info) {
  out_system_info.board_name = system_info.os_info->code_name;
}

void PopulateMarketingName(const healthd::SystemInfo& system_info,
                           mojom::SystemInfo& out_system_info) {
  const std::optional<std::string>& marketing_name =
      system_info.os_info->marketing_name;

  if (!marketing_name.has_value()) {
    DVLOG(1) << "No marketing name in SystemInfo response.";
    return;
  }

  out_system_info.marketing_name = marketing_name.value();
}

void PopulateCpuInfo(const healthd::CpuInfo& cpu_info,
                     mojom::SystemInfo& out_system_info) {
  const PhysicalCpuInfos& physical_cpus = cpu_info.physical_cpus;
  out_system_info.cpu_threads_count = cpu_info.num_total_threads;

  if (physical_cpus.empty()) {
    EmitSystemDataError(metrics::DataError::kExpectationNotMet);
    LOG(ERROR) << "No physical cpus in SystemInfo response.";
    return;
  }

  // If there is more than one physical cpu on the device, use the name of the
  // first CPU.
  out_system_info.cpu_model_name = physical_cpus[0]->model_name.value_or("");

  if (physical_cpus[0]->logical_cpus.empty()) {
    EmitSystemDataError(metrics::DataError::kExpectationNotMet);
    LOG(ERROR) << "Device reported having 0 logical CPUs.";
    return;
  }
  // Calculate `max_clock_speed_khz` as the average of all logical core clock
  // speeds until we decide the best way to consume the information in the UI.
  uint32_t total_max_ghz = 0;
  for (const auto& logical_cpu_ptr : physical_cpus[0]->logical_cpus) {
    total_max_ghz += logical_cpu_ptr->max_clock_speed_khz;
  }

  // Integer division.
  out_system_info.cpu_max_clock_speed_khz =
      total_max_ghz / physical_cpus[0]->logical_cpus.size();
}

void PopulateVersionInfo(const healthd::SystemInfo& system_info,
                         mojom::SystemInfo& out_system_info) {
  const std::string full_version =
      system_info.os_info->os_version->release_milestone + '.' +
      system_info.os_info->os_version->build_number + '.' +
      system_info.os_info->os_version->patch_number;
  out_system_info.version_info = mojom::VersionInfo::New(
      system_info.os_info->os_version->release_milestone, full_version);
}

void PopulateMemorySize(const healthd::MemoryInfo& memory_info,
                        mojom::SystemInfo& out_system_info) {
  out_system_info.total_memory_kib = memory_info.total_memory_kib;
}

bool DoesDeviceHaveBattery(
    const PowerSupplyProperties& power_supply_properties) {
  return power_supply_properties.battery_state() !=
         power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT;
}

bool DoesDeviceHaveBattery(const healthd::TelemetryInfo& telemetry_info) {
  return GetBatteryInfo(telemetry_info) != nullptr;
}

void PopulateDeviceCapabilities(const healthd::TelemetryInfo& telemetry_info,
                                mojom::SystemInfo& out_system_info) {
  mojom::DeviceCapabilitiesPtr capabilities = mojom::DeviceCapabilities::New();
  capabilities->has_battery = DoesDeviceHaveBattery(telemetry_info);
  out_system_info.device_capabilities = std::move(capabilities);
}

void PopulateBatteryInfo(const healthd::BatteryInfo& battery_info,
                         mojom::BatteryInfo& out_battery_info) {
  if (battery_info.charge_full_design == 0) {
    LOG(ERROR) << "charge_full_design from battery_info should not be zero.";
    EmitBatteryDataError(metrics::DataError::kExpectationNotMet);
  }
  out_battery_info.manufacturer = battery_info.vendor;
  out_battery_info.charge_full_design_milliamp_hours =
      battery_info.charge_full_design * kMilliampsInAnAmp;
}

void PopulatePowerInfo(const PowerSupplyProperties& power_supply_properties,
                       mojom::BatteryChargeStatus& out_charge_status) {
  const mojom::BatteryState battery_state =
      ConvertBatteryStateFromProto(power_supply_properties.battery_state());

  out_charge_status.battery_state = battery_state;
  out_charge_status.power_time =
      ConstructPowerTime(battery_state, power_supply_properties);
  out_charge_status.power_adapter_status =
      ConvertPowerSourceFromProto(power_supply_properties.external_power());
}

void PopulateBatteryChargeStatus(
    const healthd::BatteryInfo& battery_info,
    const PowerSupplyProperties& power_supply_properties,
    mojom::BatteryChargeStatus& out_charge_status) {
  PopulatePowerInfo(power_supply_properties, out_charge_status);

  out_charge_status.current_now_milliamps =
      battery_info.current_now * kMilliampsInAnAmp;
  out_charge_status.charge_now_milliamp_hours =
      battery_info.charge_now * kMilliampsInAnAmp;
}

void PopulateBatteryHealth(const healthd::BatteryInfo& battery_info,
                           mojom::BatteryHealth& out_battery_health) {
  out_battery_health.cycle_count = battery_info.cycle_count;

  if (battery_info.charge_full == 0) {
    LOG(ERROR) << "charge_full from battery_info should not be zero.";
    EmitBatteryDataError(metrics::DataError::kExpectationNotMet);
  }

  // Handle values in battery_info which could cause a SIGFPE. See b/227485637.
  if (isnan(battery_info.charge_full) ||
      isnan(battery_info.charge_full_design) ||
      battery_info.charge_full_design == 0) {
    LOG(ERROR) << "battery_info values could cause SIGFPE crash: { "
               << "charge_full_design: " << battery_info.charge_full_design
               << ", charge_full: " << battery_info.charge_full << " }";
    out_battery_health.charge_full_now_milliamp_hours = 0;
    out_battery_health.charge_full_design_milliamp_hours = 0;
    out_battery_health.battery_wear_percentage = 0;
    return;
  }

  out_battery_health.charge_full_now_milliamp_hours =
      battery_info.charge_full * kMilliampsInAnAmp;
  out_battery_health.charge_full_design_milliamp_hours =
      battery_info.charge_full_design * kMilliampsInAnAmp;
  out_battery_health.battery_wear_percentage =
      100 * out_battery_health.charge_full_now_milliamp_hours /
      out_battery_health.charge_full_design_milliamp_hours;
}

void PopulateMemoryUsage(const healthd::MemoryInfo& memory_info,
                         mojom::MemoryUsage& out_memory_usage) {
  out_memory_usage.total_memory_kib = memory_info.total_memory_kib;
  out_memory_usage.free_memory_kib = memory_info.free_memory_kib;
  out_memory_usage.available_memory_kib = memory_info.available_memory_kib;
}

CpuUsageData CalculateCpuUsage(
    const std::vector<healthd::LogicalCpuInfoPtr>& logical_cpu_infos) {
  CpuUsageData new_usage_data;

  DCHECK_GE(logical_cpu_infos.size(), 1u);
  for (const auto& logical_cpu_ptr : logical_cpu_infos) {
    new_usage_data += CpuUsageData(logical_cpu_ptr->user_time_user_hz,
                                   logical_cpu_ptr->system_time_user_hz,
                                   logical_cpu_ptr->idle_time_user_hz);
  }

  return new_usage_data;
}

void PopulateCpuUsagePercentages(const CpuUsageData& new_usage,
                                 const CpuUsageData& old_usage,
                                 mojom::CpuUsage& out_cpu_usage) {
  CpuUsageData delta = new_usage - old_usage;

  const uint64_t total_delta = delta.GetTotalTime();
  if (total_delta == 0) {
    EmitSystemDataError(metrics::DataError::kExpectationNotMet);
    return;
  }

  // Mulitply by 100 to convert to percentages.
  out_cpu_usage.percent_usage_user = 100 * delta.GetUserTime() / total_delta;
  out_cpu_usage.percent_usage_system =
      100 * delta.GetSystemTime() / total_delta;
  out_cpu_usage.percent_usage_free = 100 * delta.GetIdleTime() / total_delta;
}

void PopulateAverageCpuTemperature(const healthd::CpuInfo& cpu_info,
                                   mojom::CpuUsage& out_cpu_usage) {
  if (cpu_info.temperature_channels.empty()) {
    LOG(ERROR) << "Device reported having 0 temperature channels.";
    return;
  }

  uint32_t cumulative_total = 0;
  for (const auto& temp_channel_ptr : cpu_info.temperature_channels) {
    cumulative_total += temp_channel_ptr->temperature_celsius;
  }

  // Integer divison.
  out_cpu_usage.average_cpu_temp_celsius =
      cumulative_total / cpu_info.temperature_channels.size();
}

void PopulateAverageScaledClockSpeed(const healthd::CpuInfo& cpu_info,
                                     mojom::CpuUsage& out_cpu_usage) {
  if (cpu_info.physical_cpus.empty() ||
      cpu_info.physical_cpus[0]->logical_cpus.empty()) {
    LOG(ERROR) << "Device reported having 0 logical CPUs.";
    return;
  }

  uint32_t total_scaled_ghz = 0;
  for (const auto& logical_cpu_ptr : cpu_info.physical_cpus[0]->logical_cpus) {
    total_scaled_ghz += logical_cpu_ptr->scaling_current_frequency_khz;
  }

  // Integer division.
  out_cpu_usage.scaling_current_frequency_khz =
      total_scaled_ghz / cpu_info.physical_cpus[0]->logical_cpus.size();
}

bool IsLoggingEnabled() {
  return diagnostics::DiagnosticsLogController::IsInitialized();
}

}  // namespace

SystemDataProvider::SystemDataProvider() {
  battery_charge_status_timer_ = std::make_unique<base::RepeatingTimer>();
  battery_health_timer_ = std::make_unique<base::RepeatingTimer>();
  cpu_usage_timer_ = std::make_unique<base::RepeatingTimer>();
  memory_usage_timer_ = std::make_unique<base::RepeatingTimer>();
  chromeos::PowerManagerClient::Get()->AddObserver(this);
}

SystemDataProvider::~SystemDataProvider() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void SystemDataProvider::GetSystemInfo(GetSystemInfoCallback callback) {
  BindCrosHealthdProbeServiceIfNeccessary();

  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kBattery, ProbeCategories::kCpu,
       ProbeCategories::kMemory, ProbeCategories::kSystem},
      base::BindOnce(&SystemDataProvider::OnSystemInfoProbeResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SystemDataProvider::GetBatteryInfo(GetBatteryInfoCallback callback) {
  BindCrosHealthdProbeServiceIfNeccessary();

  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kBattery},
      base::BindOnce(&SystemDataProvider::OnBatteryInfoProbeResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SystemDataProvider::ObserveBatteryChargeStatus(
    mojo::PendingRemote<mojom::BatteryChargeStatusObserver> observer) {
  battery_charge_status_observers_.Add(std::move(observer));

  if (!battery_charge_status_timer_->IsRunning()) {
    battery_charge_status_timer_->Start(
        FROM_HERE, base::Seconds(kChargeStatusRefreshIntervalInSeconds),
        base::BindRepeating(&SystemDataProvider::UpdateBatteryChargeStatus,
                            base::Unretained(this)));
  }
  UpdateBatteryChargeStatus();
}

void SystemDataProvider::ObserveBatteryHealth(
    mojo::PendingRemote<mojom::BatteryHealthObserver> observer) {
  battery_health_observers_.Add(std::move(observer));

  if (!battery_health_timer_->IsRunning()) {
    battery_health_timer_->Start(
        FROM_HERE, base::Seconds(kBatteryHealthRefreshIntervalInSeconds),
        base::BindRepeating(&SystemDataProvider::UpdateBatteryHealth,
                            base::Unretained(this)));
  }
  UpdateBatteryHealth();
}

void SystemDataProvider::ObserveMemoryUsage(
    mojo::PendingRemote<mojom::MemoryUsageObserver> observer) {
  memory_usage_observers_.Add(std::move(observer));

  if (!memory_usage_timer_->IsRunning()) {
    memory_usage_timer_->Start(
        FROM_HERE, base::Seconds(kMemoryUsageRefreshIntervalInSeconds),
        base::BindRepeating(&SystemDataProvider::UpdateMemoryUsage,
                            base::Unretained(this)));
  }
  UpdateMemoryUsage();
}

void SystemDataProvider::ObserveCpuUsage(
    mojo::PendingRemote<mojom::CpuUsageObserver> observer) {
  cpu_usage_observers_.Add(std::move(observer));
  if (!cpu_usage_timer_->IsRunning()) {
    previous_cpu_usage_data_ = CpuUsageData();
    cpu_usage_timer_->Start(
        FROM_HERE, base::Seconds(kCpuUsageRefreshIntervalInSeconds),
        base::BindRepeating(&SystemDataProvider::UpdateCpuUsage,
                            base::Unretained(this)));
  }

  UpdateCpuUsage();
}

void SystemDataProvider::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  if (battery_charge_status_observers_.empty()) {
    return;
  }

  // Fetch updated data from CrosHealthd
  BindCrosHealthdProbeServiceIfNeccessary();
  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kBattery},
      base::BindOnce(&SystemDataProvider::OnBatteryChargeStatusUpdated,
                     weak_factory_.GetWeakPtr(), proto));
}

void SystemDataProvider::BindInterface(
    mojo::PendingReceiver<mojom::SystemDataProvider> pending_receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
}

void SystemDataProvider::SetBatteryChargeStatusTimerForTesting(
    std::unique_ptr<base::RepeatingTimer> timer) {
  battery_charge_status_timer_ = std::move(timer);
}

void SystemDataProvider::SetBatteryHealthTimerForTesting(
    std::unique_ptr<base::RepeatingTimer> timer) {
  battery_health_timer_ = std::move(timer);
}

void SystemDataProvider::SetMemoryUsageTimerForTesting(
    std::unique_ptr<base::RepeatingTimer> timer) {
  memory_usage_timer_ = std::move(timer);
}

void SystemDataProvider::SetCpuUsageTimerForTesting(
    std::unique_ptr<base::RepeatingTimer> timer) {
  cpu_usage_timer_ = std::move(timer);
}

void SystemDataProvider::OnSystemInfoProbeResponse(
    GetSystemInfoCallback callback,
    healthd::TelemetryInfoPtr info_ptr) {
  mojom::SystemInfoPtr system_info = mojom::SystemInfo::New();
  system_info->version_info = mojom::VersionInfo::New();
  system_info->device_capabilities = mojom::DeviceCapabilities::New();

  if (info_ptr.is_null()) {
    LOG(ERROR) << "Null response from croshealthd::ProbeTelemetryInfo.";
    std::move(callback).Run(std::move(system_info));
    return;
  }

  const healthd::SystemInfo* system_info_ptr =
      diagnostics::GetSystemInfo(*info_ptr);
  if (system_info_ptr) {
    PopulateBoardName(*system_info_ptr, *system_info.get());
    PopulateMarketingName(*system_info_ptr, *system_info.get());
    PopulateVersionInfo(*system_info_ptr, *system_info.get());
  } else {
    LOG(ERROR)
        << "Expected SystemInfo in croshealthd::ProbeTelemetryInfo response";
    std::move(callback).Run(std::move(system_info));
    return;
  }

  const healthd::CpuInfo* cpu_info_ptr = GetCpuInfo(*info_ptr);
  if (cpu_info_ptr) {
    PopulateCpuInfo(*cpu_info_ptr, *system_info.get());
  } else {
    LOG(ERROR)
        << "Expected CpuInfo in croshealthd::ProbeTelemetryInfo response";
  }

  const healthd::MemoryInfo* memory_info_ptr = GetMemoryInfo(*info_ptr);
  if (memory_info_ptr) {
    PopulateMemorySize(*memory_info_ptr, *system_info.get());
  } else {
    LOG(ERROR)
        << "Expected MemoryInfo in croshealthd::ProbeTelemetryInfo response";
  }

  PopulateDeviceCapabilities(*info_ptr, *system_info.get());

  if (IsLoggingEnabled()) {
    DiagnosticsLogController::Get()->GetTelemetryLog().UpdateSystemInfo(
        system_info.Clone());
  }

  std::move(callback).Run(std::move(system_info));
}

void SystemDataProvider::OnBatteryInfoProbeResponse(
    GetBatteryInfoCallback callback,
    healthd::TelemetryInfoPtr info_ptr) {
  mojom::BatteryInfoPtr battery_info = mojom::BatteryInfo::New();

  if (info_ptr.is_null()) {
    LOG(ERROR) << "Null response from croshealthd::ProbeTelemetryInfo.";
    std::move(callback).Run(std::move(battery_info));
    return;
  }

  const healthd::BatteryInfo* battery_info_ptr =
      diagnostics::GetBatteryInfo(*info_ptr);
  if (!battery_info_ptr) {
    LOG(ERROR) << "BatteryInfo requested by device does not have a battery.";
    EmitBatteryDataError(metrics::DataError::kNoData);
    std::move(callback).Run(std::move(battery_info));
    return;
  }

  PopulateBatteryInfo(*battery_info_ptr, *battery_info.get());
  std::move(callback).Run(std::move(battery_info));
}

void SystemDataProvider::UpdateBatteryChargeStatus() {
  // Fetch updated data from PowerManagerClient
  std::optional<PowerSupplyProperties> properties =
      chromeos::PowerManagerClient::Get()->GetLastStatus();

  // Fetch updated data from CrosHealthd
  BindCrosHealthdProbeServiceIfNeccessary();

  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kBattery},
      base::BindOnce(&SystemDataProvider::OnBatteryChargeStatusUpdated,
                     weak_factory_.GetWeakPtr(), properties));
}

void SystemDataProvider::UpdateBatteryHealth() {
  BindCrosHealthdProbeServiceIfNeccessary();

  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kBattery},
      base::BindOnce(&SystemDataProvider::OnBatteryHealthUpdated,
                     weak_factory_.GetWeakPtr()));
}

void SystemDataProvider::UpdateMemoryUsage() {
  BindCrosHealthdProbeServiceIfNeccessary();

  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kMemory},
      base::BindOnce(&SystemDataProvider::OnMemoryUsageUpdated,
                     weak_factory_.GetWeakPtr()));
}

void SystemDataProvider::UpdateCpuUsage() {
  BindCrosHealthdProbeServiceIfNeccessary();

  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kCpu},
      base::BindOnce(&SystemDataProvider::OnCpuUsageUpdated,
                     weak_factory_.GetWeakPtr()));
}

void SystemDataProvider::OnBatteryChargeStatusUpdated(
    const std::optional<PowerSupplyProperties>& power_supply_properties,
    healthd::TelemetryInfoPtr info_ptr) {
  mojom::BatteryChargeStatusPtr battery_charge_status =
      mojom::BatteryChargeStatus::New();

  if (info_ptr.is_null()) {
    LOG(ERROR) << "Null response from croshealthd::ProbeTelemetryInfo.";
    NotifyBatteryChargeStatusObservers(battery_charge_status);
    return;
  }

  if (!power_supply_properties.has_value()) {
    LOG(ERROR) << "Null response from power_manager_client::GetLastStatus.";
    EmitBatteryDataError(metrics::DataError::kNoData);
    NotifyBatteryChargeStatusObservers(battery_charge_status);
    return;
  }

  if (!DoesDeviceHaveBattery(*info_ptr) ||
      !DoesDeviceHaveBattery(*power_supply_properties)) {
    if (DoesDeviceHaveBattery(*info_ptr) !=
        DoesDeviceHaveBattery(*power_supply_properties)) {
      LOG(ERROR)
          << "Sources should not disagree about whether there is a battery.";
      EmitBatteryDataError(metrics::DataError::kExpectationNotMet);
    }
    NotifyBatteryChargeStatusObservers(battery_charge_status);
    return;
  }

  PopulateBatteryChargeStatus(*diagnostics::GetBatteryInfo(*info_ptr),
                              *power_supply_properties,
                              *battery_charge_status.get());
  NotifyBatteryChargeStatusObservers(battery_charge_status);
}

void SystemDataProvider::OnBatteryHealthUpdated(
    healthd::TelemetryInfoPtr info_ptr) {
  mojom::BatteryHealthPtr battery_health = mojom::BatteryHealth::New();

  if (info_ptr.is_null()) {
    LOG(ERROR) << "Null response from croshealthd::ProbeTelemetryInfo.";
    NotifyBatteryHealthObservers(battery_health);
    return;
  }

  if (!DoesDeviceHaveBattery(*info_ptr)) {
    NotifyBatteryHealthObservers(battery_health);
    return;
  }

  PopulateBatteryHealth(*diagnostics::GetBatteryInfo(*info_ptr),
                        *battery_health.get());
  NotifyBatteryHealthObservers(battery_health);
}

void SystemDataProvider::OnMemoryUsageUpdated(
    healthd::TelemetryInfoPtr info_ptr) {
  mojom::MemoryUsagePtr memory_usage = mojom::MemoryUsage::New();

  if (info_ptr.is_null()) {
    LOG(ERROR) << "Null response from croshealthd::ProbeTelemetryInfo.";
    NotifyMemoryUsageObservers(memory_usage);
    return;
  }

  const healthd::MemoryInfo* memory_info = GetMemoryInfo(*info_ptr);
  if (memory_info == nullptr) {
    LOG(ERROR) << "No MemoryInfo in response from cros_healthd.";
    NotifyMemoryUsageObservers(memory_usage);
    return;
  }

  PopulateMemoryUsage(*memory_info, *memory_usage.get());
  NotifyMemoryUsageObservers(memory_usage);
}

void SystemDataProvider::OnCpuUsageUpdated(healthd::TelemetryInfoPtr info_ptr) {
  mojom::CpuUsagePtr cpu_usage = mojom::CpuUsage::New();

  if (info_ptr.is_null()) {
    LOG(ERROR) << "Null response from croshealthd::ProbeTelemetryInfo.";
    NotifyCpuUsageObservers(cpu_usage);
    return;
  }

  // TODO(ashleydp): Add metrics to track the occurrence of invalid cros_healthd
  // CpuInfo responses.
  const healthd::CpuInfo* cpu_info = GetCpuInfo(*info_ptr);
  if (cpu_info == nullptr) {
    LOG(ERROR) << "No CpuInfo in response from cros_healthd.";
    NotifyCpuUsageObservers(cpu_usage);
    return;
  }

  ComputeAndPopulateCpuUsage(*cpu_info, *cpu_usage.get());
  PopulateAverageCpuTemperature(*cpu_info, *cpu_usage.get());
  PopulateAverageScaledClockSpeed(*cpu_info, *cpu_usage.get());

  NotifyCpuUsageObservers(cpu_usage);
}

void SystemDataProvider::ComputeAndPopulateCpuUsage(
    const healthd::CpuInfo& cpu_info,
    mojom::CpuUsage& out_cpu_usage) {
  if (cpu_info.physical_cpus.empty()) {
    LOG(ERROR) << "Device reported having zero physical CPUs";
    return;
  }

  if (cpu_info.physical_cpus[0]->logical_cpus.empty()) {
    LOG(ERROR) << "Device reported having zero logical CPUs";
    return;
  }

  // For simplicity, assume that all devices have just one physical CPU, made
  // up of one or more virtual CPUs.

  // TODO(baileyberro): Handle devices with multiple physical CPUs.
  if (cpu_info.physical_cpus.size() > 1) {
    VLOG(1) << "Device has more than one physical CPU";
  }

  const healthd::PhysicalCpuInfoPtr& physical_cpu_ptr =
      cpu_info.physical_cpus[0];

  CpuUsageData new_usage_data =
      CalculateCpuUsage(physical_cpu_ptr->logical_cpus);

  // We use the first usage data we get back as a baseline. On subsequent
  // fetches, we use the previous cumulative data to calculate a delta.
  if (previous_cpu_usage_data_.IsInitialized()) {
    PopulateCpuUsagePercentages(new_usage_data, previous_cpu_usage_data_,
                                out_cpu_usage);
  }

  previous_cpu_usage_data_ = new_usage_data;
}

void SystemDataProvider::NotifyBatteryChargeStatusObservers(
    const mojom::BatteryChargeStatusPtr& battery_charge_status) {
  for (auto& observer : battery_charge_status_observers_) {
    observer->OnBatteryChargeStatusUpdated(battery_charge_status.Clone());
  }
  if (IsLoggingEnabled()) {
    DiagnosticsLogController::Get()
        ->GetTelemetryLog()
        .UpdateBatteryChargeStatus(battery_charge_status.Clone());
  }
}

void SystemDataProvider::NotifyBatteryHealthObservers(
    const mojom::BatteryHealthPtr& battery_health) {
  for (auto& observer : battery_health_observers_) {
    observer->OnBatteryHealthUpdated(battery_health.Clone());
  }
  if (IsLoggingEnabled()) {
    DiagnosticsLogController::Get()->GetTelemetryLog().UpdateBatteryHealth(
        battery_health.Clone());
  }
}

void SystemDataProvider::NotifyMemoryUsageObservers(
    const mojom::MemoryUsagePtr& memory_usage) {
  for (auto& observer : memory_usage_observers_) {
    observer->OnMemoryUsageUpdated(memory_usage.Clone());
  }
  if (IsLoggingEnabled()) {
    DiagnosticsLogController::Get()->GetTelemetryLog().UpdateMemoryUsage(
        memory_usage.Clone());
  }
}

void SystemDataProvider::NotifyCpuUsageObservers(
    const mojom::CpuUsagePtr& cpu_usage) {
  for (auto& observer : cpu_usage_observers_) {
    observer->OnCpuUsageUpdated(cpu_usage.Clone());
  }
  if (IsLoggingEnabled()) {
    DiagnosticsLogController::Get()->GetTelemetryLog().UpdateCpuUsage(
        cpu_usage.Clone());
  }
}

void SystemDataProvider::BindCrosHealthdProbeServiceIfNeccessary() {
  if (!probe_service_ || !probe_service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->BindProbeService(
        probe_service_.BindNewPipeAndPassReceiver());
    probe_service_.set_disconnect_handler(
        base::BindOnce(&SystemDataProvider::OnProbeServiceDisconnect,
                       weak_factory_.GetWeakPtr()));
  }
}

void SystemDataProvider::OnProbeServiceDisconnect() {
  probe_service_.reset();
}

}  // namespace ash::diagnostics

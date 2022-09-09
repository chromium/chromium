// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry_api_converters.h"

#include <inttypes.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "chrome/common/chromeos/extensions/api/telemetry.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"

namespace chromeos {
namespace converters {

namespace {

namespace telemetry_api = ::chromeos::api::os_telemetry;
namespace telemetry_service = ::crosapi::mojom;

}  // namespace

namespace unchecked {

telemetry_api::CpuCStateInfo UncheckedConvertPtr(
    telemetry_service::ProbeCpuCStateInfoPtr input) {
  telemetry_api::CpuCStateInfo result;
  result.name = input->name;
  if (input->time_in_state_since_last_boot_us) {
    result.time_in_state_since_last_boot_us =
        input->time_in_state_since_last_boot_us->value;
  }
  return result;
}

telemetry_api::LogicalCpuInfo UncheckedConvertPtr(
    telemetry_service::ProbeLogicalCpuInfoPtr input) {
  telemetry_api::LogicalCpuInfo result;
  if (input->max_clock_speed_khz) {
    result.max_clock_speed_khz = input->max_clock_speed_khz->value;
  }
  if (input->scaling_max_frequency_khz) {
    result.scaling_max_frequency_khz = input->scaling_max_frequency_khz->value;
  }
  if (input->scaling_current_frequency_khz) {
    result.scaling_current_frequency_khz =
        input->scaling_current_frequency_khz->value;
  }
  if (input->idle_time_ms) {
    result.idle_time_ms = input->idle_time_ms->value;
  }
  result.c_states = ConvertPtrVector<telemetry_api::CpuCStateInfo>(
      std::move(input->c_states));
  return result;
}

telemetry_api::PhysicalCpuInfo UncheckedConvertPtr(
    telemetry_service::ProbePhysicalCpuInfoPtr input) {
  telemetry_api::PhysicalCpuInfo result;
  result.model_name = input->model_name;
  result.logical_cpus = ConvertPtrVector<telemetry_api::LogicalCpuInfo>(
      std::move(input->logical_cpus));
  return result;
}

telemetry_api::BatteryInfo UncheckedConvertPtr(
    telemetry_service::ProbeBatteryInfoPtr input) {
  telemetry_api::BatteryInfo result;
  result.vendor = std::move(input->vendor);
  result.model_name = std::move(input->model_name);
  result.technology = std::move(input->technology);
  result.status = std::move(input->status);
  if (input->cycle_count) {
    result.cycle_count = input->cycle_count->value;
  }
  if (input->voltage_now) {
    result.voltage_now = input->voltage_now->value;
  }
  if (input->charge_full_design) {
    result.charge_full_design = input->charge_full_design->value;
  }
  if (input->charge_full) {
    result.charge_full = input->charge_full->value;
  }
  if (input->voltage_min_design) {
    result.voltage_min_design = input->voltage_min_design->value;
  }
  if (input->charge_now) {
    result.charge_now = input->charge_now->value;
  }
  if (input->current_now) {
    result.current_now = input->current_now->value;
  }
  if (input->temperature) {
    result.temperature = input->temperature->value;
  }
  result.manufacture_date = std::move(input->manufacture_date);

  return result;
}

telemetry_api::OsVersionInfo UncheckedConvertPtr(
    telemetry_service::ProbeOsVersionPtr input) {
  telemetry_api::OsVersionInfo result;

  result.release_milestone = input->release_milestone;
  result.build_number = input->build_number;
  result.patch_number = input->patch_number;
  result.release_channel = input->release_channel;

  return result;
}

telemetry_api::StatefulPartitionInfo UncheckedConvertPtr(
    telemetry_service::ProbeStatefulPartitionInfoPtr input) {
  telemetry_api::StatefulPartitionInfo result;
  if (input->available_space) {
    result.available_space = input->available_space->value;
  }
  if (input->total_space) {
    result.total_space = input->total_space->value;
  }

  return result;
}

}  // namespace unchecked

telemetry_api::CpuArchitectureEnum Convert(
    telemetry_service::ProbeCpuArchitectureEnum input) {
  switch (input) {
    case telemetry_service::ProbeCpuArchitectureEnum::kUnknown:
      return telemetry_api::CpuArchitectureEnum::CPU_ARCHITECTURE_ENUM_UNKNOWN;
    case telemetry_service::ProbeCpuArchitectureEnum::kX86_64:
      return telemetry_api::CpuArchitectureEnum::CPU_ARCHITECTURE_ENUM_X86_64;
    case telemetry_service::ProbeCpuArchitectureEnum::kAArch64:
      return telemetry_api::CpuArchitectureEnum::CPU_ARCHITECTURE_ENUM_AARCH64;
    case telemetry_service::ProbeCpuArchitectureEnum::kArmv7l:
      return telemetry_api::CpuArchitectureEnum::CPU_ARCHITECTURE_ENUM_ARMV7L;
  }
  NOTREACHED();
}

}  // namespace converters
}  // namespace chromeos

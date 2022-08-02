// Copyright 2021 The Chromium Authors. All rights reserved.
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
  if (input->name.has_value()) {
    result.name = std::make_unique<std::string>(input->name.value());
  }
  if (input->time_in_state_since_last_boot_us) {
    result.time_in_state_since_last_boot_us = std::make_unique<double_t>(
        input->time_in_state_since_last_boot_us->value);
  }
  return result;
}

telemetry_api::LogicalCpuInfo UncheckedConvertPtr(
    telemetry_service::ProbeLogicalCpuInfoPtr input) {
  telemetry_api::LogicalCpuInfo result;
  if (input->max_clock_speed_khz) {
    result.max_clock_speed_khz =
        std::make_unique<int32_t>(input->max_clock_speed_khz->value);
  }
  if (input->scaling_max_frequency_khz) {
    result.scaling_max_frequency_khz =
        std::make_unique<int32_t>(input->scaling_max_frequency_khz->value);
  }
  if (input->scaling_current_frequency_khz) {
    result.scaling_current_frequency_khz =
        std::make_unique<int32_t>(input->scaling_current_frequency_khz->value);
  }
  if (input->idle_time_ms) {
    result.idle_time_ms =
        std::make_unique<double_t>(input->idle_time_ms->value);
  }
  result.c_states = ConvertPtrVector<telemetry_api::CpuCStateInfo>(
      std::move(input->c_states));
  return result;
}

telemetry_api::PhysicalCpuInfo UncheckedConvertPtr(
    telemetry_service::ProbePhysicalCpuInfoPtr input) {
  telemetry_api::PhysicalCpuInfo result;
  if (input->model_name.has_value()) {
    result.model_name =
        std::make_unique<std::string>(input->model_name.value());
  }
  result.logical_cpus = ConvertPtrVector<telemetry_api::LogicalCpuInfo>(
      std::move(input->logical_cpus));
  return result;
}

telemetry_api::BatteryInfo UncheckedConvertPtr(
    telemetry_service::ProbeBatteryInfoPtr input) {
  telemetry_api::BatteryInfo result;
  if (input->vendor.has_value()) {
    result.vendor =
        std::make_unique<std::string>(std::move(input->vendor.value()));
  }
  if (input->model_name.has_value()) {
    result.model_name =
        std::make_unique<std::string>(std::move(input->model_name.value()));
  }
  if (input->technology.has_value()) {
    result.technology =
        std::make_unique<std::string>(std::move(input->technology.value()));
  }
  if (input->status.has_value()) {
    result.status =
        std::make_unique<std::string>(std::move(input->status.value()));
  }
  if (input->cycle_count) {
    result.cycle_count = std::make_unique<double_t>(input->cycle_count->value);
  }
  if (input->voltage_now) {
    result.voltage_now = std::make_unique<double_t>(input->voltage_now->value);
  }
  if (input->charge_full_design) {
    result.charge_full_design =
        std::make_unique<double_t>(input->charge_full_design->value);
  }
  if (input->charge_full) {
    result.charge_full = std::make_unique<double_t>(input->charge_full->value);
  }
  if (input->voltage_min_design) {
    result.voltage_min_design =
        std::make_unique<double_t>(input->voltage_min_design->value);
  }
  if (input->charge_now) {
    result.charge_now = std::make_unique<double_t>(input->charge_now->value);
  }
  if (input->current_now) {
    result.current_now = std::make_unique<double_t>(input->current_now->value);
  }
  if (input->temperature) {
    result.temperature = std::make_unique<double_t>(input->temperature->value);
  }
  if (input->manufacture_date.has_value()) {
    result.manufacture_date =
        std::make_unique<std::string>(input->manufacture_date.value());
  }

  return result;
}

telemetry_api::OsVersionInfo UncheckedConvertPtr(
    telemetry_service::ProbeOsVersionPtr input) {
  telemetry_api::OsVersionInfo result;

  if (input->release_milestone) {
    result.release_milestone =
        std::make_unique<std::string>(input->release_milestone.value());
  }

  if (input->build_number) {
    result.build_number =
        std::make_unique<std::string>(input->build_number.value());
  }

  if (input->patch_number) {
    result.patch_number =
        std::make_unique<std::string>(input->patch_number.value());
  }

  if (input->release_channel) {
    result.release_channel =
        std::make_unique<std::string>(input->release_channel.value());
  }

  return result;
}

telemetry_api::StatefulPartitionInfo UncheckedConvertPtr(
    telemetry_service::ProbeStatefulPartitionInfoPtr input) {
  telemetry_api::StatefulPartitionInfo result;
  if (input->available_space) {
    result.available_space =
        std::make_unique<double_t>(input->available_space->value);
  }
  if (input->total_space) {
    result.total_space = std::make_unique<double_t>(input->total_space->value);
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

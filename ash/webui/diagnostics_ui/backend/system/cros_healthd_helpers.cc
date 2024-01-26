// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/system/cros_healthd_helpers.h"

#include <string_view>

#include "ash/webui/diagnostics_ui/backend/common/histogram_util.h"
#include "base/logging.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"

namespace ash::diagnostics {

namespace {

using cros_healthd::mojom::BatteryInfo;
using cros_healthd::mojom::BatteryResult;
using cros_healthd::mojom::BatteryResultPtr;
using cros_healthd::mojom::CpuInfo;
using cros_healthd::mojom::CpuResult;
using cros_healthd::mojom::CpuResultPtr;
using cros_healthd::mojom::MemoryInfo;
using cros_healthd::mojom::MemoryResult;
using cros_healthd::mojom::MemoryResultPtr;
using cros_healthd::mojom::NonInteractiveRoutineUpdate;
using cros_healthd::mojom::RoutineUpdate;
using cros_healthd::mojom::RoutineUpdateUnion;
using cros_healthd::mojom::RoutineUpdateUnionPtr;
using cros_healthd::mojom::SystemInfo;
using cros_healthd::mojom::SystemResult;
using cros_healthd::mojom::SystemResultPtr;
using cros_healthd::mojom::TelemetryInfo;

template <typename TResult, typename TTag>
bool CheckResponse(const TResult& result,
                   TTag expected_tag,
                   std::string_view type_name) {
  if (result.is_null()) {
    LOG(ERROR) << type_name << " not found in croshealthd response.";
    return false;
  }

  auto tag = result->which();
  if (tag == TTag::kError) {
    diagnostics::metrics::EmitCrosHealthdProbeError(type_name,
                                                    result->get_error()->type);
    LOG(ERROR) << "Error retrieving " << type_name
               << "from croshealthd: " << result->get_error()->msg;
    return false;
  }

  DCHECK_EQ(tag, expected_tag);

  return true;
}

}  // namespace

const BatteryInfo* GetBatteryInfo(const TelemetryInfo& info) {
  const BatteryResultPtr& battery_result = info.battery_result;
  if (!CheckResponse(battery_result, BatteryResult::Tag::kBatteryInfo,
                     "battery info")) {
    return nullptr;
  }

  return battery_result->get_battery_info().get();
}

const CpuInfo* GetCpuInfo(const TelemetryInfo& info) {
  const CpuResultPtr& cpu_result = info.cpu_result;
  if (!CheckResponse(cpu_result, CpuResult::Tag::kCpuInfo, "cpu info")) {
    EmitSystemDataError(metrics::DataError::kNoData);
    return nullptr;
  }

  return cpu_result->get_cpu_info().get();
}

const MemoryInfo* GetMemoryInfo(const TelemetryInfo& info) {
  const MemoryResultPtr& memory_result = info.memory_result;
  if (!CheckResponse(memory_result, MemoryResult::Tag::kMemoryInfo,
                     "memory info")) {
    return nullptr;
  }

  return memory_result->get_memory_info().get();
}

const SystemInfo* GetSystemInfo(const TelemetryInfo& info) {
  const SystemResultPtr& system_result = info.system_result;
  if (!CheckResponse(system_result, SystemResult::Tag::kSystemInfo,
                     "system info")) {
    EmitSystemDataError(metrics::DataError::kNoData);
    return nullptr;
  }

  return system_result->get_system_info().get();
}

const NonInteractiveRoutineUpdate* GetNonInteractiveRoutineUpdate(
    const RoutineUpdate& update) {
  const RoutineUpdateUnionPtr& routine_update = update.routine_update_union;

  switch (routine_update->which()) {
    case RoutineUpdateUnion::Tag::kInteractiveUpdate:
      return nullptr;
    case RoutineUpdateUnion::Tag::kNoninteractiveUpdate:
      return routine_update->get_noninteractive_update().get();
  }
}

}  // namespace ash::diagnostics

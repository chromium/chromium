// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/cros_healthd_helpers.h"

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"

namespace ash {
namespace diagnostics {
namespace {

using ::chromeos::cros_healthd::mojom::BatteryInfo;
using ::chromeos::cros_healthd::mojom::BatteryResult;
using ::chromeos::cros_healthd::mojom::BatteryResultPtr;
using ::chromeos::cros_healthd::mojom::CpuInfo;
using ::chromeos::cros_healthd::mojom::CpuResult;
using ::chromeos::cros_healthd::mojom::CpuResultPtr;
using ::chromeos::cros_healthd::mojom::MemoryInfo;
using ::chromeos::cros_healthd::mojom::MemoryResult;
using ::chromeos::cros_healthd::mojom::MemoryResultPtr;
using ::chromeos::cros_healthd::mojom::NonInteractiveRoutineUpdate;
using ::chromeos::cros_healthd::mojom::NonInteractiveRoutineUpdatePtr;
using ::chromeos::cros_healthd::mojom::RoutineUpdate;
using ::chromeos::cros_healthd::mojom::RoutineUpdateUnion;
using ::chromeos::cros_healthd::mojom::RoutineUpdateUnionPtr;
using ::chromeos::cros_healthd::mojom::SystemInfoV2;
using ::chromeos::cros_healthd::mojom::SystemResultV2;
using ::chromeos::cros_healthd::mojom::SystemResultV2Ptr;
using ::chromeos::cros_healthd::mojom::TelemetryInfo;

template <typename TResult, typename TTag>
bool CheckResponse(const TResult& result,
                   TTag expected_tag,
                   base::StringPiece type_name) {
  if (result.is_null()) {
    LOG(ERROR) << type_name << " not found in croshealthd response.";
    return false;
  }

  auto tag = result->which();
  if (tag == TTag::kError) {
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

const SystemInfoV2* GetSystemInfo(const TelemetryInfo& info) {
  const SystemResultV2Ptr& system_result = info.system_result_v2;
  if (!CheckResponse(system_result, SystemResultV2::Tag::kSystemInfoV2,
                     "system info v2")) {
    return nullptr;
  }

  return system_result->get_system_info_v2().get();
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

}  // namespace diagnostics
}  // namespace ash

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics_api_converters.h"

#include "ash/webui/telemetry_extension_ui/mojom/diagnostics_service.mojom.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"

namespace chromeos {
namespace converters {

namespace {

using MojoRoutineStatus = ::ash::health::mojom::DiagnosticRoutineStatusEnum;
using MojoRoutineType = ::ash::health::mojom::DiagnosticRoutineEnum;

using RoutineStatus = ::chromeos::api::os_diagnostics::RoutineStatus;
using RoutineType = ::chromeos::api::os_diagnostics::RoutineType;

}  // namespace

bool ConvertMojoRoutine(MojoRoutineType in, RoutineType* out) {
  DCHECK(out);
  switch (in) {
    case MojoRoutineType::kBatteryCapacity:
      *out = RoutineType::ROUTINE_TYPE_BATTERY_CAPACITY;
      return true;
    case MojoRoutineType::kBatteryCharge:
      *out = RoutineType::ROUTINE_TYPE_BATTERY_CHARGE;
      return true;
    case MojoRoutineType::kBatteryDischarge:
      *out = RoutineType::ROUTINE_TYPE_BATTERY_DISCHARGE;
      return true;
    case MojoRoutineType::kBatteryHealth:
      *out = RoutineType::ROUTINE_TYPE_BATTERY_HEALTH;
      return true;
    case MojoRoutineType::kCpuCache:
      *out = RoutineType::ROUTINE_TYPE_CPU_CACHE;
      return true;
    case MojoRoutineType::kCpuStress:
      *out = RoutineType::ROUTINE_TYPE_CPU_STRESS;
      return true;
    case MojoRoutineType::kMemory:
      *out = RoutineType::ROUTINE_TYPE_MEMORY;
      return true;
    default:
      return false;
  }
}

RoutineStatus ConvertRoutineStatus(MojoRoutineStatus status) {
  switch (status) {
    case MojoRoutineStatus::kReady:
      return RoutineStatus::ROUTINE_STATUS_READY;
    case MojoRoutineStatus::kRunning:
      return RoutineStatus::ROUTINE_STATUS_RUNNING;
    case MojoRoutineStatus::kWaiting:
      return RoutineStatus::ROUTINE_STATUS_WAITING_USER_ACTION;
    case MojoRoutineStatus::kPassed:
      return RoutineStatus::ROUTINE_STATUS_PASSED;
    case MojoRoutineStatus::kFailed:
      return RoutineStatus::ROUTINE_STATUS_FAILED;
    case MojoRoutineStatus::kError:
      return RoutineStatus::ROUTINE_STATUS_ERROR;
    case MojoRoutineStatus::kCancelled:
      return RoutineStatus::ROUTINE_STATUS_CANCELLED;
    case MojoRoutineStatus::kFailedToStart:
      return RoutineStatus::ROUTINE_STATUS_FAILED_TO_START;
    case MojoRoutineStatus::kRemoved:
      return RoutineStatus::ROUTINE_STATUS_REMOVED;
    case MojoRoutineStatus::kCancelling:
      return RoutineStatus::ROUTINE_STATUS_CANCELLING;
    case MojoRoutineStatus::kUnsupported:
      return RoutineStatus::ROUTINE_STATUS_UNSUPPORTED;
    case MojoRoutineStatus::kNotRun:
      return RoutineStatus::ROUTINE_STATUS_NOT_RUN;
  }
}

}  // namespace converters
}  // namespace chromeos

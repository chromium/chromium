// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics_api_converters.h"

#include "base/notreached.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"

namespace chromeos {
namespace converters {

namespace {

using MojoRoutineCommandType = crosapi::mojom::DiagnosticsRoutineCommandEnum;
using MojoRoutineStatus = ::crosapi::mojom::DiagnosticsRoutineStatusEnum;
using MojoRoutineType = ::crosapi::mojom::DiagnosticsRoutineEnum;
using MojoAcPowerStatusType = crosapi::mojom::DiagnosticsAcPowerStatusEnum;
using MojoRoutineUserMessageType =
    crosapi::mojom::DiagnosticsRoutineUserMessageEnum;
using MojoDiskReadRoutineType =
    crosapi::mojom::DiagnosticsDiskReadRoutineTypeEnum;
using MojoNvmeSelfTestType = crosapi::mojom::DiagnosticsNvmeSelfTestTypeEnum;

using RoutineCommandType = ::chromeos::api::os_diagnostics::RoutineCommandType;
using RoutineStatus = ::chromeos::api::os_diagnostics::RoutineStatus;
using RoutineType = ::chromeos::api::os_diagnostics::RoutineType;
using RoutineAcPowerStatusRoutineType =
    ::chromeos::api::os_diagnostics::AcPowerStatus;
using RoutineUserMessageType = ::chromeos::api::os_diagnostics::UserMessageType;
using RoutineDiskReadRoutineType =
    ::chromeos::api::os_diagnostics::DiskReadRoutineType;
using RoutineNvmeSelfTestRoutineType =
    ::chromeos::api::os_diagnostics::RunNvmeSelfTestRequest;

}  // namespace

bool ConvertMojoRoutine(MojoRoutineType in, RoutineType* out) {
  DCHECK(out);
  switch (in) {
    case MojoRoutineType::kAcPower:
      *out = RoutineType::ROUTINE_TYPE_AC_POWER;
      return true;
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
    case MojoRoutineType::kFloatingPointAccuracy:
      *out = RoutineType::ROUTINE_TYPE_CPU_FLOATING_POINT_ACCURACY;
      return true;
    case MojoRoutineType::kPrimeSearch:
      *out = RoutineType::ROUTINE_TYPE_CPU_PRIME_SEARCH;
      return true;
    case MojoRoutineType::kCpuStress:
      *out = RoutineType::ROUTINE_TYPE_CPU_STRESS;
      return true;
    case MojoRoutineType::kDiskRead:
      *out = RoutineType::ROUTINE_TYPE_DISK_READ;
      return true;
    case MojoRoutineType::kDnsResolution:
      *out = RoutineType::ROUTINE_TYPE_DNS_RESOLUTION;
      return true;
    case MojoRoutineType::kDnsResolverPresent:
      *out = RoutineType::ROUTINE_TYPE_DNS_RESOLVER_PRESENT;
      return true;
    case MojoRoutineType::kLanConnectivity:
      *out = RoutineType::ROUTINE_TYPE_LAN_CONNECTIVITY;
      return true;
    case MojoRoutineType::kMemory:
      *out = RoutineType::ROUTINE_TYPE_MEMORY;
      return true;
    case MojoRoutineType::kNvmeWearLevel:
      *out = RoutineType::ROUTINE_TYPE_NVME_WEAR_LEVEL;
      return true;
    case MojoRoutineType::kSignalStrength:
      *out = RoutineType::ROUTINE_TYPE_SIGNAL_STRENGTH;
      return true;
    case MojoRoutineType::kGatewayCanBePinged:
      *out = RoutineType::ROUTINE_TYPE_GATEWAY_CAN_BE_PINGED;
      return true;
    case MojoRoutineType::kSmartctlCheck:
      *out = RoutineType::ROUTINE_TYPE_SMARTCTL_CHECK;
      return true;
    case MojoRoutineType::kSensitiveSensor:
      *out = RoutineType::ROUTINE_TYPE_SENSITIVE_SENSOR;
      return true;
    case MojoRoutineType::kNvmeSelfTest:
      *out = RoutineType::ROUTINE_TYPE_NVME_SELF_TEST;
      return true;
    case MojoRoutineType::kFingerprintAlive:
      *out = RoutineType::ROUTINE_TYPE_FINGERPRINT_ALIVE;
      return true;
    case MojoRoutineType::kSmartctlCheckWithPercentageUsed:
      *out = RoutineType::ROUTINE_TYPE_SMARTCTL_CHECK_WITH_PERCENTAGE_USED;
      return true;
    case MojoRoutineType::kEmmcLifetime:
      *out = RoutineType::ROUTINE_TYPE_EMMC_LIFETIME;
      return true;
    default:
      return false;
  }
}

RoutineStatus ConvertRoutineStatus(MojoRoutineStatus status) {
  switch (status) {
    case MojoRoutineStatus::kUnknown:
      return RoutineStatus::ROUTINE_STATUS_UNKNOWN;
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

MojoRoutineCommandType ConvertRoutineCommand(RoutineCommandType commandType) {
  switch (commandType) {
    case RoutineCommandType::ROUTINE_COMMAND_TYPE_CANCEL:
      return MojoRoutineCommandType::kCancel;
    case RoutineCommandType::ROUTINE_COMMAND_TYPE_REMOVE:
      return MojoRoutineCommandType::kRemove;
    case RoutineCommandType::ROUTINE_COMMAND_TYPE_RESUME:
      return MojoRoutineCommandType::kContinue;
    case RoutineCommandType::ROUTINE_COMMAND_TYPE_STATUS:
      return MojoRoutineCommandType::kGetStatus;
    case RoutineCommandType::ROUTINE_COMMAND_TYPE_NONE:
      break;
  }

  NOTREACHED() << "Unknown command type: " << commandType;
  return static_cast<MojoRoutineCommandType>(
      static_cast<int>(MojoRoutineCommandType::kMaxValue) + 1);
}

MojoAcPowerStatusType ConvertAcPowerStatusRoutineType(
    RoutineAcPowerStatusRoutineType routineType) {
  switch (routineType) {
    case RoutineAcPowerStatusRoutineType::AC_POWER_STATUS_CONNECTED:
      return MojoAcPowerStatusType::kConnected;
    case RoutineAcPowerStatusRoutineType::AC_POWER_STATUS_DISCONNECTED:
      return MojoAcPowerStatusType::kDisconnected;
    case RoutineAcPowerStatusRoutineType::AC_POWER_STATUS_NONE:
      break;
  }

  NOTREACHED() << "Unknown ac power status routine type: " << routineType;
  return static_cast<MojoAcPowerStatusType>(
      static_cast<int>(MojoAcPowerStatusType::kMaxValue) + 1);
}

RoutineUserMessageType ConvertRoutineUserMessage(
    MojoRoutineUserMessageType userMessage) {
  switch (userMessage) {
    case MojoRoutineUserMessageType::kUnknown:
      return RoutineUserMessageType::USER_MESSAGE_TYPE_UNKNOWN;
    case MojoRoutineUserMessageType::kUnplugACPower:
      return RoutineUserMessageType::USER_MESSAGE_TYPE_UNPLUG_AC_POWER;
    case MojoRoutineUserMessageType::kPlugInACPower:
      return RoutineUserMessageType::USER_MESSAGE_TYPE_PLUG_IN_AC_POWER;
  }
}

MojoDiskReadRoutineType ConvertDiskReadRoutineType(
    RoutineDiskReadRoutineType routineType) {
  switch (routineType) {
    case RoutineDiskReadRoutineType::DISK_READ_ROUTINE_TYPE_LINEAR:
      return MojoDiskReadRoutineType::kLinearRead;
    case RoutineDiskReadRoutineType::DISK_READ_ROUTINE_TYPE_RANDOM:
      return MojoDiskReadRoutineType::kRandomRead;
    case RoutineDiskReadRoutineType::DISK_READ_ROUTINE_TYPE_NONE:
      break;
  }

  NOTREACHED() << "Unknown disk read routine type: " << routineType;
  return static_cast<MojoDiskReadRoutineType>(
      static_cast<int>(MojoDiskReadRoutineType::kMaxValue) + 1);
}

MojoNvmeSelfTestType ConvertNvmeSelfTestRoutineType(
    RoutineNvmeSelfTestRoutineType routine_type) {
  switch (routine_type.test_type) {
    case api::os_diagnostics::NVME_SELF_TEST_TYPE_NONE:
      return MojoNvmeSelfTestType::kUnknown;
    case api::os_diagnostics::NVME_SELF_TEST_TYPE_SHORT_TEST:
      return MojoNvmeSelfTestType::kShortSelfTest;
    case api::os_diagnostics::NVME_SELF_TEST_TYPE_LONG_TEST:
      return MojoNvmeSelfTestType::kLongSelfTest;
  }
}

}  // namespace converters
}  // namespace chromeos

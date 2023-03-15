// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/diagnostics_api_converters.h"

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
      *out = RoutineType::kAcPower;
      return true;
    case MojoRoutineType::kBatteryCapacity:
      *out = RoutineType::kBatteryCapacity;
      return true;
    case MojoRoutineType::kBatteryCharge:
      *out = RoutineType::kBatteryCharge;
      return true;
    case MojoRoutineType::kBatteryDischarge:
      *out = RoutineType::kBatteryDischarge;
      return true;
    case MojoRoutineType::kBatteryHealth:
      *out = RoutineType::kBatteryHealth;
      return true;
    case MojoRoutineType::kCpuCache:
      *out = RoutineType::kCpuCache;
      return true;
    case MojoRoutineType::kFloatingPointAccuracy:
      *out = RoutineType::kCpuFloatingPointAccuracy;
      return true;
    case MojoRoutineType::kPrimeSearch:
      *out = RoutineType::kCpuPrimeSearch;
      return true;
    case MojoRoutineType::kCpuStress:
      *out = RoutineType::kCpuStress;
      return true;
    case MojoRoutineType::kDiskRead:
      *out = RoutineType::kDiskRead;
      return true;
    case MojoRoutineType::kDnsResolution:
      *out = RoutineType::kDnsResolution;
      return true;
    case MojoRoutineType::kDnsResolverPresent:
      *out = RoutineType::kDnsResolverPresent;
      return true;
    case MojoRoutineType::kLanConnectivity:
      *out = RoutineType::kLanConnectivity;
      return true;
    case MojoRoutineType::kMemory:
      *out = RoutineType::kMemory;
      return true;
    case MojoRoutineType::kNvmeWearLevel:
      *out = RoutineType::kNvmeWearLevel;
      return true;
    case MojoRoutineType::kSignalStrength:
      *out = RoutineType::kSignalStrength;
      return true;
    case MojoRoutineType::kGatewayCanBePinged:
      *out = RoutineType::kGatewayCanBePinged;
      return true;
    case MojoRoutineType::kSmartctlCheck:
      *out = RoutineType::kSmartctlCheck;
      return true;
    case MojoRoutineType::kSensitiveSensor:
      *out = RoutineType::kSensitiveSensor;
      return true;
    case MojoRoutineType::kNvmeSelfTest:
      *out = RoutineType::kNvmeSelfTest;
      return true;
    case MojoRoutineType::kFingerprintAlive:
      *out = RoutineType::kFingerprintAlive;
      return true;
    case MojoRoutineType::kSmartctlCheckWithPercentageUsed:
      *out = RoutineType::kSmartctlCheckWithPercentageUsed;
      return true;
    case MojoRoutineType::kEmmcLifetime:
      *out = RoutineType::kEmmcLifetime;
      return true;
    default:
      return false;
  }
}

RoutineStatus ConvertRoutineStatus(MojoRoutineStatus status) {
  switch (status) {
    case MojoRoutineStatus::kUnknown:
      return RoutineStatus::kUnknown;
    case MojoRoutineStatus::kReady:
      return RoutineStatus::kReady;
    case MojoRoutineStatus::kRunning:
      return RoutineStatus::kRunning;
    case MojoRoutineStatus::kWaiting:
      return RoutineStatus::kWaitingUserAction;
    case MojoRoutineStatus::kPassed:
      return RoutineStatus::kPassed;
    case MojoRoutineStatus::kFailed:
      return RoutineStatus::kFailed;
    case MojoRoutineStatus::kError:
      return RoutineStatus::kError;
    case MojoRoutineStatus::kCancelled:
      return RoutineStatus::kCancelled;
    case MojoRoutineStatus::kFailedToStart:
      return RoutineStatus::kFailedToStart;
    case MojoRoutineStatus::kRemoved:
      return RoutineStatus::kRemoved;
    case MojoRoutineStatus::kCancelling:
      return RoutineStatus::kCancelling;
    case MojoRoutineStatus::kUnsupported:
      return RoutineStatus::kUnsupported;
    case MojoRoutineStatus::kNotRun:
      return RoutineStatus::kNotRun;
  }
}

MojoRoutineCommandType ConvertRoutineCommand(RoutineCommandType commandType) {
  switch (commandType) {
    case RoutineCommandType::kCancel:
      return MojoRoutineCommandType::kCancel;
    case RoutineCommandType::kRemove:
      return MojoRoutineCommandType::kRemove;
    case RoutineCommandType::kResume:
      return MojoRoutineCommandType::kContinue;
    case RoutineCommandType::kStatus:
      return MojoRoutineCommandType::kGetStatus;
    case RoutineCommandType::kNone:
      break;
  }

  NOTREACHED() << "Unknown command type: " << ToString(commandType);
  return static_cast<MojoRoutineCommandType>(
      static_cast<int>(MojoRoutineCommandType::kMaxValue) + 1);
}

MojoAcPowerStatusType ConvertAcPowerStatusRoutineType(
    RoutineAcPowerStatusRoutineType routineType) {
  switch (routineType) {
    case RoutineAcPowerStatusRoutineType::kConnected:
      return MojoAcPowerStatusType::kConnected;
    case RoutineAcPowerStatusRoutineType::kDisconnected:
      return MojoAcPowerStatusType::kDisconnected;
    case RoutineAcPowerStatusRoutineType::kNone:
      break;
  }

  NOTREACHED() << "Unknown ac power status routine type: "
               << ToString(routineType);
  return static_cast<MojoAcPowerStatusType>(
      static_cast<int>(MojoAcPowerStatusType::kMaxValue) + 1);
}

RoutineUserMessageType ConvertRoutineUserMessage(
    MojoRoutineUserMessageType userMessage) {
  switch (userMessage) {
    case MojoRoutineUserMessageType::kUnknown:
      return RoutineUserMessageType::kUnknown;
    case MojoRoutineUserMessageType::kUnplugACPower:
      return RoutineUserMessageType::kUnplugAcPower;
    case MojoRoutineUserMessageType::kPlugInACPower:
      return RoutineUserMessageType::kPlugInAcPower;
  }
}

MojoDiskReadRoutineType ConvertDiskReadRoutineType(
    RoutineDiskReadRoutineType routineType) {
  switch (routineType) {
    case RoutineDiskReadRoutineType::kLinear:
      return MojoDiskReadRoutineType::kLinearRead;
    case RoutineDiskReadRoutineType::kRandom:
      return MojoDiskReadRoutineType::kRandomRead;
    case RoutineDiskReadRoutineType::kNone:
      break;
  }

  NOTREACHED() << "Unknown disk read routine type: " << ToString(routineType);
  return static_cast<MojoDiskReadRoutineType>(
      static_cast<int>(MojoDiskReadRoutineType::kMaxValue) + 1);
}

MojoNvmeSelfTestType ConvertNvmeSelfTestRoutineType(
    RoutineNvmeSelfTestRoutineType routine_type) {
  switch (routine_type.test_type) {
    case api::os_diagnostics::NvmeSelfTestType::kNone:
      return MojoNvmeSelfTestType::kUnknown;
    case api::os_diagnostics::NvmeSelfTestType::kShortTest:
      return MojoNvmeSelfTestType::kShortSelfTest;
    case api::os_diagnostics::NvmeSelfTestType::kLongTest:
      return MojoNvmeSelfTestType::kLongSelfTest;
  }
}

}  // namespace converters
}  // namespace chromeos

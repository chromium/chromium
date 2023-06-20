// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/diagnostics_api_converters.h"

#include "base/notreached.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"

namespace chromeos::converters {

namespace {

namespace cx_diag = ::chromeos::api::os_diagnostics;
namespace crosapi = ::crosapi::mojom;

}  // namespace

bool ConvertMojoRoutine(crosapi::DiagnosticsRoutineEnum in,
                        cx_diag::RoutineType* out) {
  DCHECK(out);
  switch (in) {
    case crosapi::DiagnosticsRoutineEnum::kAcPower:
      *out = cx_diag::RoutineType::kAcPower;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kBatteryCapacity:
      *out = cx_diag::RoutineType::kBatteryCapacity;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kBatteryCharge:
      *out = cx_diag::RoutineType::kBatteryCharge;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kBatteryDischarge:
      *out = cx_diag::RoutineType::kBatteryDischarge;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kBatteryHealth:
      *out = cx_diag::RoutineType::kBatteryHealth;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kCpuCache:
      *out = cx_diag::RoutineType::kCpuCache;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kFloatingPointAccuracy:
      *out = cx_diag::RoutineType::kCpuFloatingPointAccuracy;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kPrimeSearch:
      *out = cx_diag::RoutineType::kCpuPrimeSearch;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kCpuStress:
      *out = cx_diag::RoutineType::kCpuStress;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kDiskRead:
      *out = cx_diag::RoutineType::kDiskRead;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kDnsResolution:
      *out = cx_diag::RoutineType::kDnsResolution;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kDnsResolverPresent:
      *out = cx_diag::RoutineType::kDnsResolverPresent;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kLanConnectivity:
      *out = cx_diag::RoutineType::kLanConnectivity;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kMemory:
      *out = cx_diag::RoutineType::kMemory;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kNvmeWearLevel:
      *out = cx_diag::RoutineType::kNvmeWearLevel;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kSignalStrength:
      *out = cx_diag::RoutineType::kSignalStrength;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kGatewayCanBePinged:
      *out = cx_diag::RoutineType::kGatewayCanBePinged;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kSmartctlCheck:
      *out = cx_diag::RoutineType::kSmartctlCheck;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kSensitiveSensor:
      *out = cx_diag::RoutineType::kSensitiveSensor;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kNvmeSelfTest:
      *out = cx_diag::RoutineType::kNvmeSelfTest;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kFingerprintAlive:
      *out = cx_diag::RoutineType::kFingerprintAlive;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kSmartctlCheckWithPercentageUsed:
      *out = cx_diag::RoutineType::kSmartctlCheckWithPercentageUsed;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kEmmcLifetime:
      *out = cx_diag::RoutineType::kEmmcLifetime;
      return true;
    default:
      return false;
  }
}

cx_diag::RoutineStatus ConvertRoutineStatus(
    crosapi::DiagnosticsRoutineStatusEnum status) {
  switch (status) {
    case crosapi::DiagnosticsRoutineStatusEnum::kUnknown:
      return cx_diag::RoutineStatus::kUnknown;
    case crosapi::DiagnosticsRoutineStatusEnum::kReady:
      return cx_diag::RoutineStatus::kReady;
    case crosapi::DiagnosticsRoutineStatusEnum::kRunning:
      return cx_diag::RoutineStatus::kRunning;
    case crosapi::DiagnosticsRoutineStatusEnum::kWaiting:
      return cx_diag::RoutineStatus::kWaitingUserAction;
    case crosapi::DiagnosticsRoutineStatusEnum::kPassed:
      return cx_diag::RoutineStatus::kPassed;
    case crosapi::DiagnosticsRoutineStatusEnum::kFailed:
      return cx_diag::RoutineStatus::kFailed;
    case crosapi::DiagnosticsRoutineStatusEnum::kError:
      return cx_diag::RoutineStatus::kError;
    case crosapi::DiagnosticsRoutineStatusEnum::kCancelled:
      return cx_diag::RoutineStatus::kCancelled;
    case crosapi::DiagnosticsRoutineStatusEnum::kFailedToStart:
      return cx_diag::RoutineStatus::kFailedToStart;
    case crosapi::DiagnosticsRoutineStatusEnum::kRemoved:
      return cx_diag::RoutineStatus::kRemoved;
    case crosapi::DiagnosticsRoutineStatusEnum::kCancelling:
      return cx_diag::RoutineStatus::kCancelling;
    case crosapi::DiagnosticsRoutineStatusEnum::kUnsupported:
      return cx_diag::RoutineStatus::kUnsupported;
    case crosapi::DiagnosticsRoutineStatusEnum::kNotRun:
      return cx_diag::RoutineStatus::kNotRun;
  }
}

crosapi::DiagnosticsRoutineCommandEnum ConvertRoutineCommand(
    cx_diag::RoutineCommandType commandType) {
  switch (commandType) {
    case cx_diag::RoutineCommandType::kCancel:
      return crosapi::DiagnosticsRoutineCommandEnum::kCancel;
    case cx_diag::RoutineCommandType::kRemove:
      return crosapi::DiagnosticsRoutineCommandEnum::kRemove;
    case cx_diag::RoutineCommandType::kResume:
      return crosapi::DiagnosticsRoutineCommandEnum::kContinue;
    case cx_diag::RoutineCommandType::kStatus:
      return crosapi::DiagnosticsRoutineCommandEnum::kGetStatus;
    case cx_diag::RoutineCommandType::kNone:
      break;
  }

  NOTREACHED() << "Unknown command type: " << ToString(commandType);
  return static_cast<crosapi::DiagnosticsRoutineCommandEnum>(
      static_cast<int>(crosapi::DiagnosticsRoutineCommandEnum::kMaxValue) + 1);
}

crosapi::DiagnosticsAcPowerStatusEnum ConvertAcPowerStatusRoutineType(
    cx_diag::AcPowerStatus routineType) {
  switch (routineType) {
    case cx_diag::AcPowerStatus::kConnected:
      return crosapi::DiagnosticsAcPowerStatusEnum::kConnected;
    case cx_diag::AcPowerStatus::kDisconnected:
      return crosapi::DiagnosticsAcPowerStatusEnum::kDisconnected;
    case cx_diag::AcPowerStatus::kNone:
      break;
  }

  NOTREACHED() << "Unknown ac power status routine type: "
               << ToString(routineType);
  return static_cast<crosapi::DiagnosticsAcPowerStatusEnum>(
      static_cast<int>(crosapi::DiagnosticsAcPowerStatusEnum::kMaxValue) + 1);
}

cx_diag::UserMessageType ConvertRoutineUserMessage(
    crosapi::DiagnosticsRoutineUserMessageEnum userMessage) {
  switch (userMessage) {
    case crosapi::DiagnosticsRoutineUserMessageEnum::kUnknown:
      return cx_diag::UserMessageType::kUnknown;
    case crosapi::DiagnosticsRoutineUserMessageEnum::kUnplugACPower:
      return cx_diag::UserMessageType::kUnplugAcPower;
    case crosapi::DiagnosticsRoutineUserMessageEnum::kPlugInACPower:
      return cx_diag::UserMessageType::kPlugInAcPower;
  }
}

crosapi::DiagnosticsDiskReadRoutineTypeEnum ConvertDiskReadRoutineType(
    cx_diag::DiskReadRoutineType routineType) {
  switch (routineType) {
    case cx_diag::DiskReadRoutineType::kLinear:
      return crosapi::DiagnosticsDiskReadRoutineTypeEnum::kLinearRead;
    case cx_diag::DiskReadRoutineType::kRandom:
      return crosapi::DiagnosticsDiskReadRoutineTypeEnum::kRandomRead;
    case cx_diag::DiskReadRoutineType::kNone:
      break;
  }

  NOTREACHED() << "Unknown disk read routine type: " << ToString(routineType);
  return static_cast<crosapi::DiagnosticsDiskReadRoutineTypeEnum>(
      static_cast<int>(crosapi::DiagnosticsDiskReadRoutineTypeEnum::kMaxValue) +
      1);
}

crosapi::DiagnosticsNvmeSelfTestTypeEnum ConvertNvmeSelfTestRoutineType(
    cx_diag::RunNvmeSelfTestRequest routine_type) {
  switch (routine_type.test_type) {
    case cx_diag::NvmeSelfTestType::kNone:
      return crosapi::DiagnosticsNvmeSelfTestTypeEnum::kUnknown;
    case cx_diag::NvmeSelfTestType::kShortTest:
      return crosapi::DiagnosticsNvmeSelfTestTypeEnum::kShortSelfTest;
    case cx_diag::NvmeSelfTestType::kLongTest:
      return crosapi::DiagnosticsNvmeSelfTestTypeEnum::kLongSelfTest;
  }
}

}  // namespace chromeos::converters

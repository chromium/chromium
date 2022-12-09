// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics_api_converters.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace converters {

namespace {

using MojoRoutineCommandType = crosapi::mojom::DiagnosticsRoutineCommandEnum;
using MojoRoutineStatus = ::crosapi::mojom::DiagnosticsRoutineStatusEnum;
using MojoRoutineType = ::crosapi::mojom::DiagnosticsRoutineEnum;
using MojoRoutineUserMessageType =
    crosapi::mojom::DiagnosticsRoutineUserMessageEnum;
using MojoDiskReadRoutineType =
    crosapi::mojom::DiagnosticsDiskReadRoutineTypeEnum;
using MojoAcPowerStatusType = crosapi::mojom::DiagnosticsAcPowerStatusEnum;
using MojoNvmeSelfTestType = crosapi::mojom::DiagnosticsNvmeSelfTestTypeEnum;

using RoutineCommandType = ::chromeos::api::os_diagnostics::RoutineCommandType;
using RoutineStatus = ::chromeos::api::os_diagnostics::RoutineStatus;
using RoutineType = ::chromeos::api::os_diagnostics::RoutineType;
using RoutineUserMessageType = ::chromeos::api::os_diagnostics::UserMessageType;
using RoutineDiskReadRoutineType =
    ::chromeos::api::os_diagnostics::DiskReadRoutineType;
using RoutineAcPowerStatusRoutineType =
    ::chromeos::api::os_diagnostics::AcPowerStatus;
using RoutineNvmeSelfTestRoutineType =
    ::chromeos::api::os_diagnostics::RunNvmeSelfTestRequest;
using RoutineNvmeSelfTestEnum =
    ::chromeos::api::os_diagnostics::NvmeSelfTestType;

}  // namespace

// Tests that ConvertMojoRoutineTest() correctly converts the supported Mojo
// routine type values to the API's routine type values. For the unsupported
// type values, the call should fail (ConvertMojoRoutineTest() returns false);
TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertMojoRoutineTest) {
  // Tests for supported routines.
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kAcPower, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_AC_POWER);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kBatteryCapacity, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_BATTERY_CAPACITY);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kBatteryCharge, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_BATTERY_CHARGE);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kBatteryDischarge, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_BATTERY_DISCHARGE);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kBatteryHealth, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_BATTERY_HEALTH);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kCpuCache, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_CPU_CACHE);
  }
  {
    RoutineType out;
    EXPECT_TRUE(
        ConvertMojoRoutine(MojoRoutineType::kFloatingPointAccuracy, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_CPU_FLOATING_POINT_ACCURACY);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kPrimeSearch, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_CPU_PRIME_SEARCH);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kCpuStress, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_CPU_STRESS);
  }
  {
    RoutineType out = RoutineType::ROUTINE_TYPE_NONE;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kDiskRead, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_DISK_READ);
  }
  {
    RoutineType out = RoutineType::ROUTINE_TYPE_NONE;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kDnsResolution, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_DNS_RESOLUTION);
  }
  {
    RoutineType out = RoutineType::ROUTINE_TYPE_NONE;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kDnsResolverPresent, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_DNS_RESOLVER_PRESENT);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kMemory, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_MEMORY);
  }
  {
    RoutineType out = RoutineType::ROUTINE_TYPE_NONE;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kNvmeSelfTest, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_NVME_SELF_TEST);
  }
  {
    RoutineType out = RoutineType::ROUTINE_TYPE_NONE;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kNvmeWearLevel, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_NVME_WEAR_LEVEL);
  }
  {
    RoutineType out = RoutineType::ROUTINE_TYPE_NONE;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kSignalStrength, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_SIGNAL_STRENGTH);
  }
  {
    RoutineType out = RoutineType::ROUTINE_TYPE_NONE;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kGatewayCanBePinged, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_GATEWAY_CAN_BE_PINGED);
  }
  {
    RoutineType out = RoutineType::ROUTINE_TYPE_NONE;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kSensitiveSensor, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_SENSITIVE_SENSOR);
  }
  {
    RoutineType out = RoutineType::ROUTINE_TYPE_NONE;
    EXPECT_TRUE(ConvertMojoRoutine(
        MojoRoutineType::kSmartctlCheckWithPercentageUsed, &out));
    EXPECT_EQ(out,
              RoutineType::ROUTINE_TYPE_SMARTCTL_CHECK_WITH_PERCENTAGE_USED);
  }
  {
    RoutineType out = RoutineType::ROUTINE_TYPE_NONE;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kSmartctlCheck, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_SMARTCTL_CHECK);
  }
  {
    RoutineType out = RoutineType::ROUTINE_TYPE_NONE;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kFingerprintAlive, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_FINGERPRINT_ALIVE);
  }
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest, ConvertRoutineStatus) {
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kReady),
            RoutineStatus::ROUTINE_STATUS_READY);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kRunning),
            RoutineStatus::ROUTINE_STATUS_RUNNING);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kWaiting),
            RoutineStatus::ROUTINE_STATUS_WAITING_USER_ACTION);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kPassed),
            RoutineStatus::ROUTINE_STATUS_PASSED);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kFailed),
            RoutineStatus::ROUTINE_STATUS_FAILED);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kError),
            RoutineStatus::ROUTINE_STATUS_ERROR);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kCancelled),
            RoutineStatus::ROUTINE_STATUS_CANCELLED);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kFailedToStart),
            RoutineStatus::ROUTINE_STATUS_FAILED_TO_START);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kRemoved),
            RoutineStatus::ROUTINE_STATUS_REMOVED);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kCancelling),
            RoutineStatus::ROUTINE_STATUS_CANCELLING);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kUnsupported),
            RoutineStatus::ROUTINE_STATUS_UNSUPPORTED);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kNotRun),
            RoutineStatus::ROUTINE_STATUS_NOT_RUN);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineCommand) {
  EXPECT_EQ(
      ConvertRoutineCommand(RoutineCommandType::ROUTINE_COMMAND_TYPE_CANCEL),
      MojoRoutineCommandType::kCancel);
  EXPECT_EQ(
      ConvertRoutineCommand(RoutineCommandType::ROUTINE_COMMAND_TYPE_REMOVE),
      MojoRoutineCommandType::kRemove);
  EXPECT_EQ(
      ConvertRoutineCommand(RoutineCommandType::ROUTINE_COMMAND_TYPE_RESUME),
      MojoRoutineCommandType::kContinue);
  EXPECT_EQ(
      ConvertRoutineCommand(RoutineCommandType::ROUTINE_COMMAND_TYPE_STATUS),
      MojoRoutineCommandType::kGetStatus);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineUserMessage) {
  EXPECT_EQ(
      ConvertRoutineUserMessage(MojoRoutineUserMessageType::kUnplugACPower),
      RoutineUserMessageType::USER_MESSAGE_TYPE_UNPLUG_AC_POWER);
  EXPECT_EQ(
      ConvertRoutineUserMessage(MojoRoutineUserMessageType::kPlugInACPower),
      RoutineUserMessageType::USER_MESSAGE_TYPE_PLUG_IN_AC_POWER);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertDiskReadRoutineType) {
  EXPECT_EQ(ConvertDiskReadRoutineType(
                RoutineDiskReadRoutineType::DISK_READ_ROUTINE_TYPE_LINEAR),
            MojoDiskReadRoutineType::kLinearRead);
  EXPECT_EQ(ConvertDiskReadRoutineType(
                RoutineDiskReadRoutineType::DISK_READ_ROUTINE_TYPE_RANDOM),
            MojoDiskReadRoutineType::kRandomRead);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertAcPowerStatusRoutineType) {
  EXPECT_EQ(ConvertAcPowerStatusRoutineType(
                RoutineAcPowerStatusRoutineType::AC_POWER_STATUS_CONNECTED),
            MojoAcPowerStatusType::kConnected);
  EXPECT_EQ(ConvertAcPowerStatusRoutineType(
                RoutineAcPowerStatusRoutineType::AC_POWER_STATUS_DISCONNECTED),
            MojoAcPowerStatusType::kDisconnected);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertNvmeSelfTestRoutineType) {
  RoutineNvmeSelfTestRoutineType input_short;
  input_short.test_type =
      RoutineNvmeSelfTestEnum::NVME_SELF_TEST_TYPE_SHORT_TEST;
  EXPECT_EQ(ConvertNvmeSelfTestRoutineType(std::move(input_short)),
            MojoNvmeSelfTestType::kShortSelfTest);

  RoutineNvmeSelfTestRoutineType input_long;
  input_long.test_type = RoutineNvmeSelfTestEnum::NVME_SELF_TEST_TYPE_LONG_TEST;
  EXPECT_EQ(ConvertNvmeSelfTestRoutineType(std::move(input_long)),
            MojoNvmeSelfTestType::kLongSelfTest);

  RoutineNvmeSelfTestRoutineType input_unknown;
  input_unknown.test_type = RoutineNvmeSelfTestEnum::NVME_SELF_TEST_TYPE_NONE;
  EXPECT_EQ(ConvertNvmeSelfTestRoutineType(std::move(input_unknown)),
            MojoNvmeSelfTestType::kUnknown);
}

}  // namespace converters
}  // namespace chromeos

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/diagnostics_api_converters.h"
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
    EXPECT_EQ(out, RoutineType::kAcPower);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kBatteryCapacity, &out));
    EXPECT_EQ(out, RoutineType::kBatteryCapacity);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kBatteryCharge, &out));
    EXPECT_EQ(out, RoutineType::kBatteryCharge);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kBatteryDischarge, &out));
    EXPECT_EQ(out, RoutineType::kBatteryDischarge);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kBatteryHealth, &out));
    EXPECT_EQ(out, RoutineType::kBatteryHealth);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kCpuCache, &out));
    EXPECT_EQ(out, RoutineType::kCpuCache);
  }
  {
    RoutineType out;
    EXPECT_TRUE(
        ConvertMojoRoutine(MojoRoutineType::kFloatingPointAccuracy, &out));
    EXPECT_EQ(out, RoutineType::kCpuFloatingPointAccuracy);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kPrimeSearch, &out));
    EXPECT_EQ(out, RoutineType::kCpuPrimeSearch);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kCpuStress, &out));
    EXPECT_EQ(out, RoutineType::kCpuStress);
  }
  {
    RoutineType out = RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kDiskRead, &out));
    EXPECT_EQ(out, RoutineType::kDiskRead);
  }
  {
    RoutineType out = RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kDnsResolution, &out));
    EXPECT_EQ(out, RoutineType::kDnsResolution);
  }
  {
    RoutineType out = RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kDnsResolverPresent, &out));
    EXPECT_EQ(out, RoutineType::kDnsResolverPresent);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kMemory, &out));
    EXPECT_EQ(out, RoutineType::kMemory);
  }
  {
    RoutineType out = RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kNvmeSelfTest, &out));
    EXPECT_EQ(out, RoutineType::kNvmeSelfTest);
  }
  {
    RoutineType out = RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kNvmeWearLevel, &out));
    EXPECT_EQ(out, RoutineType::kNvmeWearLevel);
  }
  {
    RoutineType out = RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kSignalStrength, &out));
    EXPECT_EQ(out, RoutineType::kSignalStrength);
  }
  {
    RoutineType out = RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kGatewayCanBePinged, &out));
    EXPECT_EQ(out, RoutineType::kGatewayCanBePinged);
  }
  {
    RoutineType out = RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kSensitiveSensor, &out));
    EXPECT_EQ(out, RoutineType::kSensitiveSensor);
  }
  {
    RoutineType out = RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(
        MojoRoutineType::kSmartctlCheckWithPercentageUsed, &out));
    EXPECT_EQ(out, RoutineType::kSmartctlCheckWithPercentageUsed);
  }
  {
    RoutineType out = RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kSmartctlCheck, &out));
    EXPECT_EQ(out, RoutineType::kSmartctlCheck);
  }
  {
    RoutineType out = RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kFingerprintAlive, &out));
    EXPECT_EQ(out, RoutineType::kFingerprintAlive);
  }
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest, ConvertRoutineStatus) {
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kReady),
            RoutineStatus::kReady);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kRunning),
            RoutineStatus::kRunning);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kWaiting),
            RoutineStatus::kWaitingUserAction);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kPassed),
            RoutineStatus::kPassed);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kFailed),
            RoutineStatus::kFailed);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kError),
            RoutineStatus::kError);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kCancelled),
            RoutineStatus::kCancelled);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kFailedToStart),
            RoutineStatus::kFailedToStart);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kRemoved),
            RoutineStatus::kRemoved);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kCancelling),
            RoutineStatus::kCancelling);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kUnsupported),
            RoutineStatus::kUnsupported);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kNotRun),
            RoutineStatus::kNotRun);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineCommand) {
  EXPECT_EQ(ConvertRoutineCommand(RoutineCommandType::kCancel),
            MojoRoutineCommandType::kCancel);
  EXPECT_EQ(ConvertRoutineCommand(RoutineCommandType::kRemove),
            MojoRoutineCommandType::kRemove);
  EXPECT_EQ(ConvertRoutineCommand(RoutineCommandType::kResume),
            MojoRoutineCommandType::kContinue);
  EXPECT_EQ(ConvertRoutineCommand(RoutineCommandType::kStatus),
            MojoRoutineCommandType::kGetStatus);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineUserMessage) {
  EXPECT_EQ(
      ConvertRoutineUserMessage(MojoRoutineUserMessageType::kUnplugACPower),
      RoutineUserMessageType::kUnplugAcPower);
  EXPECT_EQ(
      ConvertRoutineUserMessage(MojoRoutineUserMessageType::kPlugInACPower),
      RoutineUserMessageType::kPlugInAcPower);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertDiskReadRoutineType) {
  EXPECT_EQ(ConvertDiskReadRoutineType(RoutineDiskReadRoutineType::kLinear),
            MojoDiskReadRoutineType::kLinearRead);
  EXPECT_EQ(ConvertDiskReadRoutineType(RoutineDiskReadRoutineType::kRandom),
            MojoDiskReadRoutineType::kRandomRead);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertAcPowerStatusRoutineType) {
  EXPECT_EQ(ConvertAcPowerStatusRoutineType(
                RoutineAcPowerStatusRoutineType::kConnected),
            MojoAcPowerStatusType::kConnected);
  EXPECT_EQ(ConvertAcPowerStatusRoutineType(
                RoutineAcPowerStatusRoutineType::kDisconnected),
            MojoAcPowerStatusType::kDisconnected);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertNvmeSelfTestRoutineType) {
  RoutineNvmeSelfTestRoutineType input_short;
  input_short.test_type = RoutineNvmeSelfTestEnum::kShortTest;
  EXPECT_EQ(ConvertNvmeSelfTestRoutineType(std::move(input_short)),
            MojoNvmeSelfTestType::kShortSelfTest);

  RoutineNvmeSelfTestRoutineType input_long;
  input_long.test_type = RoutineNvmeSelfTestEnum::kLongTest;
  EXPECT_EQ(ConvertNvmeSelfTestRoutineType(std::move(input_long)),
            MojoNvmeSelfTestType::kLongSelfTest);

  RoutineNvmeSelfTestRoutineType input_unknown;
  input_unknown.test_type = RoutineNvmeSelfTestEnum::kNone;
  EXPECT_EQ(ConvertNvmeSelfTestRoutineType(std::move(input_unknown)),
            MojoNvmeSelfTestType::kUnknown);
}

}  // namespace converters
}  // namespace chromeos

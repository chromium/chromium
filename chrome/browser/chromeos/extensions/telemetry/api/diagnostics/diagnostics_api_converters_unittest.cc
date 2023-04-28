// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/diagnostics_api_converters.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::converters {

namespace {

namespace cx_diag = ::chromeos::api::os_diagnostics;
namespace crosapi = ::crosapi::mojom;

}  // namespace

// Tests that ConvertMojoRoutineTest() correctly converts the supported Mojo
// routine type values to the API's routine type values. For the unsupported
// type values, the call should fail (ConvertMojoRoutineTest() returns false);
TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertMojoRoutineTest) {
  // Tests for supported routines.
  {
    cx_diag::RoutineType out;
    EXPECT_TRUE(
        ConvertMojoRoutine(crosapi::DiagnosticsRoutineEnum::kAcPower, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kAcPower);
  }
  {
    cx_diag::RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kBatteryCapacity, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kBatteryCapacity);
  }
  {
    cx_diag::RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kBatteryCharge, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kBatteryCharge);
  }
  {
    cx_diag::RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kBatteryDischarge, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kBatteryDischarge);
  }
  {
    cx_diag::RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kBatteryHealth, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kBatteryHealth);
  }
  {
    cx_diag::RoutineType out;
    EXPECT_TRUE(
        ConvertMojoRoutine(crosapi::DiagnosticsRoutineEnum::kCpuCache, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kCpuCache);
  }
  {
    cx_diag::RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kFloatingPointAccuracy, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kCpuFloatingPointAccuracy);
  }
  {
    cx_diag::RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kPrimeSearch, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kCpuPrimeSearch);
  }
  {
    cx_diag::RoutineType out;
    EXPECT_TRUE(
        ConvertMojoRoutine(crosapi::DiagnosticsRoutineEnum::kCpuStress, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kCpuStress);
  }
  {
    cx_diag::RoutineType out = cx_diag::RoutineType::kNone;
    EXPECT_TRUE(
        ConvertMojoRoutine(crosapi::DiagnosticsRoutineEnum::kDiskRead, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kDiskRead);
  }
  {
    cx_diag::RoutineType out = cx_diag::RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kDnsResolution, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kDnsResolution);
  }
  {
    cx_diag::RoutineType out = cx_diag::RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kDnsResolverPresent, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kDnsResolverPresent);
  }
  {
    cx_diag::RoutineType out;
    EXPECT_TRUE(
        ConvertMojoRoutine(crosapi::DiagnosticsRoutineEnum::kMemory, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kMemory);
  }
  {
    cx_diag::RoutineType out = cx_diag::RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kNvmeSelfTest, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kNvmeSelfTest);
  }
  {
    cx_diag::RoutineType out = cx_diag::RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kNvmeWearLevel, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kNvmeWearLevel);
  }
  {
    cx_diag::RoutineType out = cx_diag::RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kSignalStrength, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kSignalStrength);
  }
  {
    cx_diag::RoutineType out = cx_diag::RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kGatewayCanBePinged, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kGatewayCanBePinged);
  }
  {
    cx_diag::RoutineType out = cx_diag::RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kSensitiveSensor, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kSensitiveSensor);
  }
  {
    cx_diag::RoutineType out = cx_diag::RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kSmartctlCheckWithPercentageUsed,
        &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kSmartctlCheckWithPercentageUsed);
  }
  {
    cx_diag::RoutineType out = cx_diag::RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kSmartctlCheck, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kSmartctlCheck);
  }
  {
    cx_diag::RoutineType out = cx_diag::RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kFingerprintAlive, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kFingerprintAlive);
  }
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest, ConvertRoutineStatus) {
  EXPECT_EQ(ConvertRoutineStatus(crosapi::DiagnosticsRoutineStatusEnum::kReady),
            cx_diag::RoutineStatus::kReady);
  EXPECT_EQ(
      ConvertRoutineStatus(crosapi::DiagnosticsRoutineStatusEnum::kRunning),
      cx_diag::RoutineStatus::kRunning);
  EXPECT_EQ(
      ConvertRoutineStatus(crosapi::DiagnosticsRoutineStatusEnum::kWaiting),
      cx_diag::RoutineStatus::kWaitingUserAction);
  EXPECT_EQ(
      ConvertRoutineStatus(crosapi::DiagnosticsRoutineStatusEnum::kPassed),
      cx_diag::RoutineStatus::kPassed);
  EXPECT_EQ(
      ConvertRoutineStatus(crosapi::DiagnosticsRoutineStatusEnum::kFailed),
      cx_diag::RoutineStatus::kFailed);
  EXPECT_EQ(ConvertRoutineStatus(crosapi::DiagnosticsRoutineStatusEnum::kError),
            cx_diag::RoutineStatus::kError);
  EXPECT_EQ(
      ConvertRoutineStatus(crosapi::DiagnosticsRoutineStatusEnum::kCancelled),
      cx_diag::RoutineStatus::kCancelled);
  EXPECT_EQ(ConvertRoutineStatus(
                crosapi::DiagnosticsRoutineStatusEnum::kFailedToStart),
            cx_diag::RoutineStatus::kFailedToStart);
  EXPECT_EQ(
      ConvertRoutineStatus(crosapi::DiagnosticsRoutineStatusEnum::kRemoved),
      cx_diag::RoutineStatus::kRemoved);
  EXPECT_EQ(
      ConvertRoutineStatus(crosapi::DiagnosticsRoutineStatusEnum::kCancelling),
      cx_diag::RoutineStatus::kCancelling);
  EXPECT_EQ(
      ConvertRoutineStatus(crosapi::DiagnosticsRoutineStatusEnum::kUnsupported),
      cx_diag::RoutineStatus::kUnsupported);
  EXPECT_EQ(
      ConvertRoutineStatus(crosapi::DiagnosticsRoutineStatusEnum::kNotRun),
      cx_diag::RoutineStatus::kNotRun);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineCommand) {
  EXPECT_EQ(ConvertRoutineCommand(cx_diag::RoutineCommandType::kCancel),
            crosapi::DiagnosticsRoutineCommandEnum::kCancel);
  EXPECT_EQ(ConvertRoutineCommand(cx_diag::RoutineCommandType::kRemove),
            crosapi::DiagnosticsRoutineCommandEnum::kRemove);
  EXPECT_EQ(ConvertRoutineCommand(cx_diag::RoutineCommandType::kResume),
            crosapi::DiagnosticsRoutineCommandEnum::kContinue);
  EXPECT_EQ(ConvertRoutineCommand(cx_diag::RoutineCommandType::kStatus),
            crosapi::DiagnosticsRoutineCommandEnum::kGetStatus);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineUserMessage) {
  EXPECT_EQ(ConvertRoutineUserMessage(
                crosapi::DiagnosticsRoutineUserMessageEnum::kUnplugACPower),
            cx_diag::UserMessageType::kUnplugAcPower);
  EXPECT_EQ(ConvertRoutineUserMessage(
                crosapi::DiagnosticsRoutineUserMessageEnum::kPlugInACPower),
            cx_diag::UserMessageType::kPlugInAcPower);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertDiskReadRoutineType) {
  EXPECT_EQ(ConvertDiskReadRoutineType(cx_diag::DiskReadRoutineType::kLinear),
            crosapi::DiagnosticsDiskReadRoutineTypeEnum::kLinearRead);
  EXPECT_EQ(ConvertDiskReadRoutineType(cx_diag::DiskReadRoutineType::kRandom),
            crosapi::DiagnosticsDiskReadRoutineTypeEnum::kRandomRead);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertAcPowerStatusRoutineType) {
  EXPECT_EQ(ConvertAcPowerStatusRoutineType(cx_diag::AcPowerStatus::kConnected),
            crosapi::DiagnosticsAcPowerStatusEnum::kConnected);
  EXPECT_EQ(
      ConvertAcPowerStatusRoutineType(cx_diag::AcPowerStatus::kDisconnected),
      crosapi::DiagnosticsAcPowerStatusEnum::kDisconnected);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertNvmeSelfTestRoutineType) {
  cx_diag::RunNvmeSelfTestRequest input_short;
  input_short.test_type = cx_diag::NvmeSelfTestType::kShortTest;
  EXPECT_EQ(ConvertNvmeSelfTestRoutineType(std::move(input_short)),
            crosapi::DiagnosticsNvmeSelfTestTypeEnum::kShortSelfTest);

  cx_diag::RunNvmeSelfTestRequest input_long;
  input_long.test_type = cx_diag::NvmeSelfTestType::kLongTest;
  EXPECT_EQ(ConvertNvmeSelfTestRoutineType(std::move(input_long)),
            crosapi::DiagnosticsNvmeSelfTestTypeEnum::kLongSelfTest);

  cx_diag::RunNvmeSelfTestRequest input_unknown;
  input_unknown.test_type = cx_diag::NvmeSelfTestType::kNone;
  EXPECT_EQ(ConvertNvmeSelfTestRoutineType(std::move(input_unknown)),
            crosapi::DiagnosticsNvmeSelfTestTypeEnum::kUnknown);
}

}  // namespace chromeos::converters

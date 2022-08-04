// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/diagnostics_service_converters.h"

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace converters {

// Tests that |ConvertDiagnosticsPtr| function returns nullptr if input is
// nullptr. ConvertDiagnosticsPtr is a template, so we can test this function
// with any valid type.
TEST(DiagnosticsServiceConvertersTest, ConvertDiagnosticsPtrTakesNullPtr) {
  EXPECT_TRUE(
      ConvertDiagnosticsPtr(cros_healthd::mojom::InteractiveRoutineUpdatePtr())
          .is_null());
}

TEST(DiagnosticsServiceConvertersTest, ConvertDiagnosticRoutineStatusEnum) {
  namespace cros_healthd = ::chromeos::cros_healthd::mojom;
  namespace crosapi = ::crosapi::mojom;

  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kReady),
            crosapi::DiagnosticsRoutineStatusEnum::kReady);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kRunning),
            crosapi::DiagnosticsRoutineStatusEnum::kRunning);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kWaiting),
            crosapi::DiagnosticsRoutineStatusEnum::kWaiting);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kPassed),
            crosapi::DiagnosticsRoutineStatusEnum::kPassed);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kFailed),
            crosapi::DiagnosticsRoutineStatusEnum::kFailed);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kError),
            crosapi::DiagnosticsRoutineStatusEnum::kError);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kCancelled),
            crosapi::DiagnosticsRoutineStatusEnum::kCancelled);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kFailedToStart),
            crosapi::DiagnosticsRoutineStatusEnum::kFailedToStart);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kRemoved),
            crosapi::DiagnosticsRoutineStatusEnum::kRemoved);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kCancelling),
            crosapi::DiagnosticsRoutineStatusEnum::kCancelling);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kUnsupported),
            crosapi::DiagnosticsRoutineStatusEnum::kUnsupported);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kNotRun),
            crosapi::DiagnosticsRoutineStatusEnum::kNotRun);
}

TEST(DiagnosticsServiceConvertersTest,
     ConvertDiagnosticRoutineUserMessageEnum) {
  namespace cros_healthd = ::chromeos::cros_healthd::mojom;
  namespace crosapi = ::crosapi::mojom;

  EXPECT_EQ(
      Convert(cros_healthd::DiagnosticRoutineUserMessageEnum::kUnplugACPower),
      crosapi::DiagnosticsRoutineUserMessageEnum::kUnplugACPower);
  EXPECT_EQ(
      Convert(cros_healthd::DiagnosticRoutineUserMessageEnum::kPlugInACPower),
      crosapi::DiagnosticsRoutineUserMessageEnum::kPlugInACPower);
}

TEST(DiagnosticsServiceConvertersTest, ConvertDiagnosticRoutineCommandEnum) {
  namespace cros_healthd = ::chromeos::cros_healthd::mojom;
  namespace crosapi = ::crosapi::mojom;

  EXPECT_EQ(Convert(crosapi::DiagnosticsRoutineCommandEnum::kUnknown),
            cros_healthd::DiagnosticRoutineCommandEnum::kUnknown);
  EXPECT_EQ(Convert(crosapi::DiagnosticsRoutineCommandEnum::kContinue),
            cros_healthd::DiagnosticRoutineCommandEnum::kContinue);
  EXPECT_EQ(Convert(crosapi::DiagnosticsRoutineCommandEnum::kCancel),
            cros_healthd::DiagnosticRoutineCommandEnum::kCancel);
  EXPECT_EQ(Convert(crosapi::DiagnosticsRoutineCommandEnum::kGetStatus),
            cros_healthd::DiagnosticRoutineCommandEnum::kGetStatus);
  EXPECT_EQ(Convert(crosapi::DiagnosticsRoutineCommandEnum::kRemove),
            cros_healthd::DiagnosticRoutineCommandEnum::kRemove);
}

TEST(DiagnosticsServiceConvertersTest, ConvertAcPowerStatusEnum) {
  namespace cros_healthd = ::chromeos::cros_healthd::mojom;
  namespace crosapi = ::crosapi::mojom;

  EXPECT_EQ(Convert(crosapi::DiagnosticsAcPowerStatusEnum::kUnknown),
            cros_healthd::AcPowerStatusEnum::kUnknown);
  EXPECT_EQ(Convert(crosapi::DiagnosticsAcPowerStatusEnum::kConnected),
            cros_healthd::AcPowerStatusEnum::kConnected);
  EXPECT_EQ(Convert(crosapi::DiagnosticsAcPowerStatusEnum::kDisconnected),
            cros_healthd::AcPowerStatusEnum::kDisconnected);
}

TEST(DiagnosticsServiceConvertersTest, ConvertNvmeSelfTestTypeEnum) {
  namespace cros_healthd = ::chromeos::cros_healthd::mojom;
  namespace crosapi = ::crosapi::mojom;

  EXPECT_EQ(Convert(crosapi::DiagnosticsNvmeSelfTestTypeEnum::kUnknown),
            cros_healthd::NvmeSelfTestTypeEnum::kUnknown);
  EXPECT_EQ(Convert(crosapi::DiagnosticsNvmeSelfTestTypeEnum::kShortSelfTest),
            cros_healthd::NvmeSelfTestTypeEnum::kShortSelfTest);
  EXPECT_EQ(Convert(crosapi::DiagnosticsNvmeSelfTestTypeEnum::kLongSelfTest),
            cros_healthd::NvmeSelfTestTypeEnum::kLongSelfTest);
}

TEST(DiagnosticsServiceConvertersTest, ConvertDiskReadRoutineTypeEnum) {
  namespace cros_healthd = ::chromeos::cros_healthd::mojom;
  namespace crosapi = ::crosapi::mojom;

  EXPECT_EQ(Convert(crosapi::DiagnosticsDiskReadRoutineTypeEnum::kLinearRead),
            cros_healthd::DiskReadRoutineTypeEnum::kLinearRead);
  EXPECT_EQ(Convert(crosapi::DiagnosticsDiskReadRoutineTypeEnum::kRandomRead),
            cros_healthd::DiskReadRoutineTypeEnum::kRandomRead);
}

}  // namespace converters
}  // namespace ash

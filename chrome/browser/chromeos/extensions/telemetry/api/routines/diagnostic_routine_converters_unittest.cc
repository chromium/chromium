// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_converters.h"

#include <cstdint>

#include "base/uuid.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::converters::routines {

namespace {
namespace cx_diag = api::os_diagnostics;
namespace crosapi = ::crosapi::mojom;
}  // namespace

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest,
     RoutineInitializedInfo) {
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  auto input = crosapi::TelemetryDiagnosticRoutineStateInitialized::New();

  auto result = ConvertPtr(std::move(input), kUuid);

  ASSERT_TRUE(result.uuid.has_value());
  EXPECT_EQ(*result.uuid, kUuid.AsLowercaseString());
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest, RoutineRunningInfo) {
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();
  constexpr uint32_t kPercentage = 50;

  auto input = crosapi::TelemetryDiagnosticRoutineStateRunning::New();

  auto result = ConvertPtr(std::move(input), kUuid, kPercentage);

  ASSERT_TRUE(result.uuid.has_value());
  EXPECT_EQ(*result.uuid, kUuid.AsLowercaseString());

  ASSERT_TRUE(result.percentage.has_value());
  EXPECT_EQ(static_cast<uint32_t>(*result.percentage), kPercentage);
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest, RoutineWaitingInfo) {
  constexpr char kMsg[] = "TEST";
  constexpr uint32_t kPercentage = 50;
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  auto input = crosapi::TelemetryDiagnosticRoutineStateWaiting::New();
  input->reason = crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason::
      kWaitingToBeScheduled;
  input->message = kMsg;

  auto result = ConvertPtr(std::move(input), kUuid, kPercentage);

  ASSERT_TRUE(result.uuid.has_value());
  EXPECT_EQ(*result.uuid, kUuid.AsLowercaseString());
  EXPECT_EQ(result.reason,
            cx_diag::RoutineWaitingReason::kWaitingToBeScheduled);

  ASSERT_TRUE(result.message.has_value());
  EXPECT_EQ(*result.message, kMsg);

  ASSERT_TRUE(result.percentage.has_value());
  EXPECT_EQ(static_cast<uint32_t>(*result.percentage), kPercentage);
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest, RoutineWaitingReason) {
  EXPECT_EQ(Convert(crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason::
                        kUnmappedEnumField),
            cx_diag::RoutineWaitingReason::kNone);

  EXPECT_EQ(Convert(crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason::
                        kWaitingToBeScheduled),
            cx_diag::RoutineWaitingReason::kWaitingToBeScheduled);

  EXPECT_EQ(Convert(crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason::
                        kWaitingUserInput),
            cx_diag::RoutineWaitingReason::kWaitingUserInput);
}

}  // namespace chromeos::converters::routines

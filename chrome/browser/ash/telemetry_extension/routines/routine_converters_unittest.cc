// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/routines/routine_converters.h"

#include <utility>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::converters {

namespace {

namespace crosapi = ::crosapi::mojom;
namespace healthd = cros_healthd::mojom;

}  // namespace

// Tests that `ConvertRoutinePtr` function returns nullptr if input is
// nullptr. `ConvertRoutinePtr` is a template, so we can test this function
// with any valid type.
TEST(TelemetryDiagnosticRoutineConvertersTest, ConvertRoutinePtrTakesNullPtr) {
  EXPECT_TRUE(
      ConvertRoutinePtr(crosapi::TelemetryDiagnosticRoutineArgumentPtr())
          .is_null());
}

TEST(TelemetryDiagnosticRoutineConvertersTest, ConvertRoutineArgumentPtr) {
  auto input =
      crosapi::TelemetryDiagnosticRoutineArgument::NewUnrecognizedArgument(
          true);

  const auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_unrecognizedArgument());
  EXPECT_TRUE(result->get_unrecognizedArgument());
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticRoutineStateInitializedPtr) {
  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineStateInitialized::New()),
            crosapi::TelemetryDiagnosticRoutineStateInitialized::New());
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticRoutineStateRunningPtr) {
  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineStateRunning::New()),
            crosapi::TelemetryDiagnosticRoutineStateRunning::New());
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticRoutineStateWaitingPtr) {
  constexpr char kMessage[] = "TEST";

  auto input = healthd::RoutineStateWaiting::New(
      healthd::RoutineStateWaiting::Reason::kWaitingToBeScheduled, kMessage);

  auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->reason, crosapi::TelemetryDiagnosticRoutineStateWaiting::
                                Reason::kWaitingToBeScheduled);
  EXPECT_EQ(result->message, kMessage);
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticRoutineDetailPtr) {
  EXPECT_EQ(
      ConvertRoutinePtr(healthd::RoutineDetail::NewUnrecognizedArgument(true)),
      crosapi::TelemetryDiagnosticRoutineDetail::NewUnrecognizedArgument(true));
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticRoutineStateFinishedPtr) {
  auto routine_detail = healthd::RoutineDetail::NewUnrecognizedArgument(true);

  auto input = healthd::RoutineStateFinished::New();
  input->has_passed = false;
  input->detail = std::move(routine_detail);

  auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_FALSE(result->has_passed);

  auto detail_result = std::move(result->detail);

  ASSERT_TRUE(detail_result);
  ASSERT_TRUE(detail_result->is_unrecognizedArgument());

  EXPECT_TRUE(detail_result->get_unrecognizedArgument());
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticRoutineStateUnionPtr) {
  EXPECT_EQ(
      ConvertRoutinePtr(
          healthd::RoutineStateUnion::NewUnrecognizedArgument(true)),
      crosapi::TelemetryDiagnosticRoutineStateUnion::NewUnrecognizedArgument(
          true));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineStateUnion::NewInitialized(
                healthd::RoutineStateInitialized::New())),
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewInitialized(
                crosapi::TelemetryDiagnosticRoutineStateInitialized::New()));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineStateUnion::NewRunning(
                healthd::RoutineStateRunning::New())),
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewRunning(
                crosapi::TelemetryDiagnosticRoutineStateRunning::New()));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineStateUnion::NewWaiting(
                healthd::RoutineStateWaiting::New())),
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewWaiting(
                crosapi::TelemetryDiagnosticRoutineStateWaiting::New()));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineStateUnion::NewFinished(
                healthd::RoutineStateFinished::New())),
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
                crosapi::TelemetryDiagnosticRoutineStateFinished::New()));
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticRoutineStatePtr) {
  constexpr uint8_t kPercentage = 50;

  auto input = healthd::RoutineState::New();
  input->percentage = kPercentage;
  input->state_union = healthd::RoutineStateUnion::NewRunning(
      healthd::RoutineStateRunning::New());

  auto result = ConvertRoutinePtr(std::move(input));
  ASSERT_TRUE(result);
  EXPECT_EQ(result->percentage, kPercentage);
  EXPECT_EQ(result->state_union,
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewRunning(
                crosapi::TelemetryDiagnosticRoutineStateRunning::New()));
}

}  // namespace ash::converters

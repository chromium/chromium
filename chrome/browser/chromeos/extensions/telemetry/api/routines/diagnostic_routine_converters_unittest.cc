// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_converters.h"

#include <cstdint>

#include "base/uuid.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
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

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest,
     MemoryRoutineFinishedInfo) {
  constexpr bool kHasPassed = true;
  constexpr uint32_t kBytesTested = 42;
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  auto input = crosapi::TelemetryDiagnosticMemoryRoutineDetail::New();
  input->bytes_tested = kBytesTested;

  auto memtester_result = crosapi::TelemetryDiagnosticMemtesterResult::New();
  memtester_result->passed_items = {
      crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareDIV,
      crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareMUL};
  memtester_result->failed_items = {
      crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareAND,
      crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareSUB};
  input->result = std::move(memtester_result);

  auto result = ConvertPtr(std::move(input), kUuid, kHasPassed);

  ASSERT_TRUE(result.uuid.has_value());
  EXPECT_EQ(*result.uuid, kUuid.AsLowercaseString());

  ASSERT_TRUE(result.has_passed.has_value());
  EXPECT_EQ(*result.has_passed, kHasPassed);

  ASSERT_TRUE(result.result.has_value());
  EXPECT_THAT(
      result.result.value().passed_items,
      testing::ElementsAre(cx_diag::MemtesterTestItemEnum::kCompareDiv,
                           cx_diag::MemtesterTestItemEnum::kCompareMul));
  EXPECT_THAT(
      result.result.value().failed_items,
      testing::ElementsAre(cx_diag::MemtesterTestItemEnum::kCompareAnd,
                           cx_diag::MemtesterTestItemEnum::kCompareSub));
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest, MemtesterResult) {
  auto input = crosapi::TelemetryDiagnosticMemtesterResult::New();
  input->passed_items = {
      crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareDIV,
      crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareMUL};
  input->failed_items = {
      crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareAND,
      crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareSUB};

  auto result = ConvertPtr(std::move(input));

  EXPECT_THAT(
      result.passed_items,
      testing::ElementsAre(cx_diag::MemtesterTestItemEnum::kCompareDiv,
                           cx_diag::MemtesterTestItemEnum::kCompareMul));
  EXPECT_THAT(
      result.failed_items,
      testing::ElementsAre(cx_diag::MemtesterTestItemEnum::kCompareAnd,
                           cx_diag::MemtesterTestItemEnum::kCompareSub));
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest, ExceptionReason) {
  EXPECT_EQ(
      Convert(crosapi::TelemetryExtensionException::Reason::kUnmappedEnumField),
      cx_diag::ExceptionReason::kUnknown);
  EXPECT_EQ(Convert(crosapi::TelemetryExtensionException::Reason::
                        kMojoDisconnectWithoutReason),
            cx_diag::ExceptionReason::kUnknown);
  EXPECT_EQ(Convert(crosapi::TelemetryExtensionException::Reason::kUnexpected),
            cx_diag::ExceptionReason::kUnexpected);
  EXPECT_EQ(Convert(crosapi::TelemetryExtensionException::Reason::kUnsupported),
            cx_diag::ExceptionReason::kUnsupported);
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

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest, MemtesterTestItemEnum) {
  EXPECT_EQ(Convert(crosapi::TelemetryDiagnosticMemtesterTestItemEnum::
                        kUnmappedEnumField),
            cx_diag::MemtesterTestItemEnum::kUnknown);
  EXPECT_EQ(
      Convert(crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kUnknown),
      cx_diag::MemtesterTestItemEnum::kUnknown);
  EXPECT_EQ(
      Convert(crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kStuckAddress),
      cx_diag::MemtesterTestItemEnum::kStuckAddress);
  EXPECT_EQ(
      Convert(crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareAND),
      cx_diag::MemtesterTestItemEnum::kCompareAnd);
  EXPECT_EQ(
      Convert(crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareDIV),
      cx_diag::MemtesterTestItemEnum::kCompareDiv);
  EXPECT_EQ(
      Convert(crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareMUL),
      cx_diag::MemtesterTestItemEnum::kCompareMul);
  EXPECT_EQ(
      Convert(crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareOR),
      cx_diag::MemtesterTestItemEnum::kCompareOr);
  EXPECT_EQ(
      Convert(crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareSUB),
      cx_diag::MemtesterTestItemEnum::kCompareSub);
  EXPECT_EQ(
      Convert(crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareXOR),
      cx_diag::MemtesterTestItemEnum::kCompareXor);
  EXPECT_EQ(Convert(crosapi::TelemetryDiagnosticMemtesterTestItemEnum::
                        kSequentialIncrement),
            cx_diag::MemtesterTestItemEnum::kSequentialIncrement);
  EXPECT_EQ(
      Convert(crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBitFlip),
      cx_diag::MemtesterTestItemEnum::kBitFlip);
  EXPECT_EQ(
      Convert(crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBitSpread),
      cx_diag::MemtesterTestItemEnum::kBitSpread);
  EXPECT_EQ(
      Convert(
          crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBlockSequential),
      cx_diag::MemtesterTestItemEnum::kBlockSequential);
  EXPECT_EQ(
      Convert(crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCheckerboard),
      cx_diag::MemtesterTestItemEnum::kCheckerboard);
  EXPECT_EQ(
      Convert(crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kRandomValue),
      cx_diag::MemtesterTestItemEnum::kRandomValue);
  EXPECT_EQ(
      Convert(crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kSolidBits),
      cx_diag::MemtesterTestItemEnum::kSolidBits);
  EXPECT_EQ(
      Convert(crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kWalkingOnes),
      cx_diag::MemtesterTestItemEnum::kWalkingOnes);
  EXPECT_EQ(
      Convert(
          crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kWalkingZeroes),
      cx_diag::MemtesterTestItemEnum::kWalkingZeroes);
  EXPECT_EQ(
      Convert(
          crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kEightBitWrites),
      cx_diag::MemtesterTestItemEnum::kEightBitWrites);
  EXPECT_EQ(
      Convert(
          crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kSixteenBitWrites),
      cx_diag::MemtesterTestItemEnum::kSixteenBitWrites);
}

}  // namespace chromeos::converters::routines

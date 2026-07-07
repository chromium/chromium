// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_converters.h"

#include <cstdint>

#include "base/uuid.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::converters::routines {

namespace {
namespace cx_diag = api::os_diagnostics;
}  // namespace

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest,
     RoutineInitializedInfo) {
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  auto input = ash::cros_healthd::mojom::RoutineStateInitialized::New();

  auto result = ConvertPtr(std::move(input), kUuid);

  ASSERT_TRUE(result.uuid.has_value());
  EXPECT_EQ(*result.uuid, kUuid.AsLowercaseString());
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest, RoutineRunningInfo) {
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();
  constexpr uint32_t kPercentage = 50;

  auto input = ash::cros_healthd::mojom::RoutineStateRunning::New();

  auto result = ConvertPtr(std::move(input), kUuid, kPercentage);

  ASSERT_TRUE(result.uuid.has_value());
  EXPECT_EQ(*result.uuid, kUuid.AsLowercaseString());

  ASSERT_TRUE(result.percentage.has_value());
  EXPECT_EQ(static_cast<uint32_t>(*result.percentage), kPercentage);

  EXPECT_FALSE(result.info.has_value());
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest,
     NetworkBandwidthRoutineRunningInfo) {
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();
  constexpr uint32_t kPercentage = 50;

  auto running_info =
      ash::cros_healthd::mojom::NetworkBandwidthRoutineRunningInfo::New();
  running_info->type = ash::cros_healthd::mojom::
      NetworkBandwidthRoutineRunningInfo::Type::kDownload;
  running_info->speed_kbps = 100.0;

  auto input = ash::cros_healthd::mojom::RoutineStateRunning::New();
  input->info =
      ash::cros_healthd::mojom::RoutineRunningInfo::NewNetworkBandwidth(
          std::move(running_info));

  auto result = ConvertPtr(std::move(input), kUuid, kPercentage);

  ASSERT_TRUE(result.uuid.has_value());
  EXPECT_EQ(*result.uuid, kUuid.AsLowercaseString());

  ASSERT_TRUE(result.percentage.has_value());
  EXPECT_EQ(static_cast<uint32_t>(*result.percentage), kPercentage);

  ASSERT_TRUE(result.info.has_value());
  ASSERT_TRUE(result.info->network_bandwidth.has_value());

  EXPECT_EQ(result.info->network_bandwidth->type,
            cx_diag::NetworkBandwidthRoutineRunningType::kDownload);
  EXPECT_EQ(result.info->network_bandwidth->speed_kbps, 100.0);
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest,
     RoutineInquiryCheckLedLitUpState) {
  auto result = ConvertPtr(
      ash::cros_healthd::mojom::RoutineInquiry::NewCheckLedLitUpState(
          ash::cros_healthd::mojom::CheckLedLitUpStateInquiry::New()));

  EXPECT_TRUE(result.check_led_lit_up_state.has_value());
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest,
     RoutineInquiryCheckKeyboardBacklightState) {
  auto result = ConvertPtr(
      ash::cros_healthd::mojom::RoutineInquiry::NewCheckKeyboardBacklightState(
          ash::cros_healthd::mojom::CheckKeyboardBacklightStateInquiry::New()));

  EXPECT_TRUE(result.check_keyboard_backlight_state.has_value());
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest, RoutineWaitingInfo) {
  constexpr char kMsg[] = "TEST";
  constexpr uint32_t kPercentage = 50;
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  auto input = ash::cros_healthd::mojom::RoutineStateWaiting::New();
  input->reason = ash::cros_healthd::mojom::RoutineStateWaiting::Reason::
      kWaitingToBeScheduled;
  input->message = kMsg;
  input->interaction = ash::cros_healthd::mojom::RoutineInteraction::NewInquiry(
      ash::cros_healthd::mojom::RoutineInquiry::NewUnrecognizedInquiry(false));

  auto result = ConvertPtr(std::move(input), kUuid, kPercentage);

  ASSERT_TRUE(result.uuid.has_value());
  EXPECT_EQ(*result.uuid, kUuid.AsLowercaseString());
  EXPECT_EQ(result.reason,
            cx_diag::RoutineWaitingReason::kWaitingToBeScheduled);

  ASSERT_TRUE(result.message.has_value());
  EXPECT_EQ(*result.message, kMsg);

  ASSERT_TRUE(result.percentage.has_value());
  EXPECT_EQ(static_cast<uint32_t>(*result.percentage), kPercentage);

  ASSERT_TRUE(result.interaction.has_value());
  EXPECT_TRUE(result.interaction.value().inquiry.has_value());
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest,
     LegacyMemoryRoutineFinishedInfo) {
  constexpr bool kHasPassed = true;
  constexpr uint32_t kBytesTested = 42;
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  auto input = ash::cros_healthd::mojom::MemoryRoutineDetail::New();
  input->bytes_tested = kBytesTested;

  auto memtester_result = ash::cros_healthd::mojom::MemtesterResult::New();
  memtester_result->passed_items = {
      ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareDIV,
      ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareMUL};
  memtester_result->failed_items = {
      ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareAND,
      ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareSUB};
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

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest,
     LegacyFanRoutineFinishedInfo) {
  auto input = ash::cros_healthd::mojom::FanRoutineDetail::New();
  input->passed_fan_ids = {0};
  input->failed_fan_ids = {1};
  input->fan_count_status =
      ash::cros_healthd::mojom::HardwarePresenceStatus::kMatched;

  constexpr bool kHasPassed = true;
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  auto result = ConvertPtr(std::move(input), kUuid, kHasPassed);

  ASSERT_TRUE(result.uuid.has_value());
  EXPECT_EQ(*result.uuid, kUuid.AsLowercaseString());

  ASSERT_TRUE(result.has_passed.has_value());
  EXPECT_EQ(*result.has_passed, kHasPassed);
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest, MemtesterResult) {
  auto input = ash::cros_healthd::mojom::MemtesterResult::New();
  input->passed_items = {
      ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareDIV,
      ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareMUL};
  input->failed_items = {
      ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareAND,
      ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareSUB};

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

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest,
     RoutineFinishedInfoWithoutDetail) {
  constexpr bool kHasPassed = true;
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  auto input = ash::cros_healthd::mojom::RoutineStateFinished::New();
  input->detail = nullptr;

  auto result = ConvertPtr(std::move(input), kUuid, kHasPassed);

  ASSERT_TRUE(result.uuid.has_value());
  EXPECT_EQ(*result.uuid, kUuid.AsLowercaseString());

  ASSERT_TRUE(result.has_passed.has_value());
  EXPECT_EQ(*result.has_passed, kHasPassed);

  EXPECT_FALSE(result.detail.has_value());
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest,
     RoutineFinishedInfoWithUnrecognizedArgument) {
  constexpr bool kHasPassed = true;
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  auto input = ash::cros_healthd::mojom::RoutineStateFinished::New();
  input->detail =
      ash::cros_healthd::mojom::RoutineDetail::NewUnrecognizedArgument(false);

  auto result = ConvertPtr(std::move(input), kUuid, kHasPassed);

  ASSERT_TRUE(result.uuid.has_value());
  EXPECT_EQ(*result.uuid, kUuid.AsLowercaseString());

  ASSERT_TRUE(result.has_passed.has_value());
  EXPECT_EQ(*result.has_passed, kHasPassed);

  EXPECT_FALSE(result.detail.has_value());
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest,
     RoutineFinishedInfoWithMemoryDetail) {
  constexpr bool kHasPassed = true;
  constexpr uint32_t kBytesTested = 42;
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  auto detail = ash::cros_healthd::mojom::MemoryRoutineDetail::New();
  detail->bytes_tested = kBytesTested;

  auto memtester_result = ash::cros_healthd::mojom::MemtesterResult::New();
  memtester_result->passed_items = {
      ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareDIV,
      ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareMUL};
  memtester_result->failed_items = {
      ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareAND,
      ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareSUB};
  detail->result = std::move(memtester_result);

  auto input = ash::cros_healthd::mojom::RoutineStateFinished::New();
  input->detail =
      ash::cros_healthd::mojom::RoutineDetail::NewMemory(std::move(detail));

  auto result = ConvertPtr(std::move(input), kUuid, kHasPassed);

  ASSERT_TRUE(result.uuid.has_value());
  EXPECT_EQ(*result.uuid, kUuid.AsLowercaseString());

  ASSERT_TRUE(result.has_passed.has_value());
  EXPECT_EQ(*result.has_passed, kHasPassed);

  ASSERT_TRUE(result.detail.has_value());
  ASSERT_TRUE(result.detail->memory.has_value());

  ASSERT_TRUE(result.detail->memory->bytes_tested.has_value());
  EXPECT_EQ(*result.detail->memory->bytes_tested, kBytesTested);

  ASSERT_TRUE(result.detail->memory->result.has_value());
  EXPECT_THAT(
      result.detail->memory->result->passed_items,
      testing::ElementsAre(cx_diag::MemtesterTestItemEnum::kCompareDiv,
                           cx_diag::MemtesterTestItemEnum::kCompareMul));
  EXPECT_THAT(
      result.detail->memory->result->failed_items,
      testing::ElementsAre(cx_diag::MemtesterTestItemEnum::kCompareAnd,
                           cx_diag::MemtesterTestItemEnum::kCompareSub));
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest,
     RoutineFinishedInfoWithFanDetail) {
  constexpr bool kHasPassed = true;
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  auto detail = ash::cros_healthd::mojom::FanRoutineDetail::New();
  detail->passed_fan_ids = {0};
  detail->failed_fan_ids = {1};
  detail->fan_count_status =
      ash::cros_healthd::mojom::HardwarePresenceStatus::kMatched;

  auto input = ash::cros_healthd::mojom::RoutineStateFinished::New();
  input->detail =
      ash::cros_healthd::mojom::RoutineDetail::NewFan(std::move(detail));

  auto result = ConvertPtr(std::move(input), kUuid, kHasPassed);

  ASSERT_TRUE(result.uuid.has_value());
  EXPECT_EQ(*result.uuid, kUuid.AsLowercaseString());

  ASSERT_TRUE(result.has_passed.has_value());
  EXPECT_EQ(*result.has_passed, kHasPassed);

  ASSERT_TRUE(result.detail.has_value());
  ASSERT_TRUE(result.detail->fan.has_value());

  ASSERT_TRUE(result.detail->fan->passed_fan_ids.has_value());
  EXPECT_THAT(*result.detail->fan->passed_fan_ids, testing::ElementsAre(0));

  ASSERT_TRUE(result.detail->fan->failed_fan_ids.has_value());
  EXPECT_THAT(*result.detail->fan->failed_fan_ids, testing::ElementsAre(1));

  EXPECT_THAT(result.detail->fan->fan_count_status,
              cx_diag::HardwarePresenceStatus::kMatched);
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest,
     RoutineFinishedInfoWithNetworkBandwidthDetail) {
  constexpr bool kHasPassed = true;
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  auto detail = ash::cros_healthd::mojom::NetworkBandwidthRoutineDetail::New();
  detail->download_speed_kbps = 123.0;
  detail->upload_speed_kbps = 456.0;

  auto input = ash::cros_healthd::mojom::RoutineStateFinished::New();
  input->detail = ash::cros_healthd::mojom::RoutineDetail::NewNetworkBandwidth(
      std::move(detail));

  auto result = ConvertPtr(std::move(input), kUuid, kHasPassed);

  ASSERT_TRUE(result.uuid.has_value());
  EXPECT_EQ(*result.uuid, kUuid.AsLowercaseString());

  ASSERT_TRUE(result.has_passed.has_value());
  EXPECT_EQ(*result.has_passed, kHasPassed);

  ASSERT_TRUE(result.detail.has_value());
  ASSERT_TRUE(result.detail->network_bandwidth.has_value());

  EXPECT_EQ(result.detail->network_bandwidth->download_speed_kbps, 123.0);
  EXPECT_EQ(result.detail->network_bandwidth->upload_speed_kbps, 456.0);
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest,
     RoutineFinishedInfoWithCameraFrameAnalysisDetail) {
  constexpr bool kHasPassed = true;
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  auto detail =
      ash::cros_healthd::mojom::CameraFrameAnalysisRoutineDetail::New();
  detail->issue =
      ash::cros_healthd::mojom::CameraFrameAnalysisRoutineDetail::Issue::kNone;
  detail->privacy_shutter_open_test =
      ash::cros_healthd::mojom::CameraSubtestResult::kPassed;
  detail->lens_not_dirty_test =
      ash::cros_healthd::mojom::CameraSubtestResult::kFailed;

  auto input = ash::cros_healthd::mojom::RoutineStateFinished::New();
  input->detail =
      ash::cros_healthd::mojom::RoutineDetail::NewCameraFrameAnalysis(
          std::move(detail));

  auto result = ConvertPtr(std::move(input), kUuid, kHasPassed);

  ASSERT_TRUE(result.uuid.has_value());
  EXPECT_EQ(*result.uuid, kUuid.AsLowercaseString());

  ASSERT_TRUE(result.has_passed.has_value());
  EXPECT_EQ(*result.has_passed, kHasPassed);

  ASSERT_TRUE(result.detail.has_value());
  ASSERT_TRUE(result.detail->camera_frame_analysis.has_value());

  EXPECT_EQ(result.detail->camera_frame_analysis->issue,
            cx_diag::CameraFrameAnalysisIssue::kNoIssue);
  EXPECT_EQ(result.detail->camera_frame_analysis->privacy_shutter_open_test,
            cx_diag::CameraSubtestResult::kPassed);
  EXPECT_EQ(result.detail->camera_frame_analysis->lens_not_dirty_test,
            cx_diag::CameraSubtestResult::kFailed);
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest, ExceptionReason) {
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::Exception::Reason::kUnmappedEnumField),
      cx_diag::ExceptionReason::kUnknown);
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::Exception::Reason::
                        kMojoDisconnectWithoutReason),
            cx_diag::ExceptionReason::kUnknown);
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::Exception::Reason::kUnexpected),
            cx_diag::ExceptionReason::kUnexpected);
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::Exception::Reason::kUnsupported),
            cx_diag::ExceptionReason::kUnsupported);
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::Exception::Reason::
                        kCameraFrontendNotOpened),
            cx_diag::ExceptionReason::kCameraFrontendNotOpened);
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest, RoutineWaitingReason) {
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::RoutineStateWaiting::Reason::
                        kUnmappedEnumField),
            cx_diag::RoutineWaitingReason::kNone);

  EXPECT_EQ(Convert(ash::cros_healthd::mojom::RoutineStateWaiting::Reason::
                        kWaitingToBeScheduled),
            cx_diag::RoutineWaitingReason::kWaitingToBeScheduled);

  EXPECT_EQ(Convert(ash::cros_healthd::mojom::RoutineStateWaiting::Reason::
                        kWaitingInteraction),
            cx_diag::RoutineWaitingReason::kWaitingForInteraction);
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest, MemtesterTestItemEnum) {
  EXPECT_EQ(
      Convert(
          ash::cros_healthd::mojom::MemtesterTestItemEnum::kUnmappedEnumField),
      cx_diag::MemtesterTestItemEnum::kUnknown);
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::kUnknown),
            cx_diag::MemtesterTestItemEnum::kUnknown);
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::kStuckAddress),
      cx_diag::MemtesterTestItemEnum::kStuckAddress);
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareAND),
      cx_diag::MemtesterTestItemEnum::kCompareAnd);
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareDIV),
      cx_diag::MemtesterTestItemEnum::kCompareDiv);
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareMUL),
      cx_diag::MemtesterTestItemEnum::kCompareMul);
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareOR),
      cx_diag::MemtesterTestItemEnum::kCompareOr);
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareSUB),
      cx_diag::MemtesterTestItemEnum::kCompareSub);
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareXOR),
      cx_diag::MemtesterTestItemEnum::kCompareXor);
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::
                        kSequentialIncrement),
            cx_diag::MemtesterTestItemEnum::kSequentialIncrement);
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::kBitFlip),
            cx_diag::MemtesterTestItemEnum::kBitFlip);
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::kBitSpread),
      cx_diag::MemtesterTestItemEnum::kBitSpread);
  EXPECT_EQ(
      Convert(
          ash::cros_healthd::mojom::MemtesterTestItemEnum::kBlockSequential),
      cx_diag::MemtesterTestItemEnum::kBlockSequential);
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::kCheckerboard),
      cx_diag::MemtesterTestItemEnum::kCheckerboard);
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::kRandomValue),
      cx_diag::MemtesterTestItemEnum::kRandomValue);
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::kSolidBits),
      cx_diag::MemtesterTestItemEnum::kSolidBits);
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::kWalkingOnes),
      cx_diag::MemtesterTestItemEnum::kWalkingOnes);
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::kWalkingZeroes),
      cx_diag::MemtesterTestItemEnum::kWalkingZeroes);
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::k8BitWrites),
      cx_diag::MemtesterTestItemEnum::kEightBitWrites);
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::MemtesterTestItemEnum::k16BitWrites),
      cx_diag::MemtesterTestItemEnum::kSixteenBitWrites);
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest,
     CameraFrameAnalysisRoutineDetailIssue) {
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::CameraFrameAnalysisRoutineDetail::
                        Issue::kUnmappedEnumField),
            cx_diag::CameraFrameAnalysisIssue::kNone);
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::CameraFrameAnalysisRoutineDetail::
                        Issue::kNone),
            cx_diag::CameraFrameAnalysisIssue::kNoIssue);
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::CameraFrameAnalysisRoutineDetail::
                        Issue::kCameraServiceNotAvailable),
            cx_diag::CameraFrameAnalysisIssue::kCameraServiceNotAvailable);
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::CameraFrameAnalysisRoutineDetail::
                        Issue::kBlockedByPrivacyShutter),
            cx_diag::CameraFrameAnalysisIssue::kBlockedByPrivacyShutter);
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::CameraFrameAnalysisRoutineDetail::
                        Issue::kLensAreDirty),
            cx_diag::CameraFrameAnalysisIssue::kLensAreDirty);
}

TEST(TelemetryExtensionDiagnosticRoutineConvertersTest, CameraSubtestResult) {
  EXPECT_EQ(
      Convert(
          ash::cros_healthd::mojom::CameraSubtestResult::kUnmappedEnumField),
      cx_diag::CameraSubtestResult::kNone);
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::CameraSubtestResult::kNotRun),
            cx_diag::CameraSubtestResult::kNotRun);
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::CameraSubtestResult::kPassed),
            cx_diag::CameraSubtestResult::kPassed);
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::CameraSubtestResult::kFailed),
            cx_diag::CameraSubtestResult::kFailed);
}

}  // namespace chromeos::converters::routines

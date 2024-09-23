// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_converters.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "base/notreached.h"
#include "base/uuid.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"

namespace chromeos::converters::routines {

namespace {
namespace cx_diag = api::os_diagnostics;
namespace crosapi = ::crosapi::mojom;

std::optional<cx_diag::RoutineFinishedDetailUnion> ConvertRoutineDetailUnionPtr(
    crosapi::TelemetryDiagnosticRoutineDetailPtr input) {
  if (input.is_null()) {
    return std::nullopt;
  }
  cx_diag::RoutineFinishedDetailUnion detail;
  switch (input->which()) {
    case crosapi::TelemetryDiagnosticRoutineDetail::Tag::kUnrecognizedArgument:
      LOG(WARNING) << "Got unknown routine detail";
      return std::nullopt;
    case crosapi::TelemetryDiagnosticRoutineDetail::Tag::kMemory:
      detail.memory = ConvertPtr(std::move(input->get_memory()));
      return detail;
    case crosapi::TelemetryDiagnosticRoutineDetail::Tag::kVolumeButton:
      // This member type in the union is kept only for backward compatibility.
      // There is no such a field in the web IDL definition.
      return std::nullopt;
    case crosapi::TelemetryDiagnosticRoutineDetail::Tag::kFan:
      detail.fan = ConvertPtr(std::move(input->get_fan()));
      return detail;
    case crosapi::TelemetryDiagnosticRoutineDetail::Tag::kNetworkBandwidth:
      detail.network_bandwidth =
          ConvertPtr(std::move(input->get_network_bandwidth()));
      return detail;
    case crosapi::TelemetryDiagnosticRoutineDetail::Tag::kCameraFrameAnalysis:
      detail.camera_frame_analysis =
          ConvertPtr(std::move(input->get_camera_frame_analysis()));
      return detail;
  }
}

std::optional<cx_diag::RoutineRunningInfoUnion>
ConvertRoutineRunningInfoUnionPtr(
    crosapi::TelemetryDiagnosticRoutineRunningInfoPtr input) {
  if (input.is_null()) {
    return std::nullopt;
  }
  cx_diag::RoutineRunningInfoUnion info;
  switch (input->which()) {
    case crosapi::TelemetryDiagnosticRoutineRunningInfo::Tag::
        kUnrecognizedArgument:
      LOG(WARNING) << "Got unknown routine running info";
      return std::nullopt;
    case crosapi::TelemetryDiagnosticRoutineRunningInfo::Tag::kNetworkBandwidth:
      info.network_bandwidth =
          ConvertPtr(std::move(input->get_network_bandwidth()));
      return info;
  }
}

}  // namespace

namespace unchecked {

cx_diag::RoutineInitializedInfo UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticRoutineStateInitializedPtr input,
    base::Uuid uuid) {
  cx_diag::RoutineInitializedInfo result;
  result.uuid = uuid.AsLowercaseString();
  return result;
}

cx_diag::RoutineRunningInfo UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticRoutineStateRunningPtr input,
    base::Uuid uuid,
    uint32_t percentage) {
  cx_diag::RoutineRunningInfo result;
  result.uuid = uuid.AsLowercaseString();
  result.percentage = percentage;
  result.info = ConvertRoutineRunningInfoUnionPtr(std::move(input->info));
  return result;
}

cx_diag::NetworkBandwidthRoutineRunningInfo UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticNetworkBandwidthRoutineRunningInfoPtr input) {
  cx_diag::NetworkBandwidthRoutineRunningInfo info;
  info.type = Convert(input->type);
  info.speed_kbps = input->speed_kbps;
  return info;
}

cx_diag::RoutineInquiryUnion UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticRoutineInquiryPtr input) {
  cx_diag::RoutineInquiryUnion inquiry;
  switch (input->which()) {
    case crosapi::TelemetryDiagnosticRoutineInquiry::Tag::kUnrecognizedInquiry:
      // This indicates version skew on Mojo interfaces, which is unexpected.
      // Return an empty union as a safeguard.
      break;
    case crosapi::TelemetryDiagnosticRoutineInquiry::Tag::kCheckLedLitUpState:
      inquiry.check_led_lit_up_state = cx_diag::CheckLedLitUpStateInquiry();
      break;
    case crosapi::TelemetryDiagnosticRoutineInquiry::Tag::
        kCheckKeyboardBacklightState:
      inquiry.check_keyboard_backlight_state =
          cx_diag::CheckKeyboardBacklightStateInquiry();
      break;
  }
  return inquiry;
}

cx_diag::RoutineInteractionUnion UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticRoutineInteractionPtr input) {
  cx_diag::RoutineInteractionUnion interaction;
  switch (input->which()) {
    case crosapi::TelemetryDiagnosticRoutineInteraction::Tag::
        kUnrecognizedInteraction:
      // This indicates version skew on Mojo interfaces, which is unexpected.
      // Return an empty union as a safeguard.
      break;
    case crosapi::TelemetryDiagnosticRoutineInteraction::Tag::kInquiry:
      interaction.inquiry = ConvertPtr(std::move(input->get_inquiry()));
      break;
  }
  return interaction;
}

cx_diag::RoutineWaitingInfo UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticRoutineStateWaitingPtr input,
    base::Uuid uuid,
    uint32_t percentage) {
  cx_diag::RoutineWaitingInfo result;
  result.uuid = uuid.AsLowercaseString();
  result.reason = Convert(input->reason);
  if (input->interaction) {
    result.interaction = ConvertPtr(std::move(input->interaction));
  }
  result.message = input->message;
  result.percentage = percentage;
  return result;
}

cx_diag::MemtesterResult UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticMemtesterResultPtr input) {
  cx_diag::MemtesterResult result;
  result.passed_items = ConvertVector(input->passed_items);
  result.failed_items = ConvertVector(input->failed_items);

  return result;
}

cx_diag::LegacyMemoryRoutineFinishedInfo UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticMemoryRoutineDetailPtr input,
    base::Uuid uuid,
    bool has_passed) {
  cx_diag::LegacyMemoryRoutineFinishedInfo result;
  result.uuid = uuid.AsLowercaseString();
  result.has_passed = has_passed;
  // Construct the non-legacy detail to ensure the content is the same between
  // the legacy and the non-legacy ones.
  cx_diag::MemoryRoutineFinishedDetail detail =
      UncheckedConvertPtr(std::move(input));
  result.bytes_tested = std::move(detail.bytes_tested);
  if (detail.result) {
    result.result = cx_diag::LegacyMemtesterResult();
    result.result->passed_items = std::move(detail.result->passed_items);
    result.result->failed_items = std::move(detail.result->failed_items);
  }
  return result;
}

cx_diag::LegacyVolumeButtonRoutineFinishedInfo UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticVolumeButtonRoutineDetailPtr input,
    base::Uuid uuid,
    bool has_passed) {
  cx_diag::LegacyVolumeButtonRoutineFinishedInfo result;
  result.uuid = uuid.AsLowercaseString();
  result.has_passed = has_passed;
  return result;
}

cx_diag::LegacyFanRoutineFinishedInfo UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticFanRoutineDetailPtr input,
    base::Uuid uuid,
    bool has_passed) {
  cx_diag::LegacyFanRoutineFinishedInfo result;
  result.uuid = uuid.AsLowercaseString();
  result.has_passed = has_passed;
  // Construct the non-legacy detail to ensure the content is the same between
  // the legacy and the non-legacy ones.
  cx_diag::FanRoutineFinishedDetail detail =
      UncheckedConvertPtr(std::move(input));
  result.passed_fan_ids = std::move(detail.passed_fan_ids);
  result.failed_fan_ids = std::move(detail.failed_fan_ids);
  result.fan_count_status = std::move(detail.fan_count_status);
  return result;
}

cx_diag::MemoryRoutineFinishedDetail UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticMemoryRoutineDetailPtr input) {
  cx_diag::MemoryRoutineFinishedDetail result;
  result.bytes_tested = input->bytes_tested;
  result.result = ConvertPtr(std::move(input->result));
  return result;
}

cx_diag::FanRoutineFinishedDetail UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticFanRoutineDetailPtr input) {
  cx_diag::FanRoutineFinishedDetail result;

  std::vector<int> passed_fan_ids = {};
  for (const auto& passed_fan_id : input->passed_fan_ids) {
    passed_fan_ids.push_back(passed_fan_id);
  }
  result.passed_fan_ids = passed_fan_ids;

  std::vector<int> failed_fan_ids = {};
  for (const auto& failed_fan_id : input->failed_fan_ids) {
    failed_fan_ids.push_back(failed_fan_id);
  }
  result.failed_fan_ids = failed_fan_ids;

  result.fan_count_status = Convert(input->fan_count_status);
  return result;
}

cx_diag::NetworkBandwidthRoutineFinishedDetail UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticNetworkBandwidthRoutineDetailPtr input) {
  cx_diag::NetworkBandwidthRoutineFinishedDetail result;
  result.download_speed_kbps = input->download_speed_kbps;
  result.upload_speed_kbps = input->upload_speed_kbps;
  return result;
}

cx_diag::CameraFrameAnalysisRoutineFinishedDetail UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetailPtr input) {
  cx_diag::CameraFrameAnalysisRoutineFinishedDetail result;
  result.issue = Convert(input->issue);
  result.privacy_shutter_open_test = Convert(input->privacy_shutter_open_test);
  result.lens_not_dirty_test = Convert(input->lens_not_dirty_test);
  return result;
}

cx_diag::RoutineFinishedInfo UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticRoutineStateFinishedPtr input,
    base::Uuid uuid,
    bool has_passed) {
  cx_diag::RoutineFinishedInfo result;
  result.uuid = uuid.AsLowercaseString();
  result.has_passed = has_passed;
  result.detail = ConvertRoutineDetailUnionPtr(std::move(input->detail));
  return result;
}

}  // namespace unchecked

cx_diag::ExceptionReason Convert(
    crosapi::TelemetryExtensionException::Reason input) {
  switch (input) {
    case crosapi::TelemetryExtensionException::Reason::kUnmappedEnumField:
      return cx_diag::ExceptionReason::kUnknown;
    case crosapi::TelemetryExtensionException::Reason::
        kMojoDisconnectWithoutReason:
      return cx_diag::ExceptionReason::kUnknown;
    case crosapi::TelemetryExtensionException::Reason::kUnexpected:
      return cx_diag::ExceptionReason::kUnexpected;
    case crosapi::TelemetryExtensionException::Reason::kUnsupported:
      return cx_diag::ExceptionReason::kUnsupported;
    case crosapi::TelemetryExtensionException::Reason::kCameraFrontendNotOpened:
      return cx_diag::ExceptionReason::kCameraFrontendNotOpened;
  }
  NOTREACHED_IN_MIGRATION();
}

cx_diag::RoutineWaitingReason Convert(
    crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason input) {
  switch (input) {
    case crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason::
        kUnmappedEnumField:
      return cx_diag::RoutineWaitingReason::kNone;
    case crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason::
        kWaitingToBeScheduled:
      return cx_diag::RoutineWaitingReason::kWaitingToBeScheduled;
    case crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason::
        kWaitingForInteraction:
      return cx_diag::RoutineWaitingReason::kWaitingForInteraction;
  }
  NOTREACHED();
}

cx_diag::MemtesterTestItemEnum Convert(
    crosapi::TelemetryDiagnosticMemtesterTestItemEnum input) {
  switch (input) {
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kUnmappedEnumField:
      return cx_diag::MemtesterTestItemEnum::kUnknown;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kUnknown:
      return cx_diag::MemtesterTestItemEnum::kUnknown;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kStuckAddress:
      return cx_diag::MemtesterTestItemEnum::kStuckAddress;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareAND:
      return cx_diag::MemtesterTestItemEnum::kCompareAnd;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareDIV:
      return cx_diag::MemtesterTestItemEnum::kCompareDiv;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareMUL:
      return cx_diag::MemtesterTestItemEnum::kCompareMul;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareOR:
      return cx_diag::MemtesterTestItemEnum::kCompareOr;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareSUB:
      return cx_diag::MemtesterTestItemEnum::kCompareSub;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareXOR:
      return cx_diag::MemtesterTestItemEnum::kCompareXor;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::
        kSequentialIncrement:
      return cx_diag::MemtesterTestItemEnum::kSequentialIncrement;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBitFlip:
      return cx_diag::MemtesterTestItemEnum::kBitFlip;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBitSpread:
      return cx_diag::MemtesterTestItemEnum::kBitSpread;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBlockSequential:
      return cx_diag::MemtesterTestItemEnum::kBlockSequential;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCheckerboard:
      return cx_diag::MemtesterTestItemEnum::kCheckerboard;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kRandomValue:
      return cx_diag::MemtesterTestItemEnum::kRandomValue;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kSolidBits:
      return cx_diag::MemtesterTestItemEnum::kSolidBits;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kWalkingOnes:
      return cx_diag::MemtesterTestItemEnum::kWalkingOnes;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kWalkingZeroes:
      return cx_diag::MemtesterTestItemEnum::kWalkingZeroes;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kEightBitWrites:
      return cx_diag::MemtesterTestItemEnum::kEightBitWrites;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kSixteenBitWrites:
      return cx_diag::MemtesterTestItemEnum::kSixteenBitWrites;
  }
  NOTREACHED();
}

cx_diag::HardwarePresenceStatus Convert(
    crosapi::TelemetryDiagnosticHardwarePresenceStatus input) {
  switch (input) {
    case crosapi::TelemetryDiagnosticHardwarePresenceStatus::kUnmappedEnumField:
      return cx_diag::HardwarePresenceStatus::kNone;
    case crosapi::TelemetryDiagnosticHardwarePresenceStatus::kMatched:
      return cx_diag::HardwarePresenceStatus::kMatched;
    case crosapi::TelemetryDiagnosticHardwarePresenceStatus::kNotMatched:
      return cx_diag::HardwarePresenceStatus::kNotMatched;
    case crosapi::TelemetryDiagnosticHardwarePresenceStatus::kNotConfigured:
      return cx_diag::HardwarePresenceStatus::kNotConfigured;
  }
  NOTREACHED();
}

cx_diag::NetworkBandwidthRoutineRunningType Convert(
    crosapi::TelemetryDiagnosticNetworkBandwidthRoutineRunningInfo::Type
        input) {
  switch (input) {
    case crosapi::TelemetryDiagnosticNetworkBandwidthRoutineRunningInfo::Type::
        kUnmappedEnumField:
      return cx_diag::NetworkBandwidthRoutineRunningType::kNone;
    case crosapi::TelemetryDiagnosticNetworkBandwidthRoutineRunningInfo::Type::
        kDownload:
      return cx_diag::NetworkBandwidthRoutineRunningType::kDownload;
    case crosapi::TelemetryDiagnosticNetworkBandwidthRoutineRunningInfo::Type::
        kUpload:
      return cx_diag::NetworkBandwidthRoutineRunningType::kUpload;
  }
  NOTREACHED();
}

cx_diag::CameraFrameAnalysisIssue Convert(
    crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::Issue input) {
  switch (input) {
    case crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::Issue::
        kUnmappedEnumField:
      return cx_diag::CameraFrameAnalysisIssue::kNone;
    case crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::Issue::
        kNone:
      return cx_diag::CameraFrameAnalysisIssue::kNoIssue;
    case crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::Issue::
        kCameraServiceNotAvailable:
      return cx_diag::CameraFrameAnalysisIssue::kCameraServiceNotAvailable;
    case crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::Issue::
        kBlockedByPrivacyShutter:
      return cx_diag::CameraFrameAnalysisIssue::kBlockedByPrivacyShutter;
    case crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::Issue::
        kLensAreDirty:
      return cx_diag::CameraFrameAnalysisIssue::kLensAreDirty;
  }
  NOTREACHED();
}

cx_diag::CameraSubtestResult Convert(
    crosapi::TelemetryDiagnosticCameraSubtestResult input) {
  switch (input) {
    case crosapi::TelemetryDiagnosticCameraSubtestResult::kUnmappedEnumField:
      return cx_diag::CameraSubtestResult::kNone;
    case crosapi::TelemetryDiagnosticCameraSubtestResult::kNotRun:
      return cx_diag::CameraSubtestResult::kNotRun;
    case crosapi::TelemetryDiagnosticCameraSubtestResult::kPassed:
      return cx_diag::CameraSubtestResult::kPassed;
    case crosapi::TelemetryDiagnosticCameraSubtestResult::kFailed:
      return cx_diag::CameraSubtestResult::kFailed;
  }
  NOTREACHED();
}

}  // namespace chromeos::converters::routines

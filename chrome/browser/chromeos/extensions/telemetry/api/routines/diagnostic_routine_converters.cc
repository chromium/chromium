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

namespace chromeos::converters::routines {

namespace {
namespace cx_diag = api::os_diagnostics;

std::optional<cx_diag::RoutineFinishedDetailUnion> ConvertRoutineDetailUnionPtr(
    ash::cros_healthd::mojom::RoutineDetailPtr input) {
  if (input.is_null()) {
    return std::nullopt;
  }
  cx_diag::RoutineFinishedDetailUnion detail;
  switch (auto tag = input->which(); tag) {
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kUnrecognizedArgument:
      LOG(WARNING) << "Got unknown routine detail";
      return std::nullopt;
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kMemory:
      detail.memory = ConvertPtr(std::move(input->get_memory()));
      return detail;
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kFan:
      detail.fan = ConvertPtr(std::move(input->get_fan()));
      return detail;
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kNetworkBandwidth:
      detail.network_bandwidth =
          ConvertPtr(std::move(input->get_network_bandwidth()));
      return detail;
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kCameraFrameAnalysis:
      detail.camera_frame_analysis =
          ConvertPtr(std::move(input->get_camera_frame_analysis()));
      return detail;

    // Unsupported enums
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kAudioDriver:
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kUfsLifetime:
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kBluetoothPower:
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kBluetoothDiscovery:
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kBluetoothScanning:
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kBluetoothPairing:
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kCameraAvailability:
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kSensitiveSensor:
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kBatteryDischarge:
      LOG(WARNING) << "Got unknown routine detail: " << static_cast<int>(tag);
      return std::nullopt;
  }
}

std::optional<cx_diag::RoutineRunningInfoUnion>
ConvertRoutineRunningInfoUnionPtr(
    ash::cros_healthd::mojom::RoutineRunningInfoPtr input) {
  if (input.is_null()) {
    return std::nullopt;
  }
  cx_diag::RoutineRunningInfoUnion info;
  switch (input->which()) {
    case ash::cros_healthd::mojom::RoutineRunningInfo::Tag::
        kUnrecognizedArgument:
      LOG(WARNING) << "Got unknown routine running info";
      return std::nullopt;
    case ash::cros_healthd::mojom::RoutineRunningInfo::Tag::kNetworkBandwidth:
      info.network_bandwidth =
          ConvertPtr(std::move(input->get_network_bandwidth()));
      return info;
  }
}

}  // namespace

namespace unchecked {

cx_diag::RoutineInitializedInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::RoutineStateInitializedPtr input,
    base::Uuid uuid) {
  cx_diag::RoutineInitializedInfo result;
  result.uuid = uuid.AsLowercaseString();
  return result;
}

cx_diag::RoutineRunningInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::RoutineStateRunningPtr input,
    base::Uuid uuid,
    uint32_t percentage) {
  cx_diag::RoutineRunningInfo result;
  result.uuid = uuid.AsLowercaseString();
  result.percentage = percentage;
  result.info = ConvertRoutineRunningInfoUnionPtr(std::move(input->info));
  return result;
}

cx_diag::NetworkBandwidthRoutineRunningInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::NetworkBandwidthRoutineRunningInfoPtr input) {
  cx_diag::NetworkBandwidthRoutineRunningInfo info;
  info.type = Convert(input->type);
  info.speed_kbps = input->speed_kbps;
  return info;
}

cx_diag::RoutineInquiryUnion UncheckedConvertPtr(
    ash::cros_healthd::mojom::RoutineInquiryPtr input) {
  cx_diag::RoutineInquiryUnion inquiry;
  switch (input->which()) {
    case ash::cros_healthd::mojom::RoutineInquiry::Tag::kUnrecognizedInquiry:
      // This indicates version skew on Mojo interfaces, which is unexpected.
      // Return an empty union as a safeguard.
      break;
    case ash::cros_healthd::mojom::RoutineInquiry::Tag::kCheckLedLitUpState:
      inquiry.check_led_lit_up_state = cx_diag::CheckLedLitUpStateInquiry();
      break;
    case ash::cros_healthd::mojom::RoutineInquiry::Tag::
        kCheckKeyboardBacklightState:
      inquiry.check_keyboard_backlight_state =
          cx_diag::CheckKeyboardBacklightStateInquiry();
      break;

    // Unsupported.
    case ash::cros_healthd::mojom::RoutineInquiry::Tag::kUnplugAcAdapterInquiry:
      break;
  }
  return inquiry;
}

cx_diag::RoutineInteractionUnion UncheckedConvertPtr(
    ash::cros_healthd::mojom::RoutineInteractionPtr input) {
  cx_diag::RoutineInteractionUnion interaction;
  switch (input->which()) {
    case ash::cros_healthd::mojom::RoutineInteraction::Tag::
        kUnrecognizedInteraction:
      // This indicates version skew on Mojo interfaces, which is unexpected.
      // Return an empty union as a safeguard.
      break;
    case ash::cros_healthd::mojom::RoutineInteraction::Tag::kInquiry:
      interaction.inquiry = ConvertPtr(std::move(input->get_inquiry()));
      break;
  }
  return interaction;
}

cx_diag::RoutineWaitingInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::RoutineStateWaitingPtr input,
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
    ash::cros_healthd::mojom::MemtesterResultPtr input) {
  cx_diag::MemtesterResult result;
  result.passed_items = ConvertVector(input->passed_items);
  result.failed_items = ConvertVector(input->failed_items);

  return result;
}

cx_diag::LegacyMemoryRoutineFinishedInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::MemoryRoutineDetailPtr input,
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

cx_diag::LegacyFanRoutineFinishedInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::FanRoutineDetailPtr input,
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
    ash::cros_healthd::mojom::MemoryRoutineDetailPtr input) {
  cx_diag::MemoryRoutineFinishedDetail result;
  result.bytes_tested = input->bytes_tested;
  result.result = ConvertPtr(std::move(input->result));
  return result;
}

cx_diag::FanRoutineFinishedDetail UncheckedConvertPtr(
    ash::cros_healthd::mojom::FanRoutineDetailPtr input) {
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
    ash::cros_healthd::mojom::NetworkBandwidthRoutineDetailPtr input) {
  cx_diag::NetworkBandwidthRoutineFinishedDetail result;
  result.download_speed_kbps = input->download_speed_kbps;
  result.upload_speed_kbps = input->upload_speed_kbps;
  return result;
}

cx_diag::CameraFrameAnalysisRoutineFinishedDetail UncheckedConvertPtr(
    ash::cros_healthd::mojom::CameraFrameAnalysisRoutineDetailPtr input) {
  cx_diag::CameraFrameAnalysisRoutineFinishedDetail result;
  result.issue = Convert(input->issue);
  result.privacy_shutter_open_test = Convert(input->privacy_shutter_open_test);
  result.lens_not_dirty_test = Convert(input->lens_not_dirty_test);
  return result;
}

cx_diag::RoutineFinishedInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::RoutineStateFinishedPtr input,
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
    ash::cros_healthd::mojom::Exception::Reason input) {
  switch (input) {
    case ash::cros_healthd::mojom::Exception::Reason::kUnmappedEnumField:
      return cx_diag::ExceptionReason::kUnknown;
    case ash::cros_healthd::mojom::Exception::Reason::
        kMojoDisconnectWithoutReason:
      return cx_diag::ExceptionReason::kUnknown;
    case ash::cros_healthd::mojom::Exception::Reason::kUnexpected:
      return cx_diag::ExceptionReason::kUnexpected;
    case ash::cros_healthd::mojom::Exception::Reason::kUnsupported:
      return cx_diag::ExceptionReason::kUnsupported;
    case ash::cros_healthd::mojom::Exception::Reason::kCameraFrontendNotOpened:
      return cx_diag::ExceptionReason::kCameraFrontendNotOpened;
  }
  NOTREACHED();
}

cx_diag::RoutineWaitingReason Convert(
    ash::cros_healthd::mojom::RoutineStateWaiting::Reason input) {
  switch (input) {
    case ash::cros_healthd::mojom::RoutineStateWaiting::Reason::
        kUnmappedEnumField:
      return cx_diag::RoutineWaitingReason::kNone;
    case ash::cros_healthd::mojom::RoutineStateWaiting::Reason::
        kWaitingToBeScheduled:
      return cx_diag::RoutineWaitingReason::kWaitingToBeScheduled;
    case ash::cros_healthd::mojom::RoutineStateWaiting::Reason::
        kWaitingInteraction:
      return cx_diag::RoutineWaitingReason::kWaitingForInteraction;
  }
  NOTREACHED();
}

cx_diag::MemtesterTestItemEnum Convert(
    ash::cros_healthd::mojom::MemtesterTestItemEnum input) {
  switch (input) {
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kUnmappedEnumField:
      return cx_diag::MemtesterTestItemEnum::kUnknown;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kUnknown:
      return cx_diag::MemtesterTestItemEnum::kUnknown;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kStuckAddress:
      return cx_diag::MemtesterTestItemEnum::kStuckAddress;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareAND:
      return cx_diag::MemtesterTestItemEnum::kCompareAnd;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareDIV:
      return cx_diag::MemtesterTestItemEnum::kCompareDiv;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareMUL:
      return cx_diag::MemtesterTestItemEnum::kCompareMul;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareOR:
      return cx_diag::MemtesterTestItemEnum::kCompareOr;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareSUB:
      return cx_diag::MemtesterTestItemEnum::kCompareSub;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareXOR:
      return cx_diag::MemtesterTestItemEnum::kCompareXor;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kSequentialIncrement:
      return cx_diag::MemtesterTestItemEnum::kSequentialIncrement;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kBitFlip:
      return cx_diag::MemtesterTestItemEnum::kBitFlip;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kBitSpread:
      return cx_diag::MemtesterTestItemEnum::kBitSpread;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kBlockSequential:
      return cx_diag::MemtesterTestItemEnum::kBlockSequential;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kCheckerboard:
      return cx_diag::MemtesterTestItemEnum::kCheckerboard;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kRandomValue:
      return cx_diag::MemtesterTestItemEnum::kRandomValue;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kSolidBits:
      return cx_diag::MemtesterTestItemEnum::kSolidBits;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kWalkingOnes:
      return cx_diag::MemtesterTestItemEnum::kWalkingOnes;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::kWalkingZeroes:
      return cx_diag::MemtesterTestItemEnum::kWalkingZeroes;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::k8BitWrites:
      return cx_diag::MemtesterTestItemEnum::kEightBitWrites;
    case ash::cros_healthd::mojom::MemtesterTestItemEnum::k16BitWrites:
      return cx_diag::MemtesterTestItemEnum::kSixteenBitWrites;
  }
  NOTREACHED();
}

cx_diag::HardwarePresenceStatus Convert(
    ash::cros_healthd::mojom::HardwarePresenceStatus input) {
  switch (input) {
    case ash::cros_healthd::mojom::HardwarePresenceStatus::kUnmappedEnumField:
      return cx_diag::HardwarePresenceStatus::kNone;
    case ash::cros_healthd::mojom::HardwarePresenceStatus::kMatched:
      return cx_diag::HardwarePresenceStatus::kMatched;
    case ash::cros_healthd::mojom::HardwarePresenceStatus::kNotMatched:
      return cx_diag::HardwarePresenceStatus::kNotMatched;
    case ash::cros_healthd::mojom::HardwarePresenceStatus::kNotConfigured:
      return cx_diag::HardwarePresenceStatus::kNotConfigured;
  }
  NOTREACHED();
}

cx_diag::NetworkBandwidthRoutineRunningType Convert(
    ash::cros_healthd::mojom::NetworkBandwidthRoutineRunningInfo::Type input) {
  switch (input) {
    case ash::cros_healthd::mojom::NetworkBandwidthRoutineRunningInfo::Type::
        kUnmappedEnumField:
      return cx_diag::NetworkBandwidthRoutineRunningType::kNone;
    case ash::cros_healthd::mojom::NetworkBandwidthRoutineRunningInfo::Type::
        kDownload:
      return cx_diag::NetworkBandwidthRoutineRunningType::kDownload;
    case ash::cros_healthd::mojom::NetworkBandwidthRoutineRunningInfo::Type::
        kUpload:
      return cx_diag::NetworkBandwidthRoutineRunningType::kUpload;
  }
  NOTREACHED();
}

cx_diag::CameraFrameAnalysisIssue Convert(
    ash::cros_healthd::mojom::CameraFrameAnalysisRoutineDetail::Issue input) {
  switch (input) {
    case ash::cros_healthd::mojom::CameraFrameAnalysisRoutineDetail::Issue::
        kUnmappedEnumField:
      return cx_diag::CameraFrameAnalysisIssue::kNone;
    case ash::cros_healthd::mojom::CameraFrameAnalysisRoutineDetail::Issue::
        kNone:
      return cx_diag::CameraFrameAnalysisIssue::kNoIssue;
    case ash::cros_healthd::mojom::CameraFrameAnalysisRoutineDetail::Issue::
        kCameraServiceNotAvailable:
      return cx_diag::CameraFrameAnalysisIssue::kCameraServiceNotAvailable;
    case ash::cros_healthd::mojom::CameraFrameAnalysisRoutineDetail::Issue::
        kBlockedByPrivacyShutter:
      return cx_diag::CameraFrameAnalysisIssue::kBlockedByPrivacyShutter;
    case ash::cros_healthd::mojom::CameraFrameAnalysisRoutineDetail::Issue::
        kLensAreDirty:
      return cx_diag::CameraFrameAnalysisIssue::kLensAreDirty;
  }
  NOTREACHED();
}

cx_diag::CameraSubtestResult Convert(
    ash::cros_healthd::mojom::CameraSubtestResult input) {
  switch (input) {
    case ash::cros_healthd::mojom::CameraSubtestResult::kUnmappedEnumField:
      return cx_diag::CameraSubtestResult::kNone;
    case ash::cros_healthd::mojom::CameraSubtestResult::kNotRun:
      return cx_diag::CameraSubtestResult::kNotRun;
    case ash::cros_healthd::mojom::CameraSubtestResult::kPassed:
      return cx_diag::CameraSubtestResult::kPassed;
    case ash::cros_healthd::mojom::CameraSubtestResult::kFailed:
      return cx_diag::CameraSubtestResult::kFailed;
  }
  NOTREACHED();
}

}  // namespace chromeos::converters::routines

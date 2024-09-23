// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/diagnostics_api_converters.h"

#include <optional>

#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"

namespace chromeos::converters::diagnostics {

namespace {

namespace cx_diag = ::chromeos::api::os_diagnostics;
namespace crosapi = ::crosapi::mojom;

// All fields of `cx_diag::CreateRoutineArgumentsUnion`. The enums are defined
// manually because there are no tools to generate them automatically.
enum class CreateRoutineArgumentsField {
  kMemory,
  kVolumeButton,
  kFan,
  kNetworkBandwidth,
  kLedLitUp,
  kCameraFrameAnalysis,
  kKeyboardBacklight,
};

// All fields of `cx_diag::RoutineInquiryReplyUnion`. The enums are defined
// manually because there are no tools to generate them automatically.
enum class RoutineInquiryReplyField {
  kCheckLedLitUpState,
  kCheckKeyboardBacklightState,
};

std::optional<crosapi::TelemetryDiagnosticRoutineArgumentPtr>
ConvertExtensionUnionToMojoUnion(
    const cx_diag::CreateMemoryRoutineArguments& cx_args) {
  if (cx_args.max_testing_mem_kib.has_value() &&
      cx_args.max_testing_mem_kib.value() < 0) {
    return std::nullopt;
  }

  auto args = crosapi::TelemetryDiagnosticMemoryRoutineArgument::New();
  args->max_testing_mem_kib = cx_args.max_testing_mem_kib;
  return crosapi::TelemetryDiagnosticRoutineArgument::NewMemory(
      std::move(args));
}

std::optional<crosapi::TelemetryDiagnosticRoutineArgumentPtr>
ConvertExtensionUnionToMojoUnion(
    const cx_diag::CreateVolumeButtonRoutineArguments& cx_args) {
  if (cx_args.timeout_seconds <= 0 ||
      cx_args.button_type == cx_diag::VolumeButtonType::kNone) {
    return std::nullopt;
  }

  auto args = crosapi::TelemetryDiagnosticVolumeButtonRoutineArgument::New();
  args->type = ConvertVolumeButtonRoutineButtonType(cx_args.button_type);
  args->timeout = base::Seconds(cx_args.timeout_seconds);
  return crosapi::TelemetryDiagnosticRoutineArgument::NewVolumeButton(
      std::move(args));
}

std::optional<crosapi::TelemetryDiagnosticRoutineArgumentPtr>
ConvertExtensionUnionToMojoUnion(
    const cx_diag::CreateFanRoutineArguments& cx_args) {
  return crosapi::TelemetryDiagnosticRoutineArgument::NewFan(
      crosapi::TelemetryDiagnosticFanRoutineArgument::New());
}

std::optional<crosapi::TelemetryDiagnosticRoutineArgumentPtr>
ConvertExtensionUnionToMojoUnion(
    const cx_diag::CreateNetworkBandwidthRoutineArguments& cx_args) {
  return crosapi::TelemetryDiagnosticRoutineArgument::NewNetworkBandwidth(
      crosapi::TelemetryDiagnosticNetworkBandwidthRoutineArgument::New());
}

std::optional<crosapi::TelemetryDiagnosticRoutineArgumentPtr>
ConvertExtensionUnionToMojoUnion(
    const cx_diag::CreateLedLitUpRoutineArguments& cx_args) {
  auto args = crosapi::TelemetryDiagnosticLedLitUpRoutineArgument::New();
  args->name = ConvertLedName(cx_args.name);
  args->color = ConvertLedColor(cx_args.color);
  return crosapi::TelemetryDiagnosticRoutineArgument::NewLedLitUp(
      std::move(args));
}

std::optional<crosapi::TelemetryDiagnosticRoutineArgumentPtr>
ConvertExtensionUnionToMojoUnion(
    const cx_diag::CreateCameraFrameAnalysisRoutineArguments& cx_args) {
  return crosapi::TelemetryDiagnosticRoutineArgument::NewCameraFrameAnalysis(
      crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineArgument::New());
}

std::optional<crosapi::TelemetryDiagnosticRoutineArgumentPtr>
ConvertExtensionUnionToMojoUnion(
    const cx_diag::CreateKeyboardBacklightRoutineArguments& cx_args) {
  return crosapi::TelemetryDiagnosticRoutineArgument::NewKeyboardBacklight(
      crosapi::TelemetryDiagnosticKeyboardBacklightRoutineArgument::New());
}

std::optional<crosapi::TelemetryDiagnosticRoutineInquiryReplyPtr>
ConvertExtensionUnionToMojoUnion(
    const cx_diag::CheckLedLitUpStateReply& cx_args) {
  auto args = crosapi::TelemetryDiagnosticCheckLedLitUpStateReply::New();
  args->state = ConvertLedLitUpState(cx_args.state);
  return crosapi::TelemetryDiagnosticRoutineInquiryReply::NewCheckLedLitUpState(
      std::move(args));
}

std::optional<crosapi::TelemetryDiagnosticRoutineInquiryReplyPtr>
ConvertExtensionUnionToMojoUnion(
    const cx_diag::CheckKeyboardBacklightStateReply& cx_args) {
  auto args =
      crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReply::New();
  args->state = ConvertKeyboardBacklightState(cx_args.state);
  return crosapi::TelemetryDiagnosticRoutineInquiryReply::
      NewCheckKeyboardBacklightState(std::move(args));
}

// Default implementation of `ConvertExtensionUnionToMojoUnion` raises compile
// error.
template <typename Arg, typename OutputT>
OutputT ConvertExtensionUnionToMojoUnion(const Arg& arg) {
  static_assert(
      false, "ConvertExtensionUnionToMojoUnion for specific type not defined.");
  NOTREACHED();
}

std::vector<RoutineInquiryReplyField> GetNonNullFields(
    const cx_diag::RoutineInquiryReplyUnion& extension_union) {
  std::vector<RoutineInquiryReplyField> result;
  if (extension_union.check_led_lit_up_state.has_value()) {
    result.push_back(RoutineInquiryReplyField::kCheckLedLitUpState);
  }
  if (extension_union.check_keyboard_backlight_state.has_value()) {
    result.push_back(RoutineInquiryReplyField::kCheckKeyboardBacklightState);
  }
  return result;
}

std::vector<CreateRoutineArgumentsField> GetNonNullFields(
    const cx_diag::CreateRoutineArgumentsUnion& extension_union) {
  std::vector<CreateRoutineArgumentsField> result;
  if (extension_union.memory.has_value()) {
    result.push_back(CreateRoutineArgumentsField::kMemory);
  }
  if (extension_union.volume_button.has_value()) {
    result.push_back(CreateRoutineArgumentsField::kVolumeButton);
  }
  if (extension_union.fan.has_value()) {
    result.push_back(CreateRoutineArgumentsField::kFan);
  }
  if (extension_union.network_bandwidth.has_value()) {
    result.push_back(CreateRoutineArgumentsField::kNetworkBandwidth);
  }
  if (extension_union.led_lit_up.has_value()) {
    result.push_back(CreateRoutineArgumentsField::kLedLitUp);
  }
  if (extension_union.camera_frame_analysis.has_value()) {
    result.push_back(CreateRoutineArgumentsField::kCameraFrameAnalysis);
  }
  if (extension_union.keyboard_backlight.has_value()) {
    result.push_back(CreateRoutineArgumentsField::kKeyboardBacklight);
  }
  return result;
}

}  // namespace

bool ConvertMojoRoutine(crosapi::DiagnosticsRoutineEnum in,
                        cx_diag::RoutineType* out) {
  DCHECK(out);
  switch (in) {
    case crosapi::DiagnosticsRoutineEnum::kAcPower:
      *out = cx_diag::RoutineType::kAcPower;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kBatteryCapacity:
      *out = cx_diag::RoutineType::kBatteryCapacity;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kBatteryCharge:
      *out = cx_diag::RoutineType::kBatteryCharge;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kBatteryDischarge:
      *out = cx_diag::RoutineType::kBatteryDischarge;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kBatteryHealth:
      *out = cx_diag::RoutineType::kBatteryHealth;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kCpuCache:
      *out = cx_diag::RoutineType::kCpuCache;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kFloatingPointAccuracy:
      *out = cx_diag::RoutineType::kCpuFloatingPointAccuracy;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kPrimeSearch:
      *out = cx_diag::RoutineType::kCpuPrimeSearch;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kCpuStress:
      *out = cx_diag::RoutineType::kCpuStress;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kDiskRead:
      *out = cx_diag::RoutineType::kDiskRead;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kDnsResolution:
      *out = cx_diag::RoutineType::kDnsResolution;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kDnsResolverPresent:
      *out = cx_diag::RoutineType::kDnsResolverPresent;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kLanConnectivity:
      *out = cx_diag::RoutineType::kLanConnectivity;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kMemory:
      *out = cx_diag::RoutineType::kMemory;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kSignalStrength:
      *out = cx_diag::RoutineType::kSignalStrength;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kGatewayCanBePinged:
      *out = cx_diag::RoutineType::kGatewayCanBePinged;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kSmartctlCheck:
      *out = cx_diag::RoutineType::kSmartctlCheck;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kSensitiveSensor:
      *out = cx_diag::RoutineType::kSensitiveSensor;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kNvmeSelfTest:
      *out = cx_diag::RoutineType::kNvmeSelfTest;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kFingerprintAlive:
      *out = cx_diag::RoutineType::kFingerprintAlive;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kSmartctlCheckWithPercentageUsed:
      *out = cx_diag::RoutineType::kSmartctlCheckWithPercentageUsed;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kEmmcLifetime:
      *out = cx_diag::RoutineType::kEmmcLifetime;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kBluetoothPower:
      *out = cx_diag::RoutineType::kBluetoothPower;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kUfsLifetime:
      *out = cx_diag::RoutineType::kUfsLifetime;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kPowerButton:
      *out = cx_diag::RoutineType::kPowerButton;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kAudioDriver:
      *out = cx_diag::RoutineType::kAudioDriver;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kBluetoothDiscovery:
      *out = cx_diag::RoutineType::kBluetoothDiscovery;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kBluetoothScanning:
      *out = cx_diag::RoutineType::kBluetoothScanning;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kBluetoothPairing:
      *out = cx_diag::RoutineType::kBluetoothPairing;
      return true;
    case crosapi::DiagnosticsRoutineEnum::kFan:
      *out = cx_diag::RoutineType::kFan;
      return true;
    // Below are deprecated routines.
    case crosapi::DiagnosticsRoutineEnum::DEPRECATED_kNvmeWearLevel:
    case crosapi::DiagnosticsRoutineEnum::kUnknown:
      return false;
  }
}

cx_diag::RoutineStatus ConvertRoutineStatus(
    crosapi::DiagnosticsRoutineStatusEnum status) {
  switch (status) {
    case crosapi::DiagnosticsRoutineStatusEnum::kUnknown:
      return cx_diag::RoutineStatus::kUnknown;
    case crosapi::DiagnosticsRoutineStatusEnum::kReady:
      return cx_diag::RoutineStatus::kReady;
    case crosapi::DiagnosticsRoutineStatusEnum::kRunning:
      return cx_diag::RoutineStatus::kRunning;
    case crosapi::DiagnosticsRoutineStatusEnum::kWaiting:
      return cx_diag::RoutineStatus::kWaitingUserAction;
    case crosapi::DiagnosticsRoutineStatusEnum::kPassed:
      return cx_diag::RoutineStatus::kPassed;
    case crosapi::DiagnosticsRoutineStatusEnum::kFailed:
      return cx_diag::RoutineStatus::kFailed;
    case crosapi::DiagnosticsRoutineStatusEnum::kError:
      return cx_diag::RoutineStatus::kError;
    case crosapi::DiagnosticsRoutineStatusEnum::kCancelled:
      return cx_diag::RoutineStatus::kCancelled;
    case crosapi::DiagnosticsRoutineStatusEnum::kFailedToStart:
      return cx_diag::RoutineStatus::kFailedToStart;
    case crosapi::DiagnosticsRoutineStatusEnum::kRemoved:
      return cx_diag::RoutineStatus::kRemoved;
    case crosapi::DiagnosticsRoutineStatusEnum::kCancelling:
      return cx_diag::RoutineStatus::kCancelling;
    case crosapi::DiagnosticsRoutineStatusEnum::kUnsupported:
      return cx_diag::RoutineStatus::kUnsupported;
    case crosapi::DiagnosticsRoutineStatusEnum::kNotRun:
      return cx_diag::RoutineStatus::kNotRun;
  }
}

crosapi::DiagnosticsRoutineCommandEnum ConvertRoutineCommand(
    cx_diag::RoutineCommandType commandType) {
  switch (commandType) {
    case cx_diag::RoutineCommandType::kCancel:
      return crosapi::DiagnosticsRoutineCommandEnum::kCancel;
    case cx_diag::RoutineCommandType::kRemove:
      return crosapi::DiagnosticsRoutineCommandEnum::kRemove;
    case cx_diag::RoutineCommandType::kResume:
      return crosapi::DiagnosticsRoutineCommandEnum::kContinue;
    case cx_diag::RoutineCommandType::kStatus:
      return crosapi::DiagnosticsRoutineCommandEnum::kGetStatus;
    case cx_diag::RoutineCommandType::kNone:
      break;
  }
  NOTREACHED();
}

crosapi::DiagnosticsAcPowerStatusEnum ConvertAcPowerStatusRoutineType(
    cx_diag::AcPowerStatus routineType) {
  switch (routineType) {
    case cx_diag::AcPowerStatus::kConnected:
      return crosapi::DiagnosticsAcPowerStatusEnum::kConnected;
    case cx_diag::AcPowerStatus::kDisconnected:
      return crosapi::DiagnosticsAcPowerStatusEnum::kDisconnected;
    case cx_diag::AcPowerStatus::kNone:
      break;
  }
  NOTREACHED();
}

cx_diag::UserMessageType ConvertRoutineUserMessage(
    crosapi::DiagnosticsRoutineUserMessageEnum userMessage) {
  switch (userMessage) {
    case crosapi::DiagnosticsRoutineUserMessageEnum::kUnknown:
      return cx_diag::UserMessageType::kUnknown;
    case crosapi::DiagnosticsRoutineUserMessageEnum::kUnplugACPower:
      return cx_diag::UserMessageType::kUnplugAcPower;
    case crosapi::DiagnosticsRoutineUserMessageEnum::kPlugInACPower:
      return cx_diag::UserMessageType::kPlugInAcPower;
    case crosapi::DiagnosticsRoutineUserMessageEnum::kPressPowerButton:
      return cx_diag::UserMessageType::kPressPowerButton;
  }
}

crosapi::DiagnosticsDiskReadRoutineTypeEnum ConvertDiskReadRoutineType(
    cx_diag::DiskReadRoutineType routineType) {
  switch (routineType) {
    case cx_diag::DiskReadRoutineType::kLinear:
      return crosapi::DiagnosticsDiskReadRoutineTypeEnum::kLinearRead;
    case cx_diag::DiskReadRoutineType::kRandom:
      return crosapi::DiagnosticsDiskReadRoutineTypeEnum::kRandomRead;
    case cx_diag::DiskReadRoutineType::kNone:
      break;
  }
  NOTREACHED();
}

crosapi::DiagnosticsNvmeSelfTestTypeEnum ConvertNvmeSelfTestRoutineType(
    cx_diag::RunNvmeSelfTestRequest routine_type) {
  switch (routine_type.test_type) {
    case cx_diag::NvmeSelfTestType::kNone:
      return crosapi::DiagnosticsNvmeSelfTestTypeEnum::kUnknown;
    case cx_diag::NvmeSelfTestType::kShortTest:
      return crosapi::DiagnosticsNvmeSelfTestTypeEnum::kShortSelfTest;
    case cx_diag::NvmeSelfTestType::kLongTest:
      return crosapi::DiagnosticsNvmeSelfTestTypeEnum::kLongSelfTest;
  }
}

crosapi::TelemetryDiagnosticVolumeButtonRoutineArgument::ButtonType
ConvertVolumeButtonRoutineButtonType(
    cx_diag::VolumeButtonType volume_button_type) {
  switch (volume_button_type) {
    case cx_diag::VolumeButtonType::kNone:
      return crosapi::TelemetryDiagnosticVolumeButtonRoutineArgument::
          ButtonType::kUnmappedEnumField;
    case cx_diag::VolumeButtonType::kVolumeUp:
      return crosapi::TelemetryDiagnosticVolumeButtonRoutineArgument::
          ButtonType::kVolumeUp;
    case cx_diag::VolumeButtonType::kVolumeDown:
      return crosapi::TelemetryDiagnosticVolumeButtonRoutineArgument::
          ButtonType::kVolumeDown;
  }
}

crosapi::TelemetryDiagnosticLedName ConvertLedName(cx_diag::LedName led_name) {
  switch (led_name) {
    case cx_diag::LedName::kNone:
      return crosapi::TelemetryDiagnosticLedName::kUnmappedEnumField;
    case cx_diag::LedName::kBattery:
      return crosapi::TelemetryDiagnosticLedName::kBattery;
    case cx_diag::LedName::kPower:
      return crosapi::TelemetryDiagnosticLedName::kPower;
    case cx_diag::LedName::kAdapter:
      return crosapi::TelemetryDiagnosticLedName::kAdapter;
    case cx_diag::LedName::kLeft:
      return crosapi::TelemetryDiagnosticLedName::kLeft;
    case cx_diag::LedName::kRight:
      return crosapi::TelemetryDiagnosticLedName::kRight;
  }
}

crosapi::TelemetryDiagnosticLedColor ConvertLedColor(
    cx_diag::LedColor led_color) {
  switch (led_color) {
    case cx_diag::LedColor::kNone:
      return crosapi::TelemetryDiagnosticLedColor::kUnmappedEnumField;
    case cx_diag::LedColor::kRed:
      return crosapi::TelemetryDiagnosticLedColor::kRed;
    case cx_diag::LedColor::kGreen:
      return crosapi::TelemetryDiagnosticLedColor::kGreen;
    case cx_diag::LedColor::kBlue:
      return crosapi::TelemetryDiagnosticLedColor::kBlue;
    case cx_diag::LedColor::kYellow:
      return crosapi::TelemetryDiagnosticLedColor::kYellow;
    case cx_diag::LedColor::kWhite:
      return crosapi::TelemetryDiagnosticLedColor::kWhite;
    case cx_diag::LedColor::kAmber:
      return crosapi::TelemetryDiagnosticLedColor::kAmber;
  }
}

crosapi::TelemetryDiagnosticCheckLedLitUpStateReply::State ConvertLedLitUpState(
    cx_diag::LedLitUpState led_lit_up_state) {
  switch (led_lit_up_state) {
    case cx_diag::LedLitUpState::kNone:
      return crosapi::TelemetryDiagnosticCheckLedLitUpStateReply::State::
          kUnmappedEnumField;
    case cx_diag::LedLitUpState::kCorrectColor:
      return crosapi::TelemetryDiagnosticCheckLedLitUpStateReply::State::
          kCorrectColor;
    case cx_diag::LedLitUpState::kNotLitUp:
      return crosapi::TelemetryDiagnosticCheckLedLitUpStateReply::State::
          kNotLitUp;
  }
}

crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReply::State
ConvertKeyboardBacklightState(
    cx_diag::KeyboardBacklightState keyboard_backlight_state) {
  switch (keyboard_backlight_state) {
    case cx_diag::KeyboardBacklightState::kNone:
      return crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReply::
          State::kUnmappedEnumField;
    case cx_diag::KeyboardBacklightState::kOk:
      return crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReply::
          State::kOk;
    case cx_diag::KeyboardBacklightState::kAnyNotLitUp:
      return crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReply::
          State::kAnyNotLitUp;
  }
}

std::optional<crosapi::TelemetryDiagnosticRoutineArgumentPtr>
ConvertRoutineArgumentsUnion(
    cx_diag::CreateRoutineArgumentsUnion extension_union) {
  std::vector<CreateRoutineArgumentsField> non_null_fields =
      GetNonNullFields(extension_union);

  if (non_null_fields.empty()) {
    // When extension is newer than the browser, extension might pass in a
    // routine argument that cannot be recognized by the browser. For better
    // developer experience, don't treat it as an invalid union.
    return crosapi::TelemetryDiagnosticRoutineArgument::NewUnrecognizedArgument(
        false);
  }

  // A dictionary-based union is invalid when more than one fields are set.
  if (non_null_fields.size() > 1) {
    return std::nullopt;
  }

  CHECK(non_null_fields.size() == 1);
  switch (non_null_fields.front()) {
    case CreateRoutineArgumentsField::kMemory:
      return ConvertExtensionUnionToMojoUnion(extension_union.memory.value());
    case CreateRoutineArgumentsField::kVolumeButton:
      return ConvertExtensionUnionToMojoUnion(
          extension_union.volume_button.value());
    case CreateRoutineArgumentsField::kFan:
      return ConvertExtensionUnionToMojoUnion(extension_union.fan.value());
    case CreateRoutineArgumentsField::kNetworkBandwidth:
      return ConvertExtensionUnionToMojoUnion(
          extension_union.network_bandwidth.value());
    case CreateRoutineArgumentsField::kLedLitUp:
      return ConvertExtensionUnionToMojoUnion(
          extension_union.led_lit_up.value());
    case CreateRoutineArgumentsField::kCameraFrameAnalysis:
      return ConvertExtensionUnionToMojoUnion(
          extension_union.camera_frame_analysis.value());
    case CreateRoutineArgumentsField::kKeyboardBacklight:
      return ConvertExtensionUnionToMojoUnion(
          extension_union.keyboard_backlight.value());
  }
}

std::optional<crosapi::TelemetryDiagnosticRoutineInquiryReplyPtr>
ConvertRoutineInquiryReplyUnion(
    cx_diag::RoutineInquiryReplyUnion extension_union) {
  std::vector<RoutineInquiryReplyField> non_null_fields =
      GetNonNullFields(extension_union);

  if (non_null_fields.empty()) {
    // When extension is newer than the browser, extension might pass in a reply
    // that cannot be recognized by the browser. For better developer
    // experience, don't treat it as an invalid union.
    return crosapi::TelemetryDiagnosticRoutineInquiryReply::
        NewUnrecognizedReply(false);
  }

  // A dictionary-based union is invalid when more than one fields are set.
  if (non_null_fields.size() > 1) {
    return std::nullopt;
  }

  CHECK(non_null_fields.size() == 1);
  switch (non_null_fields.front()) {
    case RoutineInquiryReplyField::kCheckLedLitUpState:
      return ConvertExtensionUnionToMojoUnion(
          extension_union.check_led_lit_up_state.value());
    case RoutineInquiryReplyField::kCheckKeyboardBacklightState:
      return ConvertExtensionUnionToMojoUnion(
          extension_union.check_keyboard_backlight_state.value());
  }
}

}  // namespace chromeos::converters::diagnostics

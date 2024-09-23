// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/diagnostics_api_converters.h"

#include "base/time/time.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::converters::diagnostics {

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
  {
    cx_diag::RoutineType out = cx_diag::RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kPowerButton, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kPowerButton);
  }
  {
    cx_diag::RoutineType out = cx_diag::RoutineType::kNone;
    EXPECT_TRUE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::kAudioDriver, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kAudioDriver);
  }
  {
    cx_diag::RoutineType out = cx_diag::RoutineType::kNone;
    EXPECT_TRUE(
        ConvertMojoRoutine(crosapi::DiagnosticsRoutineEnum::kFan, &out));
    EXPECT_EQ(out, cx_diag::RoutineType::kFan);
  }
  {
    cx_diag::RoutineType out = cx_diag::RoutineType::kNone;
    EXPECT_FALSE(ConvertMojoRoutine(
        crosapi::DiagnosticsRoutineEnum::DEPRECATED_kNvmeWearLevel, &out));
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
  EXPECT_EQ(ConvertRoutineUserMessage(
                crosapi::DiagnosticsRoutineUserMessageEnum::kPressPowerButton),
            cx_diag::UserMessageType::kPressPowerButton);
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

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertVolumeButtonRoutineButtonType) {
  EXPECT_EQ(
      ConvertVolumeButtonRoutineButtonType(cx_diag::VolumeButtonType::kNone),
      crosapi::TelemetryDiagnosticVolumeButtonRoutineArgument::ButtonType::
          kUnmappedEnumField);

  EXPECT_EQ(ConvertVolumeButtonRoutineButtonType(
                cx_diag::VolumeButtonType::kVolumeUp),
            crosapi::TelemetryDiagnosticVolumeButtonRoutineArgument::
                ButtonType::kVolumeUp);

  EXPECT_EQ(ConvertVolumeButtonRoutineButtonType(
                cx_diag::VolumeButtonType::kVolumeDown),
            crosapi::TelemetryDiagnosticVolumeButtonRoutineArgument::
                ButtonType::kVolumeDown);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest, ConvertLedName) {
  EXPECT_EQ(ConvertLedName(cx_diag::LedName::kNone),
            crosapi::TelemetryDiagnosticLedName::kUnmappedEnumField);
  EXPECT_EQ(ConvertLedName(cx_diag::LedName::kBattery),
            crosapi::TelemetryDiagnosticLedName::kBattery);
  EXPECT_EQ(ConvertLedName(cx_diag::LedName::kPower),
            crosapi::TelemetryDiagnosticLedName::kPower);
  EXPECT_EQ(ConvertLedName(cx_diag::LedName::kAdapter),
            crosapi::TelemetryDiagnosticLedName::kAdapter);
  EXPECT_EQ(ConvertLedName(cx_diag::LedName::kLeft),
            crosapi::TelemetryDiagnosticLedName::kLeft);
  EXPECT_EQ(ConvertLedName(cx_diag::LedName::kRight),
            crosapi::TelemetryDiagnosticLedName::kRight);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest, ConvertLedColor) {
  EXPECT_EQ(ConvertLedColor(cx_diag::LedColor::kNone),
            crosapi::TelemetryDiagnosticLedColor::kUnmappedEnumField);
  EXPECT_EQ(ConvertLedColor(cx_diag::LedColor::kRed),
            crosapi::TelemetryDiagnosticLedColor::kRed);
  EXPECT_EQ(ConvertLedColor(cx_diag::LedColor::kGreen),
            crosapi::TelemetryDiagnosticLedColor::kGreen);
  EXPECT_EQ(ConvertLedColor(cx_diag::LedColor::kBlue),
            crosapi::TelemetryDiagnosticLedColor::kBlue);
  EXPECT_EQ(ConvertLedColor(cx_diag::LedColor::kYellow),
            crosapi::TelemetryDiagnosticLedColor::kYellow);
  EXPECT_EQ(ConvertLedColor(cx_diag::LedColor::kWhite),
            crosapi::TelemetryDiagnosticLedColor::kWhite);
  EXPECT_EQ(ConvertLedColor(cx_diag::LedColor::kAmber),
            crosapi::TelemetryDiagnosticLedColor::kAmber);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest, ConvertLedLitUpState) {
  EXPECT_EQ(ConvertLedLitUpState(cx_diag::LedLitUpState::kNone),
            crosapi::TelemetryDiagnosticCheckLedLitUpStateReply::State::
                kUnmappedEnumField);
  EXPECT_EQ(ConvertLedLitUpState(cx_diag::LedLitUpState::kCorrectColor),
            crosapi::TelemetryDiagnosticCheckLedLitUpStateReply::State::
                kCorrectColor);
  EXPECT_EQ(
      ConvertLedLitUpState(cx_diag::LedLitUpState::kNotLitUp),
      crosapi::TelemetryDiagnosticCheckLedLitUpStateReply::State::kNotLitUp);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertKeyboardBacklightState) {
  EXPECT_EQ(
      ConvertKeyboardBacklightState(cx_diag::KeyboardBacklightState::kNone),
      crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReply::State::
          kUnmappedEnumField);
  EXPECT_EQ(
      ConvertKeyboardBacklightState(cx_diag::KeyboardBacklightState::kOk),
      crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReply::State::kOk);
  EXPECT_EQ(ConvertKeyboardBacklightState(
                cx_diag::KeyboardBacklightState::kAnyNotLitUp),
            crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReply::
                State::kAnyNotLitUp);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineArgumentsUnionErrorWithMultipleNonnullFields) {
  auto args_union = cx_diag::CreateRoutineArgumentsUnion();
  args_union.memory = cx_diag::CreateMemoryRoutineArguments();
  args_union.fan = cx_diag::CreateFanRoutineArguments();
  auto result = ConvertRoutineArgumentsUnion(std::move(args_union));
  EXPECT_FALSE(result.has_value());
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineArgumentsUnionSuccessWithAllFieldsAreNull) {
  auto result =
      ConvertRoutineArgumentsUnion(cx_diag::CreateRoutineArgumentsUnion());
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result.value().is_null());
  EXPECT_TRUE(result.value()->is_unrecognizedArgument());
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineArgumentsUnionSuccessWithMemoryArgs) {
  auto args = cx_diag::CreateMemoryRoutineArguments();
  args.max_testing_mem_kib = 42;
  auto args_union = cx_diag::CreateRoutineArgumentsUnion();
  args_union.memory = std::move(args);
  auto result = ConvertRoutineArgumentsUnion(std::move(args_union));
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result.value().is_null());
  ASSERT_TRUE(result.value()->is_memory());
  EXPECT_EQ(result.value()->get_memory()->max_testing_mem_kib, 42);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineArgumentsUnionErrorWithInvalidMemoryArgs) {
  auto args = cx_diag::CreateMemoryRoutineArguments();
  args.max_testing_mem_kib = -1;
  auto args_union = cx_diag::CreateRoutineArgumentsUnion();
  args_union.memory = std::move(args);
  auto result = ConvertRoutineArgumentsUnion(std::move(args_union));
  EXPECT_FALSE(result.has_value());
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineArgumentsUnionSuccessWithVolumeButtonArgs) {
  auto args = cx_diag::CreateVolumeButtonRoutineArguments();
  args.timeout_seconds = 42;
  args.button_type = cx_diag::VolumeButtonType::kVolumeUp;
  auto args_union = cx_diag::CreateRoutineArgumentsUnion();
  args_union.volume_button = std::move(args);
  auto result = ConvertRoutineArgumentsUnion(std::move(args_union));
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result.value().is_null());
  ASSERT_TRUE(result.value()->is_volume_button());
  EXPECT_EQ(result.value()->get_volume_button()->type,
            crosapi::TelemetryDiagnosticVolumeButtonRoutineArgument::
                ButtonType::kVolumeUp);
  EXPECT_EQ(result.value()->get_volume_button()->timeout, base::Seconds(42));
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineArgumentsUnionErrorWithInvalidVolumeButtonArgs) {
  auto args = cx_diag::CreateVolumeButtonRoutineArguments();
  args.timeout_seconds = -1;
  args.button_type = cx_diag::VolumeButtonType::kNone;
  auto args_union = cx_diag::CreateRoutineArgumentsUnion();
  args_union.volume_button = std::move(args);
  auto result = ConvertRoutineArgumentsUnion(std::move(args_union));
  EXPECT_FALSE(result.has_value());
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineArgumentsUnionSuccessWithFanArgs) {
  auto args_union = cx_diag::CreateRoutineArgumentsUnion();
  args_union.fan = cx_diag::CreateFanRoutineArguments();
  auto result = ConvertRoutineArgumentsUnion(std::move(args_union));
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result.value().is_null());
  EXPECT_TRUE(result.value()->is_fan());
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineArgumentsUnionSuccessWithLedLitUpArgs) {
  auto args = cx_diag::CreateLedLitUpRoutineArguments();
  args.name = cx_diag::LedName::kBattery;
  args.color = cx_diag::LedColor::kRed;
  auto args_union = cx_diag::CreateRoutineArgumentsUnion();
  args_union.led_lit_up = std::move(args);
  auto result = ConvertRoutineArgumentsUnion(std::move(args_union));
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result.value().is_null());
  ASSERT_TRUE(result.value()->is_led_lit_up());
  EXPECT_EQ(result.value()->get_led_lit_up()->name,
            crosapi::TelemetryDiagnosticLedName::kBattery);
  EXPECT_EQ(result.value()->get_led_lit_up()->color,
            crosapi::TelemetryDiagnosticLedColor::kRed);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineArgumentsUnionSuccessWithKeyboardBacklightArgs) {
  auto args_union = cx_diag::CreateRoutineArgumentsUnion();
  args_union.keyboard_backlight =
      cx_diag::CreateKeyboardBacklightRoutineArguments();
  auto result = ConvertRoutineArgumentsUnion(std::move(args_union));
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result.value().is_null());
  EXPECT_TRUE(result.value()->is_keyboard_backlight());
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineInquiryReplyUnionErrorWithMultipleNonnullFields) {
  auto reply_union = cx_diag::RoutineInquiryReplyUnion();
  reply_union.check_led_lit_up_state = cx_diag::CheckLedLitUpStateReply();
  reply_union.check_keyboard_backlight_state =
      cx_diag::CheckKeyboardBacklightStateReply();
  auto result = ConvertRoutineInquiryReplyUnion(std::move(reply_union));
  EXPECT_FALSE(result.has_value());
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineInquiryReplyUnionAllFieldsAreNull) {
  auto result =
      ConvertRoutineInquiryReplyUnion(cx_diag::RoutineInquiryReplyUnion());
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result.value().is_null());
  EXPECT_TRUE(result.value()->is_unrecognizedReply());
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineInquiryReplyUnionSuccessWithCheckLedLitUpState) {
  auto reply_union = cx_diag::RoutineInquiryReplyUnion();
  reply_union.check_led_lit_up_state = cx_diag::CheckLedLitUpStateReply();
  auto result = ConvertRoutineInquiryReplyUnion(std::move(reply_union));
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result.value().is_null());
  EXPECT_TRUE(result.value()->is_check_led_lit_up_state());
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineInquiryReplyUnionSuccessWithCheckKeyboardBacklightState) {
  auto reply_union = cx_diag::RoutineInquiryReplyUnion();
  reply_union.check_keyboard_backlight_state =
      cx_diag::CheckKeyboardBacklightStateReply();
  auto result = ConvertRoutineInquiryReplyUnion(std::move(reply_union));
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result.value().is_null());
  EXPECT_TRUE(result.value()->is_check_keyboard_backlight_state());
}

}  // namespace chromeos::converters::diagnostics

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/events/telemetry_event_service_converters.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "ash/system/diagnostics/mojom/input.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_exception.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_keyboard_event.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::converters {

// Tests that `ConvertStructPtr` function returns nullptr if input is
// nullptr. `ConvertStructPtr` is a template, so we can test this function
// with any valid type.
TEST(TelemetryEventServiceConvertersTest, ConvertStructPtrTakesNullPtr) {
  EXPECT_TRUE(ConvertStructPtr(cros_healthd::mojom::EventInfoPtr()).is_null());
}

TEST(TelemetryEventServiceConvertersTest, ConvertKeyboardConnectionType) {
  EXPECT_EQ(
      Convert(diagnostics::mojom::ConnectionType::kUnmappedEnumField),
      crosapi::mojom::TelemetryKeyboardConnectionType::kUnmappedEnumField);

  EXPECT_EQ(Convert(diagnostics::mojom::ConnectionType::kInternal),
            crosapi::mojom::TelemetryKeyboardConnectionType::kInternal);

  EXPECT_EQ(Convert(diagnostics::mojom::ConnectionType::kUsb),
            crosapi::mojom::TelemetryKeyboardConnectionType::kUsb);

  EXPECT_EQ(Convert(diagnostics::mojom::ConnectionType::kBluetooth),
            crosapi::mojom::TelemetryKeyboardConnectionType::kBluetooth);

  EXPECT_EQ(Convert(diagnostics::mojom::ConnectionType::kUnknown),
            crosapi::mojom::TelemetryKeyboardConnectionType::kUnknown);
}

TEST(TelemetryEventServiceConvertersTest, ConvertKeyboardPhysicalLayout) {
  EXPECT_EQ(
      Convert(diagnostics::mojom::PhysicalLayout::kUnmappedEnumField),
      crosapi::mojom::TelemetryKeyboardPhysicalLayout::kUnmappedEnumField);

  EXPECT_EQ(Convert(diagnostics::mojom::PhysicalLayout::kUnknown),
            crosapi::mojom::TelemetryKeyboardPhysicalLayout::kUnknown);

  EXPECT_EQ(Convert(diagnostics::mojom::PhysicalLayout::kChromeOS),
            crosapi::mojom::TelemetryKeyboardPhysicalLayout::kChromeOS);

  EXPECT_EQ(
      Convert(diagnostics::mojom::PhysicalLayout::kChromeOSDellEnterpriseWilco),
      crosapi::mojom::TelemetryKeyboardPhysicalLayout::kUnknown);

  EXPECT_EQ(
      Convert(
          diagnostics::mojom::PhysicalLayout::kChromeOSDellEnterpriseDrallion),
      crosapi::mojom::TelemetryKeyboardPhysicalLayout::kUnknown);
}

TEST(TelemetryEventServiceConvertersTest, ConvertKeyboardMechanicalLayout) {
  EXPECT_EQ(
      Convert(diagnostics::mojom::MechanicalLayout::kUnmappedEnumField),
      crosapi::mojom::TelemetryKeyboardMechanicalLayout::kUnmappedEnumField);

  EXPECT_EQ(Convert(diagnostics::mojom::MechanicalLayout::kUnknown),
            crosapi::mojom::TelemetryKeyboardMechanicalLayout::kUnknown);

  EXPECT_EQ(Convert(diagnostics::mojom::MechanicalLayout::kAnsi),
            crosapi::mojom::TelemetryKeyboardMechanicalLayout::kAnsi);

  EXPECT_EQ(Convert(diagnostics::mojom::MechanicalLayout::kIso),
            crosapi::mojom::TelemetryKeyboardMechanicalLayout::kIso);

  EXPECT_EQ(Convert(diagnostics::mojom::MechanicalLayout::kJis),
            crosapi::mojom::TelemetryKeyboardMechanicalLayout::kJis);
}

TEST(TelemetryEventServiceConvertersTest, ConvertKeyboardNumberPadPresence) {
  EXPECT_EQ(
      Convert(diagnostics::mojom::NumberPadPresence::kUnmappedEnumField),
      crosapi::mojom::TelemetryKeyboardNumberPadPresence::kUnmappedEnumField);

  EXPECT_EQ(Convert(diagnostics::mojom::NumberPadPresence::kUnknown),
            crosapi::mojom::TelemetryKeyboardNumberPadPresence::kUnknown);

  EXPECT_EQ(Convert(diagnostics::mojom::NumberPadPresence::kPresent),
            crosapi::mojom::TelemetryKeyboardNumberPadPresence::kPresent);

  EXPECT_EQ(Convert(diagnostics::mojom::NumberPadPresence::kNotPresent),
            crosapi::mojom::TelemetryKeyboardNumberPadPresence::kNotPresent);
}

TEST(TelemetryEventServiceConvertersTest, ConvertKeyboardTopRowKey) {
  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kUnmappedEnumField),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kUnmappedEnumField);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kNone),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kNone);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kUnknown),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kUnknown);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kBack),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kBack);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kForward),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kForward);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kRefresh),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kRefresh);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kFullscreen),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kFullscreen);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kOverview),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kOverview);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kScreenshot),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kScreenshot);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kScreenBrightnessDown),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kScreenBrightnessDown);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kScreenBrightnessUp),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kScreenBrightnessUp);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kPrivacyScreenToggle),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kPrivacyScreenToggle);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kMicrophoneMute),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kMicrophoneMute);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kVolumeMute),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kVolumeMute);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kVolumeDown),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kVolumeDown);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kVolumeUp),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kVolumeUp);

  EXPECT_EQ(
      Convert(diagnostics::mojom::TopRowKey::kKeyboardBacklightToggle),
      crosapi::mojom::TelemetryKeyboardTopRowKey::kKeyboardBacklightToggle);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kKeyboardBacklightDown),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kKeyboardBacklightDown);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kKeyboardBacklightUp),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kKeyboardBacklightUp);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kNextTrack),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kNextTrack);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kPreviousTrack),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kPreviousTrack);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kPlayPause),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kPlayPause);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kScreenMirror),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kScreenMirror);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRowKey::kDelete),
            crosapi::mojom::TelemetryKeyboardTopRowKey::kDelete);
}

TEST(TelemetryEventServiceConvertersTest, ConvertKeyboardTopRightKey) {
  EXPECT_EQ(Convert(diagnostics::mojom::TopRightKey::kUnmappedEnumField),
            crosapi::mojom::TelemetryKeyboardTopRightKey::kUnmappedEnumField);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRightKey::kUnknown),
            crosapi::mojom::TelemetryKeyboardTopRightKey::kUnknown);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRightKey::kPower),
            crosapi::mojom::TelemetryKeyboardTopRightKey::kPower);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRightKey::kLock),
            crosapi::mojom::TelemetryKeyboardTopRightKey::kLock);

  EXPECT_EQ(Convert(diagnostics::mojom::TopRightKey::kControlPanel),
            crosapi::mojom::TelemetryKeyboardTopRightKey::kControlPanel);
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetryAudioJackEventInfo_State) {
  EXPECT_EQ(
      Convert(
          cros_healthd::mojom::AudioJackEventInfo::State::kUnmappedEnumField),
      crosapi::mojom::TelemetryAudioJackEventInfo::State::kUnmappedEnumField);

  EXPECT_EQ(Convert(cros_healthd::mojom::AudioJackEventInfo::State::kAdd),
            crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd);

  EXPECT_EQ(Convert(cros_healthd::mojom::AudioJackEventInfo::State::kRemove),
            crosapi::mojom::TelemetryAudioJackEventInfo::State::kRemove);
}

TEST(TelemetryEventServiceConvertersTest, ConvertTelemetryUsbEventInfo_State) {
  EXPECT_EQ(
      Convert(cros_healthd::mojom::UsbEventInfo::State::kUnmappedEnumField),
      crosapi::mojom::TelemetryUsbEventInfo::State::kUnmappedEnumField);

  EXPECT_EQ(Convert(cros_healthd::mojom::UsbEventInfo::State::kAdd),
            crosapi::mojom::TelemetryUsbEventInfo::State::kAdd);

  EXPECT_EQ(Convert(cros_healthd::mojom::UsbEventInfo::State::kRemove),
            crosapi::mojom::TelemetryUsbEventInfo::State::kRemove);
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetrySdCardEventInfo_State) {
  EXPECT_EQ(
      Convert(cros_healthd::mojom::SdCardEventInfo::State::kUnmappedEnumField),
      crosapi::mojom::TelemetrySdCardEventInfo::State::kUnmappedEnumField);

  EXPECT_EQ(Convert(cros_healthd::mojom::SdCardEventInfo::State::kAdd),
            crosapi::mojom::TelemetrySdCardEventInfo::State::kAdd);

  EXPECT_EQ(Convert(cros_healthd::mojom::SdCardEventInfo::State::kRemove),
            crosapi::mojom::TelemetrySdCardEventInfo::State::kRemove);
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetryPowerEventInfo_State) {
  EXPECT_EQ(
      Convert(cros_healthd::mojom::PowerEventInfo::State::kUnmappedEnumField),
      crosapi::mojom::TelemetryPowerEventInfo::State::kUnmappedEnumField);

  EXPECT_EQ(Convert(cros_healthd::mojom::PowerEventInfo::State::kAcInserted),
            crosapi::mojom::TelemetryPowerEventInfo::State::kAcInserted);

  EXPECT_EQ(Convert(cros_healthd::mojom::PowerEventInfo::State::kAcRemoved),
            crosapi::mojom::TelemetryPowerEventInfo::State::kAcRemoved);

  EXPECT_EQ(Convert(cros_healthd::mojom::PowerEventInfo::State::kOsSuspend),
            crosapi::mojom::TelemetryPowerEventInfo::State::kOsSuspend);

  EXPECT_EQ(Convert(cros_healthd::mojom::PowerEventInfo::State::kOsResume),
            crosapi::mojom::TelemetryPowerEventInfo::State::kOsResume);
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetryAudioJackEventInfo_DeviceType) {
  EXPECT_EQ(Convert(cros_healthd::mojom::AudioJackEventInfo::DeviceType::
                        kUnmappedEnumField),
            crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::
                kUnmappedEnumField);

  EXPECT_EQ(
      Convert(cros_healthd::mojom::AudioJackEventInfo::DeviceType::kHeadphone),
      crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::kHeadphone);

  EXPECT_EQ(
      Convert(cros_healthd::mojom::AudioJackEventInfo::DeviceType::kMicrophone),
      crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::kMicrophone);
}

TEST(TelemetryEventServiceConvertersTest, ConvertTelemetryLidEventInfo_State) {
  EXPECT_EQ(
      Convert(cros_healthd::mojom::LidEventInfo::State::kUnmappedEnumField),
      crosapi::mojom::TelemetryLidEventInfo::State::kUnmappedEnumField);

  EXPECT_EQ(Convert(cros_healthd::mojom::LidEventInfo::State::kClosed),
            crosapi::mojom::TelemetryLidEventInfo::State::kClosed);

  EXPECT_EQ(Convert(cros_healthd::mojom::LidEventInfo::State::kOpened),
            crosapi::mojom::TelemetryLidEventInfo::State::kOpened);
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetryExtensionExceptionReason) {
  EXPECT_EQ(
      Convert(cros_healthd::mojom::Exception_Reason::kUnmappedEnumField),
      crosapi::mojom::TelemetryExtensionException::Reason::kUnmappedEnumField);

  EXPECT_EQ(
      Convert(
          cros_healthd::mojom::Exception_Reason::kMojoDisconnectWithoutReason),
      crosapi::mojom::TelemetryExtensionException::Reason::
          kMojoDisconnectWithoutReason);

  EXPECT_EQ(Convert(cros_healthd::mojom::Exception_Reason::kUnexpected),
            crosapi::mojom::TelemetryExtensionException::Reason::kUnexpected);

  EXPECT_EQ(Convert(cros_healthd::mojom::Exception_Reason::kUnsupported),
            crosapi::mojom::TelemetryExtensionException::Reason::kUnsupported);
}

TEST(TelemetryEventServiceConvertersTest, ConvertTelemetryEventCategoryEnum) {
  EXPECT_EQ(
      Convert(crosapi::mojom::TelemetryEventCategoryEnum::kUnmappedEnumField),
      cros_healthd::mojom::EventCategoryEnum::kUnmappedEnumField);

  EXPECT_EQ(Convert(crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack),
            cros_healthd::mojom::EventCategoryEnum::kAudioJack);

  EXPECT_EQ(Convert(crosapi::mojom::TelemetryEventCategoryEnum::kLid),
            cros_healthd::mojom::EventCategoryEnum::kLid);

  EXPECT_EQ(Convert(crosapi::mojom::TelemetryEventCategoryEnum::kUsb),
            cros_healthd::mojom::EventCategoryEnum::kUsb);

  EXPECT_EQ(Convert(crosapi::mojom::TelemetryEventCategoryEnum::kSdCard),
            cros_healthd::mojom::EventCategoryEnum::kSdCard);

  EXPECT_EQ(Convert(crosapi::mojom::TelemetryEventCategoryEnum::kPower),
            cros_healthd::mojom::EventCategoryEnum::kPower);

  EXPECT_EQ(
      Convert(crosapi::mojom::TelemetryEventCategoryEnum::kKeyboardDiagnostic),
      cros_healthd::mojom::EventCategoryEnum::kKeyboardDiagnostic);
}

TEST(TelemetryEventServiceConvertersTest, ConvertKeyboardInfo) {
  constexpr uint32_t kId = 1;
  constexpr char kName[] = "TESTNAME";
  constexpr char kRegionCode[] = "de";

  auto input = diagnostics::mojom::KeyboardInfo::New();
  input->id = kId;
  input->connection_type = diagnostics::mojom::ConnectionType::kBluetooth;
  input->name = kName;
  input->physical_layout = diagnostics::mojom::PhysicalLayout::kChromeOS;
  input->mechanical_layout = diagnostics::mojom::MechanicalLayout::kAnsi;
  input->region_code = kRegionCode;
  input->number_pad_present = diagnostics::mojom::NumberPadPresence::kPresent;
  input->top_row_keys = {diagnostics::mojom::TopRowKey::kBack,
                         diagnostics::mojom::TopRowKey::kForward};
  input->top_right_key = diagnostics::mojom::TopRightKey::kPower;
  input->has_assistant_key = true;

  auto result = ConvertStructPtr(std::move(input));

  ASSERT_TRUE(result);

  ASSERT_FALSE(result->id.is_null());
  EXPECT_EQ(result->id->value, kId);

  EXPECT_EQ(result->connection_type,
            crosapi::mojom::TelemetryKeyboardConnectionType::kBluetooth);

  ASSERT_TRUE(result->name);
  EXPECT_EQ(*result->name, kName);

  EXPECT_EQ(result->physical_layout,
            crosapi::mojom::TelemetryKeyboardPhysicalLayout::kChromeOS);
  EXPECT_EQ(result->mechanical_layout,
            crosapi::mojom::TelemetryKeyboardMechanicalLayout::kAnsi);

  ASSERT_TRUE(result->region_code);
  EXPECT_EQ(*result->region_code, kRegionCode);

  EXPECT_EQ(result->number_pad_present,
            crosapi::mojom::TelemetryKeyboardNumberPadPresence::kPresent);

  ASSERT_TRUE(result->top_row_keys);
  ASSERT_EQ(result->top_row_keys->size(), 2UL);
  EXPECT_THAT(*result->top_row_keys,
              testing::ElementsAre(
                  crosapi::mojom::TelemetryKeyboardTopRowKey::kBack,
                  crosapi::mojom::TelemetryKeyboardTopRowKey::kForward));

  EXPECT_EQ(result->top_right_key,
            crosapi::mojom::TelemetryKeyboardTopRightKey::kPower);

  ASSERT_FALSE(result->has_assistant_key.is_null());
  EXPECT_TRUE(result->has_assistant_key->value);
}

TEST(TelemetryEventServiceConvertersTest, ConvertKeyboardDiagnosticEventInfo) {
  constexpr uint32_t kId = 1;
  constexpr char kName[] = "TESTNAME";
  constexpr char kRegionCode[] = "de";

  std::vector<uint32_t> kTestedKeys = {1, 2, 3, 4, 5, 6};
  std::vector<uint32_t> kTestedTopRowKeys = {7, 8, 9, 10, 11, 12};

  auto keyboard = diagnostics::mojom::KeyboardInfo::New();
  keyboard->id = kId;
  keyboard->connection_type = diagnostics::mojom::ConnectionType::kBluetooth;
  keyboard->name = kName;
  keyboard->physical_layout = diagnostics::mojom::PhysicalLayout::kChromeOS;
  keyboard->mechanical_layout = diagnostics::mojom::MechanicalLayout::kAnsi;
  keyboard->region_code = kRegionCode;
  keyboard->number_pad_present =
      diagnostics::mojom::NumberPadPresence::kPresent;
  keyboard->top_row_keys = {diagnostics::mojom::TopRowKey::kBack,
                            diagnostics::mojom::TopRowKey::kForward};
  keyboard->top_right_key = diagnostics::mojom::TopRightKey::kPower;
  keyboard->has_assistant_key = true;

  auto input = diagnostics::mojom::KeyboardDiagnosticEventInfo::New();
  input->keyboard_info = std::move(keyboard);
  input->tested_keys = kTestedKeys;
  input->tested_top_row_keys = kTestedTopRowKeys;

  auto result = ConvertStructPtr(std::move(input));

  ASSERT_TRUE(result);

  auto keyboard_info_result = std::move(result->keyboard_info);
  ASSERT_TRUE(keyboard_info_result);

  ASSERT_FALSE(keyboard_info_result->id.is_null());
  EXPECT_EQ(keyboard_info_result->id->value, kId);

  EXPECT_EQ(keyboard_info_result->connection_type,
            crosapi::mojom::TelemetryKeyboardConnectionType::kBluetooth);

  ASSERT_TRUE(keyboard_info_result->name);
  EXPECT_EQ(*keyboard_info_result->name, kName);

  EXPECT_EQ(keyboard_info_result->physical_layout,
            crosapi::mojom::TelemetryKeyboardPhysicalLayout::kChromeOS);
  EXPECT_EQ(keyboard_info_result->mechanical_layout,
            crosapi::mojom::TelemetryKeyboardMechanicalLayout::kAnsi);

  ASSERT_TRUE(keyboard_info_result->region_code);
  EXPECT_EQ(*keyboard_info_result->region_code, kRegionCode);

  EXPECT_EQ(keyboard_info_result->number_pad_present,
            crosapi::mojom::TelemetryKeyboardNumberPadPresence::kPresent);

  ASSERT_TRUE(keyboard_info_result->top_row_keys);
  ASSERT_EQ(keyboard_info_result->top_row_keys->size(), 2UL);
  EXPECT_THAT(*keyboard_info_result->top_row_keys,
              testing::ElementsAre(
                  crosapi::mojom::TelemetryKeyboardTopRowKey::kBack,
                  crosapi::mojom::TelemetryKeyboardTopRowKey::kForward));

  EXPECT_EQ(keyboard_info_result->top_right_key,
            crosapi::mojom::TelemetryKeyboardTopRightKey::kPower);

  ASSERT_FALSE(keyboard_info_result->has_assistant_key.is_null());
  EXPECT_TRUE(keyboard_info_result->has_assistant_key->value);

  ASSERT_TRUE(result->tested_keys);
  EXPECT_EQ(*result->tested_keys, kTestedKeys);

  ASSERT_TRUE(result->tested_top_row_keys);
  EXPECT_EQ(*result->tested_top_row_keys, kTestedTopRowKeys);
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetryAudioJackEventInfoPtr) {
  auto input = cros_healthd::mojom::AudioJackEventInfo::New();
  input->state = cros_healthd::mojom::AudioJackEventInfo::State::kAdd;

  EXPECT_EQ(ConvertStructPtr(std::move(input)),
            crosapi::mojom::TelemetryAudioJackEventInfo::New(
                crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd));
}

TEST(TelemetryEventServiceConvertersTest, ConvertTelemetryLidEventInfoPtr) {
  auto input = cros_healthd::mojom::LidEventInfo::New();
  input->state = cros_healthd::mojom::LidEventInfo::State::kClosed;

  EXPECT_EQ(ConvertStructPtr(std::move(input)),
            crosapi::mojom::TelemetryLidEventInfo::New(
                crosapi::mojom::TelemetryLidEventInfo::State::kClosed));
}

TEST(TelemetryEventServiceConvertersTest, ConvertTelemetryUsbEventInfoPtr) {
  std::vector<std::string> categories = {"category1", "category2"};
  auto input = cros_healthd::mojom::UsbEventInfo::New();
  input->state = cros_healthd::mojom::UsbEventInfo::State::kAdd;
  input->vendor = "test_vendor";
  input->name = "test_name";
  input->vid = 1;
  input->pid = 2;
  input->categories = categories;

  EXPECT_EQ(ConvertStructPtr(std::move(input)),
            crosapi::mojom::TelemetryUsbEventInfo::New(
                "test_vendor", "test_name", 1, 2, categories,
                crosapi::mojom::TelemetryUsbEventInfo::State::kAdd));
}

TEST(TelemetryEventServiceConvertersTest, ConvertTelemetrySdCardEventInfoPtr) {
  auto input = cros_healthd::mojom::SdCardEventInfo::New();
  input->state = cros_healthd::mojom::SdCardEventInfo::State::kAdd;

  EXPECT_EQ(ConvertStructPtr(std::move(input)),
            crosapi::mojom::TelemetrySdCardEventInfo::New(
                crosapi::mojom::TelemetrySdCardEventInfo::State::kAdd));
}

TEST(TelemetryEventServiceConvertersTest, ConvertTelemetryPowerEventInfoPtr) {
  auto input = cros_healthd::mojom::PowerEventInfo::New();
  input->state = cros_healthd::mojom::PowerEventInfo::State::kAcInserted;

  EXPECT_EQ(ConvertStructPtr(std::move(input)),
            crosapi::mojom::TelemetryPowerEventInfo::New(
                crosapi::mojom::TelemetryPowerEventInfo::State::kAcInserted));
}

TEST(TelemetryEventServiceConvertersTest, ConvertTelemetryExtensionException) {
  constexpr char kDebugMessage[] = "TestMessage";

  auto input = cros_healthd::mojom::Exception::New();
  input->reason = cros_healthd::mojom::Exception::Reason::kUnexpected;
  input->debug_message = kDebugMessage;

  auto result = ConvertStructPtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->reason,
            crosapi::mojom::TelemetryExtensionException::Reason::kUnexpected);
  EXPECT_EQ(result->debug_message, kDebugMessage);
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetryExtensionSupportedPtr) {
  EXPECT_EQ(ConvertStructPtr(cros_healthd::mojom::Supported::New()),
            crosapi::mojom::TelemetryExtensionSupported::New());
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetryExtensionUnsupportedReasonPtr) {
  EXPECT_EQ(
      ConvertStructPtr(
          cros_healthd::mojom::UnsupportedReason::NewUnmappedUnionField(9)),
      crosapi::mojom::TelemetryExtensionUnsupportedReason::
          NewUnmappedUnionField(9));
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetryExtensionUnsupportedPtr) {
  constexpr char kDebugMsg[] = "Test";
  constexpr uint8_t kUnmappedUnionField = 4;

  auto input = cros_healthd::mojom::Unsupported::New();
  input->debug_message = kDebugMsg;
  input->reason = cros_healthd::mojom::UnsupportedReason::NewUnmappedUnionField(
      kUnmappedUnionField);

  auto result = ConvertStructPtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->debug_message, kDebugMsg);
  EXPECT_EQ(result->reason,
            crosapi::mojom::TelemetryExtensionUnsupportedReason::
                NewUnmappedUnionField(kUnmappedUnionField));
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetryExtensionSupportStatusPtr) {
  constexpr char kDebugMsg[] = "Test";
  constexpr uint8_t kUnmappedUnionField = 4;

  EXPECT_EQ(ConvertStructPtr(cros_healthd::mojom::SupportStatus::NewSupported(
                cros_healthd::mojom::Supported::New())),
            crosapi::mojom::TelemetryExtensionSupportStatus::NewSupported(
                crosapi::mojom::TelemetryExtensionSupported::New()));

  EXPECT_EQ(
      ConvertStructPtr(
          cros_healthd::mojom::SupportStatus::NewUnmappedUnionField(
              kUnmappedUnionField)),
      crosapi::mojom::TelemetryExtensionSupportStatus::NewUnmappedUnionField(
          kUnmappedUnionField));

  auto unsupported = cros_healthd::mojom::Unsupported::New();
  unsupported->debug_message = kDebugMsg;
  unsupported->reason =
      cros_healthd::mojom::UnsupportedReason::NewUnmappedUnionField(
          kUnmappedUnionField);

  auto unsupported_result =
      ConvertStructPtr(cros_healthd::mojom::SupportStatus::NewUnsupported(
          std::move(unsupported)));

  ASSERT_TRUE(unsupported_result->is_unsupported());
  EXPECT_EQ(unsupported_result->get_unsupported()->debug_message, kDebugMsg);
  EXPECT_EQ(unsupported_result->get_unsupported()->reason,
            crosapi::mojom::TelemetryExtensionUnsupportedReason::
                NewUnmappedUnionField(kUnmappedUnionField));

  auto exception = cros_healthd::mojom::Exception::New();
  exception->reason = cros_healthd::mojom::Exception::Reason::kUnexpected;
  exception->debug_message = kDebugMsg;

  auto exception_result = ConvertStructPtr(
      cros_healthd::mojom::SupportStatus::NewException(std::move(exception)));

  ASSERT_TRUE(exception_result->is_exception());
  EXPECT_EQ(exception_result->get_exception()->reason,
            crosapi::mojom::TelemetryExtensionException::Reason::kUnexpected);
  EXPECT_EQ(exception_result->get_exception()->debug_message, kDebugMsg);
}

TEST(TelemetryEventServiceConvertersTest, ConvertTelemetryEventInfoPtr) {
  auto audio_jack_info = cros_healthd::mojom::AudioJackEventInfo::New();
  audio_jack_info->state = cros_healthd::mojom::AudioJackEventInfo::State::kAdd;

  auto input = cros_healthd::mojom::EventInfo::NewAudioJackEventInfo(
      std::move(audio_jack_info));

  EXPECT_EQ(ConvertStructPtr(std::move(input)),
            crosapi::mojom::TelemetryEventInfo::NewAudioJackEventInfo(
                crosapi::mojom::TelemetryAudioJackEventInfo::New(
                    crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd)));

  auto illegal_info = cros_healthd::mojom::ThunderboltEventInfo::New();
  auto illegal_input = cros_healthd::mojom::EventInfo::NewThunderboltEventInfo(
      std::move(illegal_info));

  EXPECT_TRUE(ConvertStructPtr(std::move(illegal_input)).is_null());
}

}  // namespace ash::converters

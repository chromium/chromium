// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api_converters.h"

#include "chrome/common/chromeos/extensions/api/events.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::converters::events {

namespace {

namespace cx_events = ::chromeos::api::os_events;

}  // namespace

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertAudioJackState) {
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::AudioJackEventInfo::State::
                        kUnmappedEnumField),
            cx_events::AudioJackEvent::kNone);

  EXPECT_EQ(Convert(ash::cros_healthd::mojom::AudioJackEventInfo::State::kAdd),
            cx_events::AudioJackEvent::kConnected);

  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::AudioJackEventInfo::State::kRemove),
      cx_events::AudioJackEvent::kDisconnected);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest,
     ConvertAudioJackDeviceType) {
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::AudioJackEventInfo::DeviceType::
                        kUnmappedEnumField),
            cx_events::AudioJackDeviceType::kNone);

  EXPECT_EQ(
      Convert(
          ash::cros_healthd::mojom::AudioJackEventInfo::DeviceType::kHeadphone),
      cx_events::AudioJackDeviceType::kHeadphone);

  EXPECT_EQ(Convert(ash::cros_healthd::mojom::AudioJackEventInfo::DeviceType::
                        kMicrophone),
            cx_events::AudioJackDeviceType::kMicrophone);
}

TEST(CrosTelemetryEventServiceConvertersTest, ConvertKeyboardConnectionType) {
  EXPECT_EQ(
      Convert(ash::diagnostics::mojom::ConnectionType::kUnmappedEnumField),
      cx_events::KeyboardConnectionType::kNone);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::ConnectionType::kInternal),
            cx_events::KeyboardConnectionType::kInternal);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::ConnectionType::kUsb),
            cx_events::KeyboardConnectionType::kUsb);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::ConnectionType::kBluetooth),
            cx_events::KeyboardConnectionType::kBluetooth);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::ConnectionType::kUnknown),
            cx_events::KeyboardConnectionType::kUnknown);
}

TEST(CrosTelemetryEventServiceConvertersTest, ConvertKeyboardPhysicalLayout) {
  EXPECT_EQ(
      Convert(ash::diagnostics::mojom::PhysicalLayout::kUnmappedEnumField),
      cx_events::PhysicalKeyboardLayout::kNone);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::PhysicalLayout::kUnknown),
            cx_events::PhysicalKeyboardLayout::kUnknown);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::PhysicalLayout::
                        kChromeOSDellEnterpriseWilco),
            cx_events::PhysicalKeyboardLayout::kUnknown);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::PhysicalLayout::
                        kChromeOSDellEnterpriseDrallion),
            cx_events::PhysicalKeyboardLayout::kUnknown);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::PhysicalLayout::kChromeOS),
            cx_events::PhysicalKeyboardLayout::kChromeOs);
}

TEST(CrosTelemetryEventServiceConvertersTest, ConvertKeyboardMechanicalLayout) {
  EXPECT_EQ(
      Convert(ash::diagnostics::mojom::MechanicalLayout::kUnmappedEnumField),
      cx_events::MechanicalKeyboardLayout::kNone);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::MechanicalLayout::kUnknown),
            cx_events::MechanicalKeyboardLayout::kUnknown);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::MechanicalLayout::kAnsi),
            cx_events::MechanicalKeyboardLayout::kAnsi);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::MechanicalLayout::kIso),
            cx_events::MechanicalKeyboardLayout::kIso);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::MechanicalLayout::kJis),
            cx_events::MechanicalKeyboardLayout::kJis);
}

TEST(CrosTelemetryEventServiceConvertersTest,
     ConvertKeyboardNumberPadPresence) {
  EXPECT_EQ(
      Convert(ash::diagnostics::mojom::NumberPadPresence::kUnmappedEnumField),
      cx_events::KeyboardNumberPadPresence::kNone);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::NumberPadPresence::kUnknown),
            cx_events::KeyboardNumberPadPresence::kUnknown);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::NumberPadPresence::kPresent),
            cx_events::KeyboardNumberPadPresence::kPresent);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::NumberPadPresence::kNotPresent),
            cx_events::KeyboardNumberPadPresence::kNotPresent);
}

TEST(CrosTelemetryEventServiceConvertersTest, ConvertKeyboardTopRowKey) {
  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kUnmappedEnumField),
            cx_events::KeyboardTopRowKey::kNone);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kNone),
            cx_events::KeyboardTopRowKey::kNoKey);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kUnknown),
            cx_events::KeyboardTopRowKey::kUnknown);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kAccessibility),
            cx_events::KeyboardTopRowKey::kUnknown);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kDictation),
            cx_events::KeyboardTopRowKey::kUnknown);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kBack),
            cx_events::KeyboardTopRowKey::kBack);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kForward),
            cx_events::KeyboardTopRowKey::kForward);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kRefresh),
            cx_events::KeyboardTopRowKey::kRefresh);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kFullscreen),
            cx_events::KeyboardTopRowKey::kFullscreen);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kOverview),
            cx_events::KeyboardTopRowKey::kOverview);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kScreenshot),
            cx_events::KeyboardTopRowKey::kScreenshot);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kScreenBrightnessDown),
            cx_events::KeyboardTopRowKey::kScreenBrightnessDown);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kScreenBrightnessUp),
            cx_events::KeyboardTopRowKey::kScreenBrightnessUp);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kPrivacyScreenToggle),
            cx_events::KeyboardTopRowKey::kPrivacyScreenToggle);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kMicrophoneMute),
            cx_events::KeyboardTopRowKey::kMicrophoneMute);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kVolumeMute),
            cx_events::KeyboardTopRowKey::kVolumeMute);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kVolumeDown),
            cx_events::KeyboardTopRowKey::kVolumeDown);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kVolumeUp),
            cx_events::KeyboardTopRowKey::kVolumeUp);

  EXPECT_EQ(
      Convert(ash::diagnostics::mojom::TopRowKey::kKeyboardBacklightToggle),
      cx_events::KeyboardTopRowKey::kKeyboardBacklightToggle);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kKeyboardBacklightDown),
            cx_events::KeyboardTopRowKey::kKeyboardBacklightDown);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kKeyboardBacklightUp),
            cx_events::KeyboardTopRowKey::kKeyboardBacklightUp);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kNextTrack),
            cx_events::KeyboardTopRowKey::kNextTrack);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kPreviousTrack),
            cx_events::KeyboardTopRowKey::kPreviousTrack);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kPlayPause),
            cx_events::KeyboardTopRowKey::kPlayPause);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kScreenMirror),
            cx_events::KeyboardTopRowKey::kScreenMirror);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRowKey::kDelete),
            cx_events::KeyboardTopRowKey::kDelete);
}

TEST(CrosTelemetryEventServiceConvertersTest, ConvertKeyboardTopRightKey) {
  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRightKey::kUnmappedEnumField),
            cx_events::KeyboardTopRightKey::kNone);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRightKey::kUnknown),
            cx_events::KeyboardTopRightKey::kUnknown);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRightKey::kPower),
            cx_events::KeyboardTopRightKey::kPower);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRightKey::kLock),
            cx_events::KeyboardTopRightKey::kLock);

  EXPECT_EQ(Convert(ash::diagnostics::mojom::TopRightKey::kControlPanel),
            cx_events::KeyboardTopRightKey::kControlPanel);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertLidState) {
  EXPECT_EQ(
      Convert(
          ash::cros_healthd::mojom::LidEventInfo::State::kUnmappedEnumField),
      cx_events::LidEvent::kNone);

  EXPECT_EQ(Convert(ash::cros_healthd::mojom::LidEventInfo::State::kClosed),
            cx_events::LidEvent::kClosed);

  EXPECT_EQ(Convert(ash::cros_healthd::mojom::LidEventInfo::State::kOpened),
            cx_events::LidEvent::kOpened);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertUsbState) {
  EXPECT_EQ(
      Convert(
          ash::cros_healthd::mojom::UsbEventInfo::State::kUnmappedEnumField),
      cx_events::UsbEvent::kNone);

  EXPECT_EQ(Convert(ash::cros_healthd::mojom::UsbEventInfo::State::kAdd),
            cx_events::UsbEvent::kConnected);

  EXPECT_EQ(Convert(ash::cros_healthd::mojom::UsbEventInfo::State::kRemove),
            cx_events::UsbEvent::kDisconnected);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertDisplayInputType) {
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::DisplayInputType::kUnmappedEnumField),
      cx_events::DisplayInputType::kUnknown);

  EXPECT_EQ(Convert(ash::cros_healthd::mojom::DisplayInputType::kDigital),
            cx_events::DisplayInputType::kDigital);

  EXPECT_EQ(Convert(ash::cros_healthd::mojom::DisplayInputType::kAnalog),
            cx_events::DisplayInputType::kAnalog);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest,
     ConvertExternalDisplayState) {
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::ExternalDisplayEventInfo::State::
                        kUnmappedEnumField),
            cx_events::ExternalDisplayEvent::kNone);

  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::ExternalDisplayEventInfo::State::kAdd),
      cx_events::ExternalDisplayEvent::kConnected);

  EXPECT_EQ(
      Convert(
          ash::cros_healthd::mojom::ExternalDisplayEventInfo::State::kRemove),
      cx_events::ExternalDisplayEvent::kDisconnected);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertSdCardState) {
  EXPECT_EQ(
      Convert(
          ash::cros_healthd::mojom::SdCardEventInfo::State::kUnmappedEnumField),
      cx_events::SdCardEvent::kNone);

  EXPECT_EQ(Convert(ash::cros_healthd::mojom::SdCardEventInfo::State::kAdd),
            cx_events::SdCardEvent::kConnected);

  EXPECT_EQ(Convert(ash::cros_healthd::mojom::SdCardEventInfo::State::kRemove),
            cx_events::SdCardEvent::kDisconnected);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertPowerState) {
  EXPECT_EQ(
      Convert(
          ash::cros_healthd::mojom::PowerEventInfo::State::kUnmappedEnumField),
      cx_events::PowerEvent::kNone);

  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::PowerEventInfo::State::kAcInserted),
      cx_events::PowerEvent::kAcInserted);

  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::PowerEventInfo::State::kAcRemoved),
      cx_events::PowerEvent::kAcRemoved);

  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::PowerEventInfo::State::kOsSuspend),
      cx_events::PowerEvent::kOsSuspend);

  EXPECT_EQ(Convert(ash::cros_healthd::mojom::PowerEventInfo::State::kOsResume),
            cx_events::PowerEvent::kOsResume);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertStylusGarageState) {
  EXPECT_EQ(Convert(ash::cros_healthd::mojom::StylusGarageEventInfo::State::
                        kUnmappedEnumField),
            cx_events::StylusGarageEvent::kNone);

  EXPECT_EQ(
      Convert(
          ash::cros_healthd::mojom::StylusGarageEventInfo::State::kInserted),
      cx_events::StylusGarageEvent::kInserted);

  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::StylusGarageEventInfo::State::kRemoved),
      cx_events::StylusGarageEvent::kRemoved);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertInputTouchButton) {
  EXPECT_EQ(
      Convert(ash::cros_healthd::mojom::InputTouchButton::kUnmappedEnumField),
      cx_events::InputTouchButton::kNone);

  EXPECT_EQ(Convert(ash::cros_healthd::mojom::InputTouchButton::kLeft),
            cx_events::InputTouchButton::kLeft);

  EXPECT_EQ(Convert(ash::cros_healthd::mojom::InputTouchButton::kMiddle),
            cx_events::InputTouchButton::kMiddle);

  EXPECT_EQ(Convert(ash::cros_healthd::mojom::InputTouchButton::kRight),
            cx_events::InputTouchButton::kRight);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest,
     ConvertStylusTouchpointInfo) {
  constexpr int kX = 1;
  constexpr int kY = 1;
  constexpr int kPressure = 1;
  {
    auto output =
        ConvertStructPtr(ash::cros_healthd::mojom::StylusTouchPointInfo::New(
            kX, kY, ash::cros_healthd::mojom::NullableUint32::New(kPressure)));
    EXPECT_EQ(output.x, kX);
    EXPECT_EQ(output.y, kY);
    EXPECT_EQ(output.pressure, kPressure);
  }
  {
    auto output = ConvertStructPtr(
        ash::cros_healthd::mojom::StylusTouchPointInfo::New(kX, kY, nullptr));
    EXPECT_EQ(output.pressure, std::nullopt);
  }
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertEventCategoryEnum) {
  EXPECT_EQ(Convert(cx_events::EventCategory::kNone),
            ash::cros_healthd::mojom::EventCategoryEnum::kUnmappedEnumField);

  EXPECT_EQ(Convert(cx_events::EventCategory::kAudioJack),
            ash::cros_healthd::mojom::EventCategoryEnum::kAudioJack);

  EXPECT_EQ(Convert(cx_events::EventCategory::kLid),
            ash::cros_healthd::mojom::EventCategoryEnum::kLid);

  EXPECT_EQ(Convert(cx_events::EventCategory::kUsb),
            ash::cros_healthd::mojom::EventCategoryEnum::kUsb);

  EXPECT_EQ(Convert(cx_events::EventCategory::kExternalDisplay),
            ash::cros_healthd::mojom::EventCategoryEnum::kExternalDisplay);

  EXPECT_EQ(Convert(cx_events::EventCategory::kSdCard),
            ash::cros_healthd::mojom::EventCategoryEnum::kSdCard);

  EXPECT_EQ(Convert(cx_events::EventCategory::kPower),
            ash::cros_healthd::mojom::EventCategoryEnum::kPower);

  EXPECT_EQ(Convert(cx_events::EventCategory::kKeyboardDiagnostic),
            ash::cros_healthd::mojom::EventCategoryEnum::kKeyboardDiagnostic);

  EXPECT_EQ(Convert(cx_events::EventCategory::kStylusGarage),
            ash::cros_healthd::mojom::EventCategoryEnum::kStylusGarage);

  EXPECT_EQ(Convert(cx_events::EventCategory::kTouchpadButton),
            ash::cros_healthd::mojom::EventCategoryEnum::kTouchpad);

  EXPECT_EQ(Convert(cx_events::EventCategory::kTouchpadTouch),
            ash::cros_healthd::mojom::EventCategoryEnum::kTouchpad);

  EXPECT_EQ(Convert(cx_events::EventCategory::kTouchpadConnected),
            ash::cros_healthd::mojom::EventCategoryEnum::kTouchpad);

  EXPECT_EQ(Convert(cx_events::EventCategory::kTouchscreenTouch),
            ash::cros_healthd::mojom::EventCategoryEnum::kTouchscreen);

  EXPECT_EQ(Convert(cx_events::EventCategory::kTouchscreenConnected),
            ash::cros_healthd::mojom::EventCategoryEnum::kTouchscreen);

  EXPECT_EQ(Convert(cx_events::EventCategory::kStylusTouch),
            ash::cros_healthd::mojom::EventCategoryEnum::kStylus);

  EXPECT_EQ(Convert(cx_events::EventCategory::kStylusConnected),
            ash::cros_healthd::mojom::EventCategoryEnum::kStylus);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertKeyboardInfo) {
  constexpr int kId = 1;
  constexpr char kName[] = "TESTNAME";
  constexpr char kRegionCode[] = "de";

  auto input = ash::diagnostics::mojom::KeyboardInfo::New();
  input->id = kId;
  input->connection_type = ash::diagnostics::mojom::ConnectionType::kBluetooth;
  input->name = kName;
  input->physical_layout = ash::diagnostics::mojom::PhysicalLayout::kChromeOS;
  input->mechanical_layout = ash::diagnostics::mojom::MechanicalLayout::kAnsi;
  input->region_code = kRegionCode;
  input->number_pad_present =
      ash::diagnostics::mojom::NumberPadPresence::kPresent;
  input->top_row_keys = {ash::diagnostics::mojom::TopRowKey::kBack,
                         ash::diagnostics::mojom::TopRowKey::kForward};
  input->top_right_key = ash::diagnostics::mojom::TopRightKey::kPower;
  input->has_assistant_key = true;

  auto result = ConvertStructPtr(std::move(input));

  ASSERT_TRUE(result.id.has_value());
  EXPECT_EQ(*result.id, kId);

  EXPECT_EQ(result.connection_type,
            cx_events::KeyboardConnectionType::kBluetooth);

  ASSERT_TRUE(result.name.has_value());
  EXPECT_EQ(*result.name, kName);

  EXPECT_EQ(result.physical_layout,
            cx_events::PhysicalKeyboardLayout::kChromeOs);
  EXPECT_EQ(result.mechanical_layout,
            cx_events::MechanicalKeyboardLayout::kAnsi);

  ASSERT_TRUE(result.region_code.has_value());
  EXPECT_EQ(*result.region_code, kRegionCode);

  EXPECT_EQ(result.number_pad_present,
            cx_events::KeyboardNumberPadPresence::kPresent);

  ASSERT_EQ(result.top_row_keys.size(), 2UL);
  EXPECT_THAT(result.top_row_keys,
              testing::ElementsAre(cx_events::KeyboardTopRowKey::kBack,
                                   cx_events::KeyboardTopRowKey::kForward));

  EXPECT_EQ(result.top_right_key, cx_events::KeyboardTopRightKey::kPower);

  ASSERT_TRUE(result.has_assistant_key.has_value());
  EXPECT_TRUE(result.has_assistant_key.value());
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest,
     ConvertKeyboardDiagnosticEventInfo) {
  constexpr int kId = 1;
  constexpr char kName[] = "TESTNAME";
  constexpr char kRegionCode[] = "de";

  const std::vector<uint32_t> kTestedKeys = {1, 2, 3, 4, 5, 6};
  const std::vector<uint32_t> kTestedTopRowKeys = {7, 8, 9, 10, 11, 12};

  auto keyboard = ash::diagnostics::mojom::KeyboardInfo::New();
  keyboard->id = kId;
  keyboard->connection_type =
      ash::diagnostics::mojom::ConnectionType::kBluetooth;
  keyboard->name = kName;
  keyboard->physical_layout =
      ash::diagnostics::mojom::PhysicalLayout::kChromeOS;
  keyboard->mechanical_layout =
      ash::diagnostics::mojom::MechanicalLayout::kAnsi;
  keyboard->region_code = kRegionCode;
  keyboard->number_pad_present =
      ash::diagnostics::mojom::NumberPadPresence::kPresent;
  keyboard->top_row_keys = {ash::diagnostics::mojom::TopRowKey::kBack,
                            ash::diagnostics::mojom::TopRowKey::kForward};
  keyboard->top_right_key = ash::diagnostics::mojom::TopRightKey::kPower;
  keyboard->has_assistant_key = true;

  auto input = ash::diagnostics::mojom::KeyboardDiagnosticEventInfo::New();
  input->keyboard_info = std::move(keyboard);
  input->tested_keys = kTestedKeys;
  input->tested_top_row_keys = kTestedTopRowKeys;

  auto result = ConvertStructPtr(std::move(input));

  ASSERT_TRUE(result.keyboard_info.has_value());

  auto keyboard_info_result = std::move(result.keyboard_info.value());
  ASSERT_TRUE(keyboard_info_result.id.has_value());
  EXPECT_EQ(*keyboard_info_result.id, kId);

  EXPECT_EQ(keyboard_info_result.connection_type,
            cx_events::KeyboardConnectionType::kBluetooth);

  ASSERT_TRUE(keyboard_info_result.name.has_value());
  EXPECT_EQ(*keyboard_info_result.name, kName);

  EXPECT_EQ(keyboard_info_result.physical_layout,
            cx_events::PhysicalKeyboardLayout::kChromeOs);
  EXPECT_EQ(keyboard_info_result.mechanical_layout,
            cx_events::MechanicalKeyboardLayout::kAnsi);

  ASSERT_TRUE(keyboard_info_result.region_code.has_value());
  EXPECT_EQ(*keyboard_info_result.region_code, kRegionCode);

  EXPECT_EQ(keyboard_info_result.number_pad_present,
            cx_events::KeyboardNumberPadPresence::kPresent);

  ASSERT_EQ(keyboard_info_result.top_row_keys.size(), 2UL);
  EXPECT_THAT(keyboard_info_result.top_row_keys,
              testing::ElementsAre(cx_events::KeyboardTopRowKey::kBack,
                                   cx_events::KeyboardTopRowKey::kForward));

  EXPECT_EQ(keyboard_info_result.top_right_key,
            cx_events::KeyboardTopRightKey::kPower);

  ASSERT_TRUE(keyboard_info_result.has_assistant_key.has_value());
  EXPECT_TRUE(keyboard_info_result.has_assistant_key.value());

  EXPECT_THAT(result.tested_keys, testing::ElementsAre(1, 2, 3, 4, 5, 6));
  EXPECT_THAT(result.tested_top_row_keys,
              testing::ElementsAre(7, 8, 9, 10, 11, 12));
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertAudioJackEventInfo) {
  auto input = ash::cros_healthd::mojom::AudioJackEventInfo::New();
  input->state = ash::cros_healthd::mojom::AudioJackEventInfo::State::kAdd;
  input->device_type =
      ash::cros_healthd::mojom::AudioJackEventInfo::DeviceType::kHeadphone;

  auto result = ConvertStructPtr(std::move(input));

  EXPECT_EQ(result.event, cx_events::AudioJackEvent::kConnected);
  EXPECT_EQ(result.device_type, cx_events::AudioJackDeviceType::kHeadphone);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertLidEventInfo) {
  auto input = ash::cros_healthd::mojom::LidEventInfo::New();
  input->state = ash::cros_healthd::mojom::LidEventInfo::State::kOpened;

  auto result = ConvertStructPtr(std::move(input));

  EXPECT_EQ(result.event, cx_events::LidEvent::kOpened);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertUsbEventInfo) {
  auto input = ash::cros_healthd::mojom::UsbEventInfo::New();
  std::vector<std::string> categories = {"category1", "category2"};
  input->state = ash::cros_healthd::mojom::UsbEventInfo::State::kAdd;
  input->vendor = "test_vendor";
  input->name = "test_name";
  input->vid = 1;
  input->pid = 2;
  input->categories = categories;

  auto result = ConvertStructPtr(std::move(input));

  EXPECT_EQ(result.event, cx_events::UsbEvent::kConnected);
  EXPECT_EQ(result.vendor, "test_vendor");
  EXPECT_EQ(result.name, "test_name");
  EXPECT_EQ(result.vid, 1);
  EXPECT_EQ(result.pid, 2);
  EXPECT_EQ(result.categories, categories);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest,
     ConvertExternalDisplayEventInfo) {
  constexpr uint32_t kDisplayWidth = 0;
  constexpr uint32_t kDisplayHeight = 1;
  constexpr uint32_t kResolutionHorizontal = 2;
  constexpr uint32_t kResolutionVertical = 3;
  constexpr double kRefreshRate = 4.4;
  constexpr char kManufacturer[] = "manufacturer";
  constexpr uint16_t kModelId = 5;
  constexpr uint32_t kSerialNumber = 6;
  constexpr uint8_t kManufactureWeek = 7;
  constexpr uint16_t kManufactureYear = 8;
  constexpr char kEdidVersion[] = "1.4";
  constexpr ash::cros_healthd::mojom::DisplayInputType kInputType =
      ash::cros_healthd::mojom::DisplayInputType::kDigital;
  constexpr char kDisplayName[] = "display";

  auto input = ash::cros_healthd::mojom::ExternalDisplayEventInfo::New();
  input->state =
      ash::cros_healthd::mojom::ExternalDisplayEventInfo::State::kAdd;
  input->display_info = ash::cros_healthd::mojom::ExternalDisplayInfo::New(
      ash::cros_healthd::mojom::NullableUint32::New(kDisplayWidth),
      ash::cros_healthd::mojom::NullableUint32::New(kDisplayHeight),
      ash::cros_healthd::mojom::NullableUint32::New(kResolutionHorizontal),
      ash::cros_healthd::mojom::NullableUint32::New(kResolutionVertical),
      ash::cros_healthd::mojom::NullableDouble::New(kRefreshRate),
      std::string(kManufacturer),
      ash::cros_healthd::mojom::NullableUint16::New(kModelId),
      ash::cros_healthd::mojom::NullableUint32::New(kSerialNumber),
      ash::cros_healthd::mojom::NullableUint8::New(kManufactureWeek),
      ash::cros_healthd::mojom::NullableUint16::New(kManufactureYear),
      std::string(kEdidVersion), kInputType, std::string(kDisplayName));

  auto result = ConvertStructPtr(std::move(input));

  EXPECT_EQ(result.event, cx_events::ExternalDisplayEvent::kConnected);

  ASSERT_TRUE(result.display_info.has_value());
  const auto& display_info = result.display_info.value();

  ASSERT_TRUE(display_info.display_width.has_value());
  EXPECT_EQ(static_cast<uint32_t>(display_info.display_width.value()),
            kDisplayWidth);
  ASSERT_TRUE(display_info.display_height.has_value());
  EXPECT_EQ(static_cast<uint32_t>(display_info.display_height.value()),
            kDisplayHeight);
  ASSERT_TRUE(display_info.resolution_horizontal.has_value());
  EXPECT_EQ(static_cast<uint32_t>(display_info.resolution_horizontal.value()),
            kResolutionHorizontal);
  ASSERT_TRUE(display_info.resolution_vertical.has_value());
  EXPECT_EQ(static_cast<uint32_t>(display_info.resolution_vertical.value()),
            kResolutionVertical);
  ASSERT_TRUE(display_info.refresh_rate.has_value());
  EXPECT_EQ(static_cast<double>(display_info.refresh_rate.value()),
            kRefreshRate);
  EXPECT_EQ(display_info.manufacturer, kManufacturer);
  ASSERT_TRUE(display_info.model_id.has_value());
  EXPECT_EQ(static_cast<uint16_t>(display_info.model_id.value()), kModelId);
  // serial_number is not converted in ConvertPtr() for now.
  EXPECT_FALSE(display_info.serial_number);
  ASSERT_TRUE(display_info.manufacture_week.has_value());
  EXPECT_EQ(static_cast<uint8_t>(display_info.manufacture_week.value()),
            kManufactureWeek);
  ASSERT_TRUE(display_info.manufacture_year.has_value());
  EXPECT_EQ(static_cast<uint16_t>(display_info.manufacture_year.value()),
            kManufactureYear);
  EXPECT_EQ(display_info.edid_version, kEdidVersion);
  EXPECT_EQ(display_info.input_type, Convert(kInputType));
  EXPECT_EQ(display_info.display_name, kDisplayName);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertSdCardEventInfo) {
  auto input = ash::cros_healthd::mojom::SdCardEventInfo::New();
  input->state = ash::cros_healthd::mojom::SdCardEventInfo::State::kAdd;

  auto result = ConvertStructPtr(std::move(input));

  EXPECT_EQ(result.event, cx_events::SdCardEvent::kConnected);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertPowerEventInfo) {
  auto input = ash::cros_healthd::mojom::PowerEventInfo::New();
  input->state = ash::cros_healthd::mojom::PowerEventInfo::State::kAcInserted;

  auto result = ConvertStructPtr(std::move(input));

  EXPECT_EQ(result.event, cx_events::PowerEvent::kAcInserted);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest,
     ConvertStylusGarageEventInfo) {
  auto input = ash::cros_healthd::mojom::StylusGarageEventInfo::New();
  input->state =
      ash::cros_healthd::mojom::StylusGarageEventInfo::State::kInserted;

  auto result = ConvertStructPtr(std::move(input));

  EXPECT_EQ(result.event, cx_events::StylusGarageEvent::kInserted);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest,
     ConvertTouchpadEventInfoButtonEvent) {
  auto button_event = ash::cros_healthd::mojom::TouchpadButtonEvent::New();
  button_event->pressed = true;
  button_event->button = ash::cros_healthd::mojom::InputTouchButton::kLeft;

  auto result = ConvertStructPtr(std::move(button_event));

  EXPECT_EQ(result.state, cx_events::InputTouchButtonState::kPressed);
  EXPECT_EQ(result.button, cx_events::InputTouchButton::kLeft);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest,
     ConvertTouchpadEventInfoTouchEvent) {
  constexpr int32_t kTrackingId1 = 1;
  constexpr int32_t kX1 = 2;
  constexpr int32_t kY1 = 3;
  constexpr int32_t kPressure1 = 4;
  constexpr int32_t kTouchMajor1 = 5;
  constexpr int32_t kTouchMinor1 = 6;
  constexpr int32_t kTrackingId2 = 7;
  constexpr int32_t kX2 = 8;
  constexpr int32_t kY2 = 9;

  std::vector<ash::cros_healthd::mojom::TouchPointInfoPtr> touch_points;
  touch_points.push_back(ash::cros_healthd::mojom::TouchPointInfo::New(
      kTrackingId1, kX1, kY1,
      ash::cros_healthd::mojom::NullableUint32::New(kPressure1),
      ash::cros_healthd::mojom::NullableUint32::New(kTouchMajor1),
      ash::cros_healthd::mojom::NullableUint32::New(kTouchMinor1)));
  touch_points.push_back(ash::cros_healthd::mojom::TouchPointInfo::New(
      kTrackingId2, kX2, kY2, nullptr, nullptr, nullptr));

  auto touch_event = ash::cros_healthd::mojom::TouchpadTouchEvent::New(
      std::move(touch_points));

  auto result = ConvertStructPtr(std::move(touch_event));

  EXPECT_EQ(result.touch_points.size(), static_cast<size_t>(2));

  EXPECT_EQ(result.touch_points[0].tracking_id, kTrackingId1);
  EXPECT_EQ(result.touch_points[0].x, kX1);
  EXPECT_EQ(result.touch_points[0].y, kY1);
  EXPECT_EQ(result.touch_points[0].pressure, kPressure1);
  EXPECT_EQ(result.touch_points[0].touch_major, kTouchMajor1);
  EXPECT_EQ(result.touch_points[0].touch_minor, kTouchMinor1);

  EXPECT_EQ(result.touch_points[1].tracking_id, kTrackingId2);
  EXPECT_EQ(result.touch_points[1].x, kX2);
  EXPECT_EQ(result.touch_points[1].y, kY2);
  EXPECT_EQ(result.touch_points[1].pressure, std::nullopt);
  EXPECT_EQ(result.touch_points[1].touch_major, std::nullopt);
  EXPECT_EQ(result.touch_points[1].touch_minor, std::nullopt);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest,
     ConvertTouchpadEventInfoConnectedEvent) {
  constexpr int32_t kMaxX = 1;
  constexpr int32_t kMaxY = 2;
  constexpr int32_t kMaxPressure = 3;

  std::vector<ash::cros_healthd::mojom::InputTouchButton> buttons{
      ash::cros_healthd::mojom::InputTouchButton::kLeft,
      ash::cros_healthd::mojom::InputTouchButton::kMiddle,
      ash::cros_healthd::mojom::InputTouchButton::kRight};

  auto connected_event = ash::cros_healthd::mojom::TouchpadConnectedEvent::New(
      kMaxX, kMaxY, kMaxPressure, std::move(buttons));

  auto result = ConvertStructPtr(std::move(connected_event));

  EXPECT_EQ(result.max_x, kMaxX);
  EXPECT_EQ(result.max_y, kMaxY);
  EXPECT_EQ(result.max_pressure, kMaxPressure);

  EXPECT_EQ(result.buttons.size(), static_cast<size_t>(3));
  EXPECT_EQ(result.buttons[0], cx_events::InputTouchButton::kLeft);
  EXPECT_EQ(result.buttons[1], cx_events::InputTouchButton::kMiddle);
  EXPECT_EQ(result.buttons[2], cx_events::InputTouchButton::kRight);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest,
     ConvertTouchscreenEventInfoTouchEvent) {
  constexpr int32_t kTrackingId1 = 1;
  constexpr int32_t kX1 = 2;
  constexpr int32_t kY1 = 3;
  constexpr int32_t kPressure1 = 4;
  constexpr int32_t kTouchMajor1 = 5;
  constexpr int32_t kTouchMinor1 = 6;
  constexpr int32_t kTrackingId2 = 7;
  constexpr int32_t kX2 = 8;
  constexpr int32_t kY2 = 9;

  std::vector<ash::cros_healthd::mojom::TouchPointInfoPtr> touch_points;
  touch_points.push_back(ash::cros_healthd::mojom::TouchPointInfo::New(
      kTrackingId1, kX1, kY1,
      ash::cros_healthd::mojom::NullableUint32::New(kPressure1),
      ash::cros_healthd::mojom::NullableUint32::New(kTouchMajor1),
      ash::cros_healthd::mojom::NullableUint32::New(kTouchMinor1)));
  touch_points.push_back(ash::cros_healthd::mojom::TouchPointInfo::New(
      kTrackingId2, kX2, kY2, nullptr, nullptr, nullptr));

  auto touch_event = ash::cros_healthd::mojom::TouchscreenTouchEvent::New(
      std::move(touch_points));

  auto result = ConvertStructPtr(std::move(touch_event));

  EXPECT_EQ(result.touch_points.size(), static_cast<size_t>(2));

  EXPECT_EQ(result.touch_points[0].tracking_id, kTrackingId1);
  EXPECT_EQ(result.touch_points[0].x, kX1);
  EXPECT_EQ(result.touch_points[0].y, kY1);
  EXPECT_EQ(result.touch_points[0].pressure, kPressure1);
  EXPECT_EQ(result.touch_points[0].touch_major, kTouchMajor1);
  EXPECT_EQ(result.touch_points[0].touch_minor, kTouchMinor1);

  EXPECT_EQ(result.touch_points[1].tracking_id, kTrackingId2);
  EXPECT_EQ(result.touch_points[1].x, kX2);
  EXPECT_EQ(result.touch_points[1].y, kY2);
  EXPECT_EQ(result.touch_points[1].pressure, std::nullopt);
  EXPECT_EQ(result.touch_points[1].touch_major, std::nullopt);
  EXPECT_EQ(result.touch_points[1].touch_minor, std::nullopt);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest,
     ConvertTouchscreenEventInfoConnectedEvent) {
  constexpr int32_t kMaxX = 1;
  constexpr int32_t kMaxY = 2;
  constexpr int32_t kMaxPressure = 3;

  auto connected_event =
      ash::cros_healthd::mojom::TouchscreenConnectedEvent::New(kMaxX, kMaxY,
                                                               kMaxPressure);

  auto result = ConvertStructPtr(std::move(connected_event));

  EXPECT_EQ(result.max_x, kMaxX);
  EXPECT_EQ(result.max_y, kMaxY);
  EXPECT_EQ(result.max_pressure, kMaxPressure);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertNullableInt) {
  auto output =
      ConvertStructPtr(ash::cros_healthd::mojom::NullableUint32::New(10));
  EXPECT_EQ(output, uint32_t{10});
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertTouchpointInfo) {
  constexpr int32_t kTrackingId = 1;
  constexpr int32_t kX = 2;
  constexpr int32_t kY = 3;
  constexpr int32_t kPressure = 4;
  constexpr int32_t kTouchMajor = 5;

  auto output = ConvertStructPtr(ash::cros_healthd::mojom::TouchPointInfo::New(
      kTrackingId, kX, kY,
      ash::cros_healthd::mojom::NullableUint32::New(kPressure),
      ash::cros_healthd::mojom::NullableUint32::New(kTouchMajor), nullptr));

  EXPECT_EQ(output.tracking_id, kTrackingId);
  EXPECT_EQ(output.x, kX);
  EXPECT_EQ(output.y, kY);
  EXPECT_EQ(output.pressure, kPressure);
  EXPECT_EQ(output.touch_major, kTouchMajor);
  EXPECT_EQ(output.touch_minor, std::nullopt);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest,
     ConvertStylusTouchEventInfo) {
  constexpr int32_t kX = 1;
  constexpr int32_t kY = 2;
  constexpr int32_t kPressure = 3;

  auto touch_event = ash::cros_healthd::mojom::StylusTouchEvent::New(
      ash::cros_healthd::mojom::StylusTouchPointInfo::New(
          kX, kY, ash::cros_healthd::mojom::NullableUint32::New(kPressure)));

  auto result = ConvertStructPtr(std::move(touch_event));

  EXPECT_EQ(result.touch_point->x, kX);
  EXPECT_EQ(result.touch_point->y, kY);
  EXPECT_EQ(result.touch_point->pressure, kPressure);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest,
     ConvertStylusConnectedEventInfo) {
  constexpr int32_t kMaxX = 1;
  constexpr int32_t kMaxY = 2;
  constexpr int32_t kMaxPressure = 3;

  auto connected_event = ash::cros_healthd::mojom::StylusConnectedEvent::New(
      kMaxX, kMaxY, kMaxPressure);

  auto result = ConvertStructPtr(std::move(connected_event));

  EXPECT_EQ(result.max_x, kMaxX);
  EXPECT_EQ(result.max_y, kMaxY);
  EXPECT_EQ(result.max_pressure, kMaxPressure);
}

}  // namespace chromeos::converters::events

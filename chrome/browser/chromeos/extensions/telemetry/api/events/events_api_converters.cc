// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api_converters.h"

#include <cstdint>
#include <optional>

#include "base/notreached.h"
#include "chrome/common/chromeos/extensions/api/events.h"

namespace chromeos::converters::events {

namespace {

namespace cx_events = ::chromeos::api::os_events;

}  // namespace

namespace unchecked {

cx_events::AudioJackEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::AudioJackEventInfoPtr ptr) {
  cx_events::AudioJackEventInfo result;

  result.event = Convert(ptr->state);
  result.device_type = Convert(ptr->device_type);

  return result;
}

cx_events::KeyboardInfo UncheckedConvertPtr(
    ash::diagnostics::mojom::KeyboardInfoPtr ptr) {
  cx_events::KeyboardInfo result;

  result.id = ptr->id;
  result.connection_type = Convert(ptr->connection_type);
  result.name = std::move(ptr->name);
  result.physical_layout = Convert(ptr->physical_layout);
  result.mechanical_layout = Convert(ptr->mechanical_layout);
  result.region_code = std::move(ptr->region_code);
  result.number_pad_present = Convert(ptr->number_pad_present);
  result.top_row_keys = ConvertVector(ptr->top_row_keys);
  result.top_right_key = Convert(ptr->top_right_key);
  result.has_assistant_key = ptr->has_assistant_key;

  return result;
}

cx_events::KeyboardDiagnosticEventInfo UncheckedConvertPtr(
    ash::diagnostics::mojom::KeyboardDiagnosticEventInfoPtr ptr) {
  cx_events::KeyboardDiagnosticEventInfo result;

  result.keyboard_info = ConvertStructPtr(std::move(ptr->keyboard_info));
  result.tested_keys = ConvertVector(ptr->tested_keys);
  result.tested_top_row_keys = ConvertVector(ptr->tested_top_row_keys);

  return result;
}

cx_events::LidEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::LidEventInfoPtr ptr) {
  cx_events::LidEventInfo result;

  result.event = Convert(ptr->state);

  return result;
}

cx_events::UsbEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::UsbEventInfoPtr ptr) {
  cx_events::UsbEventInfo result;

  result.event = Convert(ptr->state);
  result.vendor = ptr->vendor;
  result.name = ptr->name;
  result.vid = ptr->vid;
  result.pid = ptr->pid;
  result.categories = ptr->categories;

  return result;
}

cx_events::ExternalDisplayEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::ExternalDisplayEventInfoPtr ptr) {
  cx_events::ExternalDisplayEventInfo result;

  result.event = Convert(ptr->state);
  result.display_info = ConvertStructPtr(std::move(ptr->display_info));

  return result;
}

cx_events::ExternalDisplayInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::ExternalDisplayInfoPtr input) {
  cx_events::ExternalDisplayInfo result;

  if (input->display_width) {
    result.display_width = input->display_width->value;
  }
  if (input->display_height) {
    result.display_height = input->display_height->value;
  }
  if (input->resolution_horizontal) {
    result.resolution_horizontal = input->resolution_horizontal->value;
  }
  if (input->resolution_vertical) {
    result.resolution_vertical = input->resolution_vertical->value;
  }
  if (input->refresh_rate) {
    result.refresh_rate = input->refresh_rate->value;
  }
  if (input->manufacturer) {
    result.manufacturer = std::move(*input->manufacturer);
  }
  if (input->model_id) {
    result.model_id = input->model_id->value;
  }
  // Not reporting serial_number for now until we get Privacy's approval.
  // result.serial_number = std::move(input->serial_number);
  if (input->manufacture_week) {
    result.manufacture_week = input->manufacture_week->value;
  }
  if (input->manufacture_year) {
    result.manufacture_year = input->manufacture_year->value;
  }
  if (input->edid_version) {
    result.edid_version = std::move(*input->edid_version);
  }
  result.input_type = Convert(input->input_type);
  if (input->display_name) {
    result.display_name = std::move(*input->display_name);
  }

  return result;
}

cx_events::SdCardEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::SdCardEventInfoPtr ptr) {
  cx_events::SdCardEventInfo result;

  result.event = Convert(ptr->state);

  return result;
}

cx_events::PowerEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::PowerEventInfoPtr ptr) {
  cx_events::PowerEventInfo result;

  result.event = Convert(ptr->state);

  return result;
}

cx_events::StylusGarageEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::StylusGarageEventInfoPtr ptr) {
  cx_events::StylusGarageEventInfo result;

  result.event = Convert(ptr->state);

  return result;
}

std::optional<uint32_t> UncheckedConvertPtr(
    ash::cros_healthd::mojom::NullableUint32Ptr ptr) {
  return ptr->value;
}

cx_events::TouchpadButtonEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::TouchpadButtonEventPtr ptr) {
  cx_events::TouchpadButtonEventInfo result;
  result.button = Convert(ptr->button);
  result.state = ptr->pressed ? cx_events::InputTouchButtonState::kPressed
                              : cx_events::InputTouchButtonState::kReleased;
  return result;
}

cx_events::TouchpadTouchEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::TouchpadTouchEventPtr ptr) {
  cx_events::TouchpadTouchEventInfo result;
  std::vector<cx_events::TouchPointInfo> converted_touch_points =
      ConvertStructPtrVector<cx_events::TouchPointInfo>(
          std::move(ptr->touch_points));
  result.touch_points = std::move(converted_touch_points);
  return result;
}

cx_events::TouchpadConnectedEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::TouchpadConnectedEventPtr ptr) {
  cx_events::TouchpadConnectedEventInfo result;
  std::vector<cx_events::InputTouchButton> converted_buttons =
      ConvertVector(std::move(ptr->buttons));
  result.buttons = std::move(converted_buttons);
  result.max_x = ptr->max_x;
  result.max_y = ptr->max_y;
  result.max_pressure = ptr->max_pressure;
  return result;
}

cx_events::TouchscreenTouchEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::TouchscreenTouchEventPtr ptr) {
  cx_events::TouchscreenTouchEventInfo result;
  std::vector<cx_events::TouchPointInfo> converted_touch_points =
      ConvertStructPtrVector<cx_events::TouchPointInfo>(
          std::move(ptr->touch_points));
  result.touch_points = std::move(converted_touch_points);
  return result;
}

cx_events::TouchscreenConnectedEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::TouchscreenConnectedEventPtr ptr) {
  cx_events::TouchscreenConnectedEventInfo result;
  result.max_x = ptr->max_x;
  result.max_y = ptr->max_y;
  result.max_pressure = ptr->max_pressure;
  return result;
}

cx_events::TouchPointInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::TouchPointInfoPtr ptr) {
  cx_events::TouchPointInfo result;
  result.tracking_id = ptr->tracking_id;
  result.x = ptr->x;
  result.y = ptr->y;
  result.pressure = ConvertStructPtr(std::move(ptr->pressure));
  result.touch_major = ConvertStructPtr(std::move(ptr->touch_major));
  result.touch_minor = ConvertStructPtr(std::move(ptr->touch_minor));
  return result;
}

cx_events::StylusTouchPointInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::StylusTouchPointInfoPtr ptr) {
  cx_events::StylusTouchPointInfo result;
  if (ptr.is_null()) {
    return result;
  }
  result.x = ptr->x;
  result.y = ptr->y;
  result.pressure = ConvertStructPtr(std::move(ptr->pressure));
  return result;
}

cx_events::StylusTouchEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::StylusTouchEventPtr ptr) {
  cx_events::StylusTouchEventInfo result;
  result.touch_point = ConvertStructPtr(std::move(ptr->touch_point));
  return result;
}

cx_events::StylusConnectedEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::StylusConnectedEventPtr ptr) {
  cx_events::StylusConnectedEventInfo result;
  result.max_x = ptr->max_x;
  result.max_y = ptr->max_y;
  result.max_pressure = ptr->max_pressure;
  return result;
}

}  // namespace unchecked

cx_events::AudioJackEvent Convert(
    ash::cros_healthd::mojom::AudioJackEventInfo::State state) {
  switch (state) {
    case ash::cros_healthd::mojom::AudioJackEventInfo_State::kUnmappedEnumField:
      return cx_events::AudioJackEvent::kNone;
    case ash::cros_healthd::mojom::AudioJackEventInfo_State::kAdd:
      return cx_events::AudioJackEvent::kConnected;
    case ash::cros_healthd::mojom::AudioJackEventInfo_State::kRemove:
      return cx_events::AudioJackEvent::kDisconnected;
  }
  NOTREACHED();
}

cx_events::AudioJackDeviceType Convert(
    ash::cros_healthd::mojom::AudioJackEventInfo::DeviceType device_type) {
  switch (device_type) {
    case ash::cros_healthd::mojom::AudioJackEventInfo_DeviceType::
        kUnmappedEnumField:
      return cx_events::AudioJackDeviceType::kNone;
    case ash::cros_healthd::mojom::AudioJackEventInfo_DeviceType::kHeadphone:
      return cx_events::AudioJackDeviceType::kHeadphone;
    case ash::cros_healthd::mojom::AudioJackEventInfo_DeviceType::kMicrophone:
      return cx_events::AudioJackDeviceType::kMicrophone;
  }
  NOTREACHED();
}

cx_events::KeyboardConnectionType Convert(
    ash::diagnostics::mojom::ConnectionType input) {
  switch (input) {
    case ash::diagnostics::mojom::ConnectionType::kUnmappedEnumField:
      return cx_events::KeyboardConnectionType::kNone;
    case ash::diagnostics::mojom::ConnectionType::kInternal:
      return cx_events::KeyboardConnectionType::kInternal;
    case ash::diagnostics::mojom::ConnectionType::kUsb:
      return cx_events::KeyboardConnectionType::kUsb;
    case ash::diagnostics::mojom::ConnectionType::kBluetooth:
      return cx_events::KeyboardConnectionType::kBluetooth;
    case ash::diagnostics::mojom::ConnectionType::kUnknown:
      return cx_events::KeyboardConnectionType::kUnknown;
  }
  NOTREACHED();
}

cx_events::PhysicalKeyboardLayout Convert(
    ash::diagnostics::mojom::PhysicalLayout input) {
  switch (input) {
    case ash::diagnostics::mojom::PhysicalLayout::kUnmappedEnumField:
      return cx_events::PhysicalKeyboardLayout::kNone;
    case ash::diagnostics::mojom::PhysicalLayout::kUnknown:
    case ash::diagnostics::mojom::PhysicalLayout::kChromeOSDellEnterpriseWilco:
    case ash::diagnostics::mojom::PhysicalLayout::
        kChromeOSDellEnterpriseDrallion:
      return cx_events::PhysicalKeyboardLayout::kUnknown;
    case ash::diagnostics::mojom::PhysicalLayout::kChromeOS:
      return cx_events::PhysicalKeyboardLayout::kChromeOs;
  }
  NOTREACHED();
}

cx_events::MechanicalKeyboardLayout Convert(
    ash::diagnostics::mojom::MechanicalLayout input) {
  switch (input) {
    case ash::diagnostics::mojom::MechanicalLayout::kUnmappedEnumField:
      return cx_events::MechanicalKeyboardLayout::kNone;
    case ash::diagnostics::mojom::MechanicalLayout::kUnknown:
      return cx_events::MechanicalKeyboardLayout::kUnknown;
    case ash::diagnostics::mojom::MechanicalLayout::kAnsi:
      return cx_events::MechanicalKeyboardLayout::kAnsi;
    case ash::diagnostics::mojom::MechanicalLayout::kIso:
      return cx_events::MechanicalKeyboardLayout::kIso;
    case ash::diagnostics::mojom::MechanicalLayout::kJis:
      return cx_events::MechanicalKeyboardLayout::kJis;
  }
  NOTREACHED();
}

cx_events::KeyboardNumberPadPresence Convert(
    ash::diagnostics::mojom::NumberPadPresence input) {
  switch (input) {
    case ash::diagnostics::mojom::NumberPadPresence::kUnmappedEnumField:
      return cx_events::KeyboardNumberPadPresence::kNone;
    case ash::diagnostics::mojom::NumberPadPresence::kUnknown:
      return cx_events::KeyboardNumberPadPresence::kUnknown;
    case ash::diagnostics::mojom::NumberPadPresence::kPresent:
      return cx_events::KeyboardNumberPadPresence::kPresent;
    case ash::diagnostics::mojom::NumberPadPresence::kNotPresent:
      return cx_events::KeyboardNumberPadPresence::kNotPresent;
  }
  NOTREACHED();
}

cx_events::KeyboardTopRowKey Convert(ash::diagnostics::mojom::TopRowKey input) {
  switch (input) {
    case ash::diagnostics::mojom::TopRowKey::kUnmappedEnumField:
      return cx_events::KeyboardTopRowKey::kNone;
    case ash::diagnostics::mojom::TopRowKey::kNone:
      return cx_events::KeyboardTopRowKey::kNoKey;
    case ash::diagnostics::mojom::TopRowKey::kUnknown:
    case ash::diagnostics::mojom::TopRowKey::kAccessibility:
    case ash::diagnostics::mojom::TopRowKey::kDictation:
      return cx_events::KeyboardTopRowKey::kUnknown;
    case ash::diagnostics::mojom::TopRowKey::kBack:
      return cx_events::KeyboardTopRowKey::kBack;
    case ash::diagnostics::mojom::TopRowKey::kForward:
      return cx_events::KeyboardTopRowKey::kForward;
    case ash::diagnostics::mojom::TopRowKey::kRefresh:
      return cx_events::KeyboardTopRowKey::kRefresh;
    case ash::diagnostics::mojom::TopRowKey::kFullscreen:
      return cx_events::KeyboardTopRowKey::kFullscreen;
    case ash::diagnostics::mojom::TopRowKey::kOverview:
      return cx_events::KeyboardTopRowKey::kOverview;
    case ash::diagnostics::mojom::TopRowKey::kScreenshot:
      return cx_events::KeyboardTopRowKey::kScreenshot;
    case ash::diagnostics::mojom::TopRowKey::kScreenBrightnessDown:
      return cx_events::KeyboardTopRowKey::kScreenBrightnessDown;
    case ash::diagnostics::mojom::TopRowKey::kScreenBrightnessUp:
      return cx_events::KeyboardTopRowKey::kScreenBrightnessUp;
    case ash::diagnostics::mojom::TopRowKey::kPrivacyScreenToggle:
      return cx_events::KeyboardTopRowKey::kPrivacyScreenToggle;
    case ash::diagnostics::mojom::TopRowKey::kMicrophoneMute:
      return cx_events::KeyboardTopRowKey::kMicrophoneMute;
    case ash::diagnostics::mojom::TopRowKey::kVolumeMute:
      return cx_events::KeyboardTopRowKey::kVolumeMute;
    case ash::diagnostics::mojom::TopRowKey::kVolumeDown:
      return cx_events::KeyboardTopRowKey::kVolumeDown;
    case ash::diagnostics::mojom::TopRowKey::kVolumeUp:
      return cx_events::KeyboardTopRowKey::kVolumeUp;
    case ash::diagnostics::mojom::TopRowKey::kKeyboardBacklightToggle:
      return cx_events::KeyboardTopRowKey::kKeyboardBacklightToggle;
    case ash::diagnostics::mojom::TopRowKey::kKeyboardBacklightDown:
      return cx_events::KeyboardTopRowKey::kKeyboardBacklightDown;
    case ash::diagnostics::mojom::TopRowKey::kKeyboardBacklightUp:
      return cx_events::KeyboardTopRowKey::kKeyboardBacklightUp;
    case ash::diagnostics::mojom::TopRowKey::kNextTrack:
      return cx_events::KeyboardTopRowKey::kNextTrack;
    case ash::diagnostics::mojom::TopRowKey::kPreviousTrack:
      return cx_events::KeyboardTopRowKey::kPreviousTrack;
    case ash::diagnostics::mojom::TopRowKey::kPlayPause:
      return cx_events::KeyboardTopRowKey::kPlayPause;
    case ash::diagnostics::mojom::TopRowKey::kScreenMirror:
      return cx_events::KeyboardTopRowKey::kScreenMirror;
    case ash::diagnostics::mojom::TopRowKey::kDelete:
      return cx_events::KeyboardTopRowKey::kDelete;
  }
  NOTREACHED();
}

cx_events::KeyboardTopRightKey Convert(
    ash::diagnostics::mojom::TopRightKey input) {
  switch (input) {
    case ash::diagnostics::mojom::TopRightKey::kUnmappedEnumField:
      return cx_events::KeyboardTopRightKey::kNone;
    case ash::diagnostics::mojom::TopRightKey::kUnknown:
      return cx_events::KeyboardTopRightKey::kUnknown;
    case ash::diagnostics::mojom::TopRightKey::kPower:
      return cx_events::KeyboardTopRightKey::kPower;
    case ash::diagnostics::mojom::TopRightKey::kLock:
      return cx_events::KeyboardTopRightKey::kLock;
    case ash::diagnostics::mojom::TopRightKey::kControlPanel:
      return cx_events::KeyboardTopRightKey::kControlPanel;
  }
  NOTREACHED();
}

cx_events::LidEvent Convert(
    ash::cros_healthd::mojom::LidEventInfo::State state) {
  switch (state) {
    case ash::cros_healthd::mojom::LidEventInfo_State::kUnmappedEnumField:
      return cx_events::LidEvent::kNone;
    case ash::cros_healthd::mojom::LidEventInfo_State::kClosed:
      return cx_events::LidEvent::kClosed;
    case ash::cros_healthd::mojom::LidEventInfo_State::kOpened:
      return cx_events::LidEvent::kOpened;
  }
  NOTREACHED();
}

cx_events::UsbEvent Convert(
    ash::cros_healthd::mojom::UsbEventInfo::State state) {
  switch (state) {
    case ash::cros_healthd::mojom::UsbEventInfo_State::kUnmappedEnumField:
      return cx_events::UsbEvent::kNone;
    case ash::cros_healthd::mojom::UsbEventInfo_State::kAdd:
      return cx_events::UsbEvent::kConnected;
    case ash::cros_healthd::mojom::UsbEventInfo_State::kRemove:
      return cx_events::UsbEvent::kDisconnected;
  }
  NOTREACHED();
}

cx_events::ExternalDisplayEvent Convert(
    ash::cros_healthd::mojom::ExternalDisplayEventInfo::State state) {
  switch (state) {
    case ash::cros_healthd::mojom::ExternalDisplayEventInfo_State::
        kUnmappedEnumField:
      return cx_events::ExternalDisplayEvent::kNone;
    case ash::cros_healthd::mojom::ExternalDisplayEventInfo_State::kAdd:
      return cx_events::ExternalDisplayEvent::kConnected;
    case ash::cros_healthd::mojom::ExternalDisplayEventInfo_State::kRemove:
      return cx_events::ExternalDisplayEvent::kDisconnected;
  }
  NOTREACHED();
}

cx_events::SdCardEvent Convert(
    ash::cros_healthd::mojom::SdCardEventInfo::State state) {
  switch (state) {
    case ash::cros_healthd::mojom::SdCardEventInfo_State::kUnmappedEnumField:
      return cx_events::SdCardEvent::kNone;
    case ash::cros_healthd::mojom::SdCardEventInfo_State::kAdd:
      return cx_events::SdCardEvent::kConnected;
    case ash::cros_healthd::mojom::SdCardEventInfo_State::kRemove:
      return cx_events::SdCardEvent::kDisconnected;
  }
  NOTREACHED();
}

cx_events::PowerEvent Convert(
    ash::cros_healthd::mojom::PowerEventInfo::State state) {
  switch (state) {
    case ash::cros_healthd::mojom::PowerEventInfo_State::kUnmappedEnumField:
      return cx_events::PowerEvent::kNone;
    case ash::cros_healthd::mojom::PowerEventInfo_State::kAcInserted:
      return cx_events::PowerEvent::kAcInserted;
    case ash::cros_healthd::mojom::PowerEventInfo_State::kAcRemoved:
      return cx_events::PowerEvent::kAcRemoved;
    case ash::cros_healthd::mojom::PowerEventInfo_State::kOsSuspend:
      return cx_events::PowerEvent::kOsSuspend;
    case ash::cros_healthd::mojom::PowerEventInfo_State::kOsResume:
      return cx_events::PowerEvent::kOsResume;
  }
  NOTREACHED();
}

cx_events::StylusGarageEvent Convert(
    ash::cros_healthd::mojom::StylusGarageEventInfo::State state) {
  switch (state) {
    case ash::cros_healthd::mojom::StylusGarageEventInfo_State::
        kUnmappedEnumField:
      return cx_events::StylusGarageEvent::kNone;
    case ash::cros_healthd::mojom::StylusGarageEventInfo_State::kInserted:
      return cx_events::StylusGarageEvent::kInserted;
    case ash::cros_healthd::mojom::StylusGarageEventInfo_State::kRemoved:
      return cx_events::StylusGarageEvent::kRemoved;
  }
  NOTREACHED();
}

cx_events::InputTouchButton Convert(
    ash::cros_healthd::mojom::InputTouchButton button) {
  switch (button) {
    case ash::cros_healthd::mojom::InputTouchButton::kUnmappedEnumField:
      return cx_events::InputTouchButton::kNone;
    case ash::cros_healthd::mojom::InputTouchButton::kLeft:
      return cx_events::InputTouchButton::kLeft;
    case ash::cros_healthd::mojom::InputTouchButton::kMiddle:
      return cx_events::InputTouchButton::kMiddle;
    case ash::cros_healthd::mojom::InputTouchButton::kRight:
      return cx_events::InputTouchButton::kRight;
  }
  NOTREACHED();
}

ash::cros_healthd::mojom::EventCategoryEnum Convert(
    cx_events::EventCategory input) {
  switch (input) {
    case cx_events::EventCategory::kNone:
      return ash::cros_healthd::mojom::EventCategoryEnum::kUnmappedEnumField;
    case cx_events::EventCategory::kAudioJack:
      return ash::cros_healthd::mojom::EventCategoryEnum::kAudioJack;
    case cx_events::EventCategory::kLid:
      return ash::cros_healthd::mojom::EventCategoryEnum::kLid;
    case cx_events::EventCategory::kUsb:
      return ash::cros_healthd::mojom::EventCategoryEnum::kUsb;
    case cx_events::EventCategory::kExternalDisplay:
      return ash::cros_healthd::mojom::EventCategoryEnum::kExternalDisplay;
    case cx_events::EventCategory::kSdCard:
      return ash::cros_healthd::mojom::EventCategoryEnum::kSdCard;
    case cx_events::EventCategory::kPower:
      return ash::cros_healthd::mojom::EventCategoryEnum::kPower;
    case cx_events::EventCategory::kKeyboardDiagnostic:
      return ash::cros_healthd::mojom::EventCategoryEnum::kKeyboardDiagnostic;
    case cx_events::EventCategory::kStylusGarage:
      return ash::cros_healthd::mojom::EventCategoryEnum::kStylusGarage;
    case cx_events::EventCategory::kTouchpadButton:
      return ash::cros_healthd::mojom::EventCategoryEnum::kTouchpad;
    case cx_events::EventCategory::kTouchpadTouch:
      return ash::cros_healthd::mojom::EventCategoryEnum::kTouchpad;
    case cx_events::EventCategory::kTouchpadConnected:
      return ash::cros_healthd::mojom::EventCategoryEnum::kTouchpad;
    case cx_events::EventCategory::kTouchscreenTouch:
      return ash::cros_healthd::mojom::EventCategoryEnum::kTouchscreen;
    case cx_events::EventCategory::kTouchscreenConnected:
      return ash::cros_healthd::mojom::EventCategoryEnum::kTouchscreen;
    case cx_events::EventCategory::kStylusTouch:
      return ash::cros_healthd::mojom::EventCategoryEnum::kStylus;
    case cx_events::EventCategory::kStylusConnected:
      return ash::cros_healthd::mojom::EventCategoryEnum::kStylus;
  }
  NOTREACHED();
}

cx_events::DisplayInputType Convert(
    ash::cros_healthd::mojom::DisplayInputType input) {
  switch (input) {
    case ash::cros_healthd::mojom::DisplayInputType::kUnmappedEnumField:
      return cx_events::DisplayInputType::kUnknown;
    case ash::cros_healthd::mojom::DisplayInputType::kDigital:
      return cx_events::DisplayInputType::kDigital;
    case ash::cros_healthd::mojom::DisplayInputType::kAnalog:
      return cx_events::DisplayInputType::kAnalog;
  }
  NOTREACHED();
}

int Convert(uint32_t input) {
  return static_cast<int>(input);
}

}  // namespace chromeos::converters::events

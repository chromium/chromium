// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api_converters.h"

#include <cstdint>
#include <optional>

#include "base/notreached.h"
#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_keyboard_event.mojom.h"

namespace chromeos::converters::events {

namespace {

namespace cx_events = ::chromeos::api::os_events;
namespace crosapi = ::crosapi::mojom;

}  // namespace

namespace unchecked {

cx_events::AudioJackEventInfo UncheckedConvertPtr(
    crosapi::TelemetryAudioJackEventInfoPtr ptr) {
  cx_events::AudioJackEventInfo result;

  result.event = Convert(ptr->state);
  result.device_type = Convert(ptr->device_type);

  return result;
}

cx_events::KeyboardInfo UncheckedConvertPtr(
    crosapi::TelemetryKeyboardInfoPtr ptr) {
  cx_events::KeyboardInfo result;

  result.id = ConvertStructPtr(std::move(ptr->id));
  result.connection_type = Convert(ptr->connection_type);
  result.name = std::move(ptr->name);
  result.physical_layout = Convert(ptr->physical_layout);
  result.mechanical_layout = Convert(ptr->mechanical_layout);
  result.region_code = std::move(ptr->region_code);
  result.number_pad_present = Convert(ptr->number_pad_present);
  if (ptr->top_row_keys) {
    result.top_row_keys = ConvertVector(ptr->top_row_keys.value());
  }
  result.top_right_key = Convert(ptr->top_right_key);
  if (ptr->has_assistant_key) {
    result.has_assistant_key = ptr->has_assistant_key->value;
  }

  return result;
}

cx_events::KeyboardDiagnosticEventInfo UncheckedConvertPtr(
    crosapi::TelemetryKeyboardDiagnosticEventInfoPtr ptr) {
  cx_events::KeyboardDiagnosticEventInfo result;

  result.keyboard_info = ConvertStructPtr(std::move(ptr->keyboard_info));

  if (ptr->tested_keys) {
    result.tested_keys = ConvertVector(ptr->tested_keys.value());
  }

  if (ptr->tested_top_row_keys) {
    result.tested_top_row_keys =
        ConvertVector(ptr->tested_top_row_keys.value());
  }

  return result;
}

cx_events::LidEventInfo UncheckedConvertPtr(
    crosapi::TelemetryLidEventInfoPtr ptr) {
  cx_events::LidEventInfo result;

  result.event = Convert(ptr->state);

  return result;
}

cx_events::UsbEventInfo UncheckedConvertPtr(
    crosapi::TelemetryUsbEventInfoPtr ptr) {
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
    crosapi::TelemetryExternalDisplayEventInfoPtr ptr) {
  cx_events::ExternalDisplayEventInfo result;

  result.event = Convert(ptr->state);
  result.display_info = ConvertStructPtr(std::move(ptr->display_info));

  return result;
}

cx_events::ExternalDisplayInfo UncheckedConvertPtr(
    crosapi::ProbeExternalDisplayInfoPtr input) {
  cx_events::ExternalDisplayInfo result;

  result.display_width = std::move(input->display_width);
  result.display_height = std::move(input->display_height);
  result.resolution_horizontal = std::move(input->resolution_horizontal);
  result.resolution_vertical = std::move(input->resolution_vertical);
  result.refresh_rate = std::move(input->refresh_rate);
  result.manufacturer = std::move(input->manufacturer);
  result.model_id = std::move(input->model_id);
  // Not reporting serial_number for now until we get Privacy's approval.
  // result.serial_number = std::move(input->serial_number);
  result.manufacture_week = std::move(input->manufacture_week);
  result.manufacture_year = std::move(input->manufacture_year);
  result.edid_version = std::move(input->edid_version);
  result.input_type = Convert(input->input_type);
  result.display_name = (input->display_name);

  return result;
}

cx_events::SdCardEventInfo UncheckedConvertPtr(
    crosapi::TelemetrySdCardEventInfoPtr ptr) {
  cx_events::SdCardEventInfo result;

  result.event = Convert(ptr->state);

  return result;
}

cx_events::PowerEventInfo UncheckedConvertPtr(
    crosapi::TelemetryPowerEventInfoPtr ptr) {
  cx_events::PowerEventInfo result;

  result.event = Convert(ptr->state);

  return result;
}

cx_events::StylusGarageEventInfo UncheckedConvertPtr(
    crosapi::TelemetryStylusGarageEventInfoPtr ptr) {
  cx_events::StylusGarageEventInfo result;

  result.event = Convert(ptr->state);

  return result;
}

std::optional<uint32_t> UncheckedConvertPtr(crosapi::UInt32ValuePtr ptr) {
  return ptr->value;
}

cx_events::TouchpadButtonEventInfo UncheckedConvertPtr(
    crosapi::TelemetryTouchpadButtonEventInfoPtr ptr) {
  cx_events::TouchpadButtonEventInfo result;
  result.button = Convert(ptr->button);
  result.state = Convert(ptr->state);
  return result;
}

cx_events::TouchpadTouchEventInfo UncheckedConvertPtr(
    crosapi::TelemetryTouchpadTouchEventInfoPtr ptr) {
  cx_events::TouchpadTouchEventInfo result;
  std::vector<cx_events::TouchPointInfo> converted_touch_points =
      ConvertStructPtrVector<cx_events::TouchPointInfo>(
          std::move(ptr->touch_points));
  result.touch_points = std::move(converted_touch_points);
  return result;
}

cx_events::TouchpadConnectedEventInfo UncheckedConvertPtr(
    crosapi::TelemetryTouchpadConnectedEventInfoPtr ptr) {
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
    crosapi::TelemetryTouchscreenTouchEventInfoPtr ptr) {
  cx_events::TouchscreenTouchEventInfo result;
  std::vector<cx_events::TouchPointInfo> converted_touch_points =
      ConvertStructPtrVector<cx_events::TouchPointInfo>(
          std::move(ptr->touch_points));
  result.touch_points = std::move(converted_touch_points);
  return result;
}

cx_events::TouchscreenConnectedEventInfo UncheckedConvertPtr(
    crosapi::TelemetryTouchscreenConnectedEventInfoPtr ptr) {
  cx_events::TouchscreenConnectedEventInfo result;
  result.max_x = ptr->max_x;
  result.max_y = ptr->max_y;
  result.max_pressure = ptr->max_pressure;
  return result;
}

cx_events::TouchPointInfo UncheckedConvertPtr(
    crosapi::TelemetryTouchPointInfoPtr ptr) {
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
    crosapi::TelemetryStylusTouchPointInfoPtr ptr) {
  cx_events::StylusTouchPointInfo result;
  if (ptr.is_null()) {
    return result;
  }
  result.x = ptr->x;
  result.y = ptr->y;
  result.pressure = ptr->pressure;
  return result;
}

cx_events::StylusTouchEventInfo UncheckedConvertPtr(
    crosapi::TelemetryStylusTouchEventInfoPtr ptr) {
  cx_events::StylusTouchEventInfo result;
  result.touch_point = ConvertStructPtr(std::move(ptr->touch_point));
  return result;
}

cx_events::StylusConnectedEventInfo UncheckedConvertPtr(
    crosapi::TelemetryStylusConnectedEventInfoPtr ptr) {
  cx_events::StylusConnectedEventInfo result;
  result.max_x = ptr->max_x;
  result.max_y = ptr->max_y;
  result.max_pressure = ptr->max_pressure;
  return result;
}

}  // namespace unchecked

cx_events::AudioJackEvent Convert(
    crosapi::TelemetryAudioJackEventInfo::State state) {
  switch (state) {
    case crosapi::TelemetryAudioJackEventInfo_State::kUnmappedEnumField:
      return cx_events::AudioJackEvent::kNone;
    case crosapi::TelemetryAudioJackEventInfo_State::kAdd:
      return cx_events::AudioJackEvent::kConnected;
    case crosapi::TelemetryAudioJackEventInfo_State::kRemove:
      return cx_events::AudioJackEvent::kDisconnected;
  }
  NOTREACHED();
}

cx_events::AudioJackDeviceType Convert(
    crosapi::TelemetryAudioJackEventInfo::DeviceType device_type) {
  switch (device_type) {
    case crosapi::TelemetryAudioJackEventInfo_DeviceType::kUnmappedEnumField:
      return cx_events::AudioJackDeviceType::kNone;
    case crosapi::TelemetryAudioJackEventInfo_DeviceType::kHeadphone:
      return cx_events::AudioJackDeviceType::kHeadphone;
    case crosapi::TelemetryAudioJackEventInfo_DeviceType::kMicrophone:
      return cx_events::AudioJackDeviceType::kMicrophone;
  }
  NOTREACHED();
}

cx_events::KeyboardConnectionType Convert(
    crosapi::TelemetryKeyboardConnectionType input) {
  switch (input) {
    case crosapi::TelemetryKeyboardConnectionType::kUnmappedEnumField:
      return cx_events::KeyboardConnectionType::kNone;
    case crosapi::TelemetryKeyboardConnectionType::kInternal:
      return cx_events::KeyboardConnectionType::kInternal;
    case crosapi::TelemetryKeyboardConnectionType::kUsb:
      return cx_events::KeyboardConnectionType::kUsb;
    case crosapi::TelemetryKeyboardConnectionType::kBluetooth:
      return cx_events::KeyboardConnectionType::kBluetooth;
    case crosapi::TelemetryKeyboardConnectionType::kUnknown:
      return cx_events::KeyboardConnectionType::kUnknown;
  }
  NOTREACHED();
}

cx_events::PhysicalKeyboardLayout Convert(
    crosapi::TelemetryKeyboardPhysicalLayout input) {
  switch (input) {
    case crosapi::TelemetryKeyboardPhysicalLayout::kUnmappedEnumField:
      return cx_events::PhysicalKeyboardLayout::kNone;
    case crosapi::TelemetryKeyboardPhysicalLayout::kUnknown:
      return cx_events::PhysicalKeyboardLayout::kUnknown;
    case crosapi::TelemetryKeyboardPhysicalLayout::kChromeOS:
      return cx_events::PhysicalKeyboardLayout::kChromeOs;
  }
  NOTREACHED();
}

cx_events::MechanicalKeyboardLayout Convert(
    crosapi::TelemetryKeyboardMechanicalLayout input) {
  switch (input) {
    case crosapi::TelemetryKeyboardMechanicalLayout::kUnmappedEnumField:
      return cx_events::MechanicalKeyboardLayout::kNone;
    case crosapi::TelemetryKeyboardMechanicalLayout::kUnknown:
      return cx_events::MechanicalKeyboardLayout::kUnknown;
    case crosapi::TelemetryKeyboardMechanicalLayout::kAnsi:
      return cx_events::MechanicalKeyboardLayout::kAnsi;
    case crosapi::TelemetryKeyboardMechanicalLayout::kIso:
      return cx_events::MechanicalKeyboardLayout::kIso;
    case crosapi::TelemetryKeyboardMechanicalLayout::kJis:
      return cx_events::MechanicalKeyboardLayout::kJis;
  }
  NOTREACHED();
}

cx_events::KeyboardNumberPadPresence Convert(
    crosapi::TelemetryKeyboardNumberPadPresence input) {
  switch (input) {
    case crosapi::TelemetryKeyboardNumberPadPresence::kUnmappedEnumField:
      return cx_events::KeyboardNumberPadPresence::kNone;
    case crosapi::TelemetryKeyboardNumberPadPresence::kUnknown:
      return cx_events::KeyboardNumberPadPresence::kUnknown;
    case crosapi::TelemetryKeyboardNumberPadPresence::kPresent:
      return cx_events::KeyboardNumberPadPresence::kPresent;
    case crosapi::TelemetryKeyboardNumberPadPresence::kNotPresent:
      return cx_events::KeyboardNumberPadPresence::kNotPresent;
  }
  NOTREACHED();
}

cx_events::KeyboardTopRowKey Convert(
    crosapi::TelemetryKeyboardTopRowKey input) {
  switch (input) {
    case crosapi::TelemetryKeyboardTopRowKey::kUnmappedEnumField:
      return cx_events::KeyboardTopRowKey::kNone;
    case crosapi::TelemetryKeyboardTopRowKey::kNone:
      return cx_events::KeyboardTopRowKey::kNoKey;
    case crosapi::TelemetryKeyboardTopRowKey::kUnknown:
      return cx_events::KeyboardTopRowKey::kUnknown;
    case crosapi::TelemetryKeyboardTopRowKey::kBack:
      return cx_events::KeyboardTopRowKey::kBack;
    case crosapi::TelemetryKeyboardTopRowKey::kForward:
      return cx_events::KeyboardTopRowKey::kForward;
    case crosapi::TelemetryKeyboardTopRowKey::kRefresh:
      return cx_events::KeyboardTopRowKey::kRefresh;
    case crosapi::TelemetryKeyboardTopRowKey::kFullscreen:
      return cx_events::KeyboardTopRowKey::kFullscreen;
    case crosapi::TelemetryKeyboardTopRowKey::kOverview:
      return cx_events::KeyboardTopRowKey::kOverview;
    case crosapi::TelemetryKeyboardTopRowKey::kScreenshot:
      return cx_events::KeyboardTopRowKey::kScreenshot;
    case crosapi::TelemetryKeyboardTopRowKey::kScreenBrightnessDown:
      return cx_events::KeyboardTopRowKey::kScreenBrightnessDown;
    case crosapi::TelemetryKeyboardTopRowKey::kScreenBrightnessUp:
      return cx_events::KeyboardTopRowKey::kScreenBrightnessUp;
    case crosapi::TelemetryKeyboardTopRowKey::kPrivacyScreenToggle:
      return cx_events::KeyboardTopRowKey::kPrivacyScreenToggle;
    case crosapi::TelemetryKeyboardTopRowKey::kMicrophoneMute:
      return cx_events::KeyboardTopRowKey::kMicrophoneMute;
    case crosapi::TelemetryKeyboardTopRowKey::kVolumeMute:
      return cx_events::KeyboardTopRowKey::kVolumeMute;
    case crosapi::TelemetryKeyboardTopRowKey::kVolumeDown:
      return cx_events::KeyboardTopRowKey::kVolumeDown;
    case crosapi::TelemetryKeyboardTopRowKey::kVolumeUp:
      return cx_events::KeyboardTopRowKey::kVolumeUp;
    case crosapi::TelemetryKeyboardTopRowKey::kKeyboardBacklightToggle:
      return cx_events::KeyboardTopRowKey::kKeyboardBacklightToggle;
    case crosapi::TelemetryKeyboardTopRowKey::kKeyboardBacklightDown:
      return cx_events::KeyboardTopRowKey::kKeyboardBacklightDown;
    case crosapi::TelemetryKeyboardTopRowKey::kKeyboardBacklightUp:
      return cx_events::KeyboardTopRowKey::kKeyboardBacklightUp;
    case crosapi::TelemetryKeyboardTopRowKey::kNextTrack:
      return cx_events::KeyboardTopRowKey::kNextTrack;
    case crosapi::TelemetryKeyboardTopRowKey::kPreviousTrack:
      return cx_events::KeyboardTopRowKey::kPreviousTrack;
    case crosapi::TelemetryKeyboardTopRowKey::kPlayPause:
      return cx_events::KeyboardTopRowKey::kPlayPause;
    case crosapi::TelemetryKeyboardTopRowKey::kScreenMirror:
      return cx_events::KeyboardTopRowKey::kScreenMirror;
    case crosapi::TelemetryKeyboardTopRowKey::kDelete:
      return cx_events::KeyboardTopRowKey::kDelete;
  }
  NOTREACHED();
}

cx_events::KeyboardTopRightKey Convert(
    crosapi::TelemetryKeyboardTopRightKey input) {
  switch (input) {
    case crosapi::TelemetryKeyboardTopRightKey::kUnmappedEnumField:
      return cx_events::KeyboardTopRightKey::kNone;
    case crosapi::TelemetryKeyboardTopRightKey::kUnknown:
      return cx_events::KeyboardTopRightKey::kUnknown;
    case crosapi::TelemetryKeyboardTopRightKey::kPower:
      return cx_events::KeyboardTopRightKey::kPower;
    case crosapi::TelemetryKeyboardTopRightKey::kLock:
      return cx_events::KeyboardTopRightKey::kLock;
    case crosapi::TelemetryKeyboardTopRightKey::kControlPanel:
      return cx_events::KeyboardTopRightKey::kControlPanel;
  }
  NOTREACHED();
}

cx_events::LidEvent Convert(crosapi::TelemetryLidEventInfo::State state) {
  switch (state) {
    case crosapi::TelemetryLidEventInfo_State::kUnmappedEnumField:
      return cx_events::LidEvent::kNone;
    case crosapi::TelemetryLidEventInfo_State::kClosed:
      return cx_events::LidEvent::kClosed;
    case crosapi::TelemetryLidEventInfo_State::kOpened:
      return cx_events::LidEvent::kOpened;
  }
  NOTREACHED();
}

cx_events::UsbEvent Convert(crosapi::TelemetryUsbEventInfo::State state) {
  switch (state) {
    case crosapi::TelemetryUsbEventInfo_State::kUnmappedEnumField:
      return cx_events::UsbEvent::kNone;
    case crosapi::TelemetryUsbEventInfo_State::kAdd:
      return cx_events::UsbEvent::kConnected;
    case crosapi::TelemetryUsbEventInfo_State::kRemove:
      return cx_events::UsbEvent::kDisconnected;
  }
  NOTREACHED();
}

cx_events::ExternalDisplayEvent Convert(
    crosapi::TelemetryExternalDisplayEventInfo::State state) {
  switch (state) {
    case crosapi::TelemetryExternalDisplayEventInfo_State::kUnmappedEnumField:
      return cx_events::ExternalDisplayEvent::kNone;
    case crosapi::TelemetryExternalDisplayEventInfo_State::kAdd:
      return cx_events::ExternalDisplayEvent::kConnected;
    case crosapi::TelemetryExternalDisplayEventInfo_State::kRemove:
      return cx_events::ExternalDisplayEvent::kDisconnected;
  }
  NOTREACHED();
}

cx_events::SdCardEvent Convert(crosapi::TelemetrySdCardEventInfo::State state) {
  switch (state) {
    case crosapi::TelemetrySdCardEventInfo_State::kUnmappedEnumField:
      return cx_events::SdCardEvent::kNone;
    case crosapi::TelemetrySdCardEventInfo_State::kAdd:
      return cx_events::SdCardEvent::kConnected;
    case crosapi::TelemetrySdCardEventInfo_State::kRemove:
      return cx_events::SdCardEvent::kDisconnected;
  }
  NOTREACHED();
}

cx_events::PowerEvent Convert(crosapi::TelemetryPowerEventInfo::State state) {
  switch (state) {
    case crosapi::TelemetryPowerEventInfo_State::kUnmappedEnumField:
      return cx_events::PowerEvent::kNone;
    case crosapi::TelemetryPowerEventInfo_State::kAcInserted:
      return cx_events::PowerEvent::kAcInserted;
    case crosapi::TelemetryPowerEventInfo_State::kAcRemoved:
      return cx_events::PowerEvent::kAcRemoved;
    case crosapi::TelemetryPowerEventInfo_State::kOsSuspend:
      return cx_events::PowerEvent::kOsSuspend;
    case crosapi::TelemetryPowerEventInfo_State::kOsResume:
      return cx_events::PowerEvent::kOsResume;
  }
  NOTREACHED();
}

cx_events::StylusGarageEvent Convert(
    crosapi::TelemetryStylusGarageEventInfo::State state) {
  switch (state) {
    case crosapi::TelemetryStylusGarageEventInfo_State::kUnmappedEnumField:
      return cx_events::StylusGarageEvent::kNone;
    case crosapi::TelemetryStylusGarageEventInfo_State::kInserted:
      return cx_events::StylusGarageEvent::kInserted;
    case crosapi::TelemetryStylusGarageEventInfo_State::kRemoved:
      return cx_events::StylusGarageEvent::kRemoved;
  }
  NOTREACHED();
}

cx_events::InputTouchButton Convert(crosapi::TelemetryInputTouchButton button) {
  switch (button) {
    case crosapi::TelemetryInputTouchButton::kUnmappedEnumField:
      return cx_events::InputTouchButton::kNone;
    case crosapi::TelemetryInputTouchButton::kLeft:
      return cx_events::InputTouchButton::kLeft;
    case crosapi::TelemetryInputTouchButton::kMiddle:
      return cx_events::InputTouchButton::kMiddle;
    case crosapi::TelemetryInputTouchButton::kRight:
      return cx_events::InputTouchButton::kRight;
  }
  NOTREACHED();
}

crosapi::TelemetryEventCategoryEnum Convert(cx_events::EventCategory input) {
  switch (input) {
    case cx_events::EventCategory::kNone:
      return crosapi::TelemetryEventCategoryEnum::kUnmappedEnumField;
    case cx_events::EventCategory::kAudioJack:
      return crosapi::TelemetryEventCategoryEnum::kAudioJack;
    case cx_events::EventCategory::kLid:
      return crosapi::TelemetryEventCategoryEnum::kLid;
    case cx_events::EventCategory::kUsb:
      return crosapi::TelemetryEventCategoryEnum::kUsb;
    case cx_events::EventCategory::kExternalDisplay:
      return crosapi::TelemetryEventCategoryEnum::kExternalDisplay;
    case cx_events::EventCategory::kSdCard:
      return crosapi::TelemetryEventCategoryEnum::kSdCard;
    case cx_events::EventCategory::kPower:
      return crosapi::TelemetryEventCategoryEnum::kPower;
    case cx_events::EventCategory::kKeyboardDiagnostic:
      return crosapi::TelemetryEventCategoryEnum::kKeyboardDiagnostic;
    case cx_events::EventCategory::kStylusGarage:
      return crosapi::TelemetryEventCategoryEnum::kStylusGarage;
    case cx_events::EventCategory::kTouchpadButton:
      return crosapi::TelemetryEventCategoryEnum::kTouchpadButton;
    case cx_events::EventCategory::kTouchpadTouch:
      return crosapi::TelemetryEventCategoryEnum::kTouchpadTouch;
    case cx_events::EventCategory::kTouchpadConnected:
      return crosapi::TelemetryEventCategoryEnum::kTouchpadConnected;
    case cx_events::EventCategory::kTouchscreenTouch:
      return crosapi::TelemetryEventCategoryEnum::kTouchscreenTouch;
    case cx_events::EventCategory::kTouchscreenConnected:
      return crosapi::TelemetryEventCategoryEnum::kTouchscreenConnected;
    case cx_events::EventCategory::kStylusTouch:
      return crosapi::TelemetryEventCategoryEnum::kStylusTouch;
    case cx_events::EventCategory::kStylusConnected:
      return crosapi::TelemetryEventCategoryEnum::kStylusConnected;
  }
  NOTREACHED();
}

cx_events::InputTouchButtonState Convert(
    crosapi::TelemetryTouchpadButtonEventInfo::State state) {
  switch (state) {
    case crosapi::TelemetryTouchpadButtonEventInfo_State::kUnmappedEnumField:
      return cx_events::InputTouchButtonState::kNone;
    case crosapi::TelemetryTouchpadButtonEventInfo_State::kPressed:
      return cx_events::InputTouchButtonState::kPressed;
    case crosapi::TelemetryTouchpadButtonEventInfo_State::kReleased:
      return cx_events::InputTouchButtonState::kReleased;
  }
  NOTREACHED();
}

cx_events::DisplayInputType Convert(crosapi::ProbeDisplayInputType input) {
  switch (input) {
    case crosapi::ProbeDisplayInputType::kUnmappedEnumField:
      return cx_events::DisplayInputType::kUnknown;
    case crosapi::ProbeDisplayInputType::kDigital:
      return cx_events::DisplayInputType::kDigital;
    case crosapi::ProbeDisplayInputType::kAnalog:
      return cx_events::DisplayInputType::kAnalog;
  }
  NOTREACHED();
}

int Convert(uint32_t input) {
  return static_cast<int>(input);
}

}  // namespace chromeos::converters::events

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api_converters.h"

#include "base/notreached.h"
#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"

namespace chromeos::converters {

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

cx_events::SdCardEventInfo UncheckedConvertPtr(
    crosapi::TelemetrySdCardEventInfoPtr ptr) {
  cx_events::SdCardEventInfo result;

  result.event = Convert(ptr->state);

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
    case cx_events::EventCategory::kSdCard:
      return crosapi::TelemetryEventCategoryEnum::kSdCard;
  }
  NOTREACHED();
}

}  // namespace chromeos::converters

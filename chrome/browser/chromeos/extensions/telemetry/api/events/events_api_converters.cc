// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api_converters.h"

#include "base/notreached.h"
#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"

namespace chromeos::converters {

namespace unchecked {

api::os_events::AudioJackEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryAudioJackEventInfoPtr ptr) {
  api::os_events::AudioJackEventInfo result;

  result.event = Convert(ptr->state);
  result.device_type = Convert(ptr->device_type);

  return result;
}

api::os_events::LidEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryLidEventInfoPtr ptr) {
  api::os_events::LidEventInfo result;

  result.event = Convert(ptr->state);

  return result;
}

}  // namespace unchecked

api::os_events::AudioJackEvent Convert(
    crosapi::mojom::TelemetryAudioJackEventInfo::State state) {
  switch (state) {
    case crosapi::mojom::TelemetryAudioJackEventInfo_State::kUnmappedEnumField:
      return api::os_events::AudioJackEvent::kNone;
    case crosapi::mojom::TelemetryAudioJackEventInfo_State::kAdd:
      return api::os_events::AudioJackEvent::kConnected;
    case crosapi::mojom::TelemetryAudioJackEventInfo_State::kRemove:
      return api::os_events::AudioJackEvent::kDisconnected;
  }
  NOTREACHED();
}

api::os_events::AudioJackDeviceType Convert(
    crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType device_type) {
  switch (device_type) {
    case crosapi::mojom::TelemetryAudioJackEventInfo_DeviceType::
        kUnmappedEnumField:
      return api::os_events::AudioJackDeviceType::kNone;
    case crosapi::mojom::TelemetryAudioJackEventInfo_DeviceType::kHeadphone:
      return api::os_events::AudioJackDeviceType::kHeadphone;
    case crosapi::mojom::TelemetryAudioJackEventInfo_DeviceType::kMicrophone:
      return api::os_events::AudioJackDeviceType::kMicrophone;
  }
  NOTREACHED();
}

api::os_events::LidEvent Convert(
    crosapi::mojom::TelemetryLidEventInfo::State state) {
  switch (state) {
    case crosapi::mojom::TelemetryLidEventInfo_State::kUnmappedEnumField:
      return api::os_events::LidEvent::kNone;
    case crosapi::mojom::TelemetryLidEventInfo_State::kClosed:
      return api::os_events::LidEvent::kClosed;
    case crosapi::mojom::TelemetryLidEventInfo_State::kOpened:
      return api::os_events::LidEvent::kOpened;
  }
  NOTREACHED();
}

crosapi::mojom::TelemetryEventCategoryEnum Convert(
    api::os_events::EventCategory input) {
  switch (input) {
    case api::os_events::EventCategory::kNone:
      return crosapi::mojom::TelemetryEventCategoryEnum::kUnmappedEnumField;
    case api::os_events::EventCategory::kAudioJack:
      return crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack;
    case api::os_events::EventCategory::kLid:
      return crosapi::mojom::TelemetryEventCategoryEnum::kLid;
  }
  NOTREACHED();
}

}  // namespace chromeos::converters

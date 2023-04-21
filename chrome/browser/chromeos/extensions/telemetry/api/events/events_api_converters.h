// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENTS_API_CONVERTERS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENTS_API_CONVERTERS_H_

#include <utility>

#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"

namespace chromeos::converters {

namespace unchecked {

api::os_events::AudioJackEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryAudioJackEventInfoPtr ptr);

api::os_events::LidEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryLidEventInfoPtr ptr);

}  // namespace unchecked

api::os_events::AudioJackEvent Convert(
    crosapi::mojom::TelemetryAudioJackEventInfo::State state);

api::os_events::AudioJackDeviceType Convert(
    crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType device_type);

api::os_events::LidEvent Convert(
    crosapi::mojom::TelemetryLidEventInfo::State state);

crosapi::mojom::TelemetryEventCategoryEnum Convert(
    api::os_events::EventCategory input);

template <class OutputT, class InputT>
OutputT ConvertEventPtr(InputT input) {
  return (!input.is_null()) ? unchecked::UncheckedConvertPtr(std::move(input))
                            : OutputT();
}

}  // namespace chromeos::converters

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENTS_API_CONVERTERS_H_

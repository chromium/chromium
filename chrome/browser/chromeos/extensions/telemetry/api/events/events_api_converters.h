// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENTS_API_CONVERTERS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENTS_API_CONVERTERS_H_

#include <cstdint>
#include <type_traits>
#include <utility>

#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_keyboard_event.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos::converters {

namespace unchecked {

api::os_events::AudioJackEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryAudioJackEventInfoPtr ptr);

api::os_events::KeyboardInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryKeyboardInfoPtr ptr);

api::os_events::KeyboardDiagnosticEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryKeyboardDiagnosticEventInfoPtr ptr);

api::os_events::LidEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryLidEventInfoPtr ptr);

api::os_events::UsbEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryUsbEventInfoPtr ptr);

api::os_events::SdCardEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetrySdCardEventInfoPtr ptr);

api::os_events::PowerEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryPowerEventInfoPtr ptr);

absl::optional<uint32_t> UncheckedConvertPtr(
    crosapi::mojom::UInt32ValuePtr ptr);

}  // namespace unchecked

api::os_events::AudioJackEvent Convert(
    crosapi::mojom::TelemetryAudioJackEventInfo::State state);

api::os_events::AudioJackDeviceType Convert(
    crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType device_type);

api::os_events::KeyboardConnectionType Convert(
    crosapi::mojom::TelemetryKeyboardConnectionType input);

api::os_events::PhysicalKeyboardLayout Convert(
    crosapi::mojom::TelemetryKeyboardPhysicalLayout input);

api::os_events::MechanicalKeyboardLayout Convert(
    crosapi::mojom::TelemetryKeyboardMechanicalLayout input);

api::os_events::KeyboardNumberPadPresence Convert(
    crosapi::mojom::TelemetryKeyboardNumberPadPresence input);

api::os_events::KeyboardTopRowKey Convert(
    crosapi::mojom::TelemetryKeyboardTopRowKey input);

api::os_events::KeyboardTopRightKey Convert(
    crosapi::mojom::TelemetryKeyboardTopRightKey input);

api::os_events::LidEvent Convert(
    crosapi::mojom::TelemetryLidEventInfo::State state);

api::os_events::UsbEvent Convert(
    crosapi::mojom::TelemetryUsbEventInfo::State state);

api::os_events::SdCardEvent Convert(
    crosapi::mojom::TelemetrySdCardEventInfo::State state);

api::os_events::PowerEvent Convert(
    crosapi::mojom::TelemetryPowerEventInfo::State state);

crosapi::mojom::TelemetryEventCategoryEnum Convert(
    api::os_events::EventCategory input);

int Convert(uint32_t input);

template <class OutputT,
          class InputT,
          std::enable_if_t<std::is_enum_v<InputT> || std::is_integral_v<InputT>,
                           bool> = true>
std::vector<OutputT> ConvertVector(std::vector<InputT> input) {
  std::vector<OutputT> output;
  for (auto elem : input) {
    output.push_back(Convert(std::move(elem)));
  }
  return output;
}

template <class OutputT, class InputT>
OutputT ConvertStructPtr(InputT input) {
  return (!input.is_null()) ? unchecked::UncheckedConvertPtr(std::move(input))
                            : OutputT();
}

}  // namespace chromeos::converters

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENTS_API_CONVERTERS_H_

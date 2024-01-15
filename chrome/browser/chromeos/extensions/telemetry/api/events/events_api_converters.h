// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENTS_API_CONVERTERS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENTS_API_CONVERTERS_H_

#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_keyboard_event.mojom.h"

namespace chromeos::converters::events {

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

api::os_events::ExternalDisplayEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryExternalDisplayEventInfoPtr ptr);

api::os_events::ExternalDisplayInfo UncheckedConvertPtr(
    crosapi::mojom::ProbeExternalDisplayInfoPtr input);

api::os_events::SdCardEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetrySdCardEventInfoPtr ptr);

api::os_events::PowerEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryPowerEventInfoPtr ptr);

api::os_events::StylusGarageEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryStylusGarageEventInfoPtr ptr);

std::optional<uint32_t> UncheckedConvertPtr(crosapi::mojom::UInt32ValuePtr ptr);

api::os_events::TouchpadButtonEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryTouchpadButtonEventInfoPtr ptr);

api::os_events::TouchpadTouchEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryTouchpadTouchEventInfoPtr ptr);

api::os_events::TouchpadConnectedEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryTouchpadConnectedEventInfoPtr ptr);

api::os_events::TouchscreenTouchEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryTouchscreenTouchEventInfoPtr ptr);

api::os_events::TouchscreenConnectedEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryTouchscreenConnectedEventInfoPtr ptr);

api::os_events::TouchPointInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryTouchPointInfoPtr ptr);

api::os_events::StylusTouchPointInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryStylusTouchPointInfoPtr ptr);

api::os_events::StylusTouchEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryStylusTouchEventInfoPtr ptr);

api::os_events::StylusConnectedEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryStylusConnectedEventInfoPtr ptr);

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

api::os_events::ExternalDisplayEvent Convert(
    crosapi::mojom::TelemetryExternalDisplayEventInfo::State state);

api::os_events::SdCardEvent Convert(
    crosapi::mojom::TelemetrySdCardEventInfo::State state);

api::os_events::PowerEvent Convert(
    crosapi::mojom::TelemetryPowerEventInfo::State state);

api::os_events::StylusGarageEvent Convert(
    crosapi::mojom::TelemetryStylusGarageEventInfo::State state);

api::os_events::InputTouchButton Convert(
    crosapi::mojom::TelemetryInputTouchButton button);

api::os_events::InputTouchButtonState Convert(
    crosapi::mojom::TelemetryTouchpadButtonEventInfo::State state);

api::os_events::DisplayInputType Convert(
    crosapi::mojom::ProbeDisplayInputType input);

crosapi::mojom::TelemetryEventCategoryEnum Convert(
    api::os_events::EventCategory input);

int Convert(uint32_t input);

template <class InputT,
          class OutputT = decltype(Convert(std::declval<InputT>())),
          class = std::enable_if_t<std::is_enum_v<InputT> ||
                                   std::is_integral_v<InputT>>>
std::vector<OutputT> ConvertVector(std::vector<InputT> input) {
  std::vector<OutputT> output;
  for (auto elem : input) {
    output.push_back(Convert(std::move(elem)));
  }
  return output;
}

template <class InputT,
          class OutputT =
              decltype(unchecked::UncheckedConvertPtr(std::declval<InputT>())),
          class = std::enable_if_t<std::is_default_constructible_v<OutputT>>>
OutputT ConvertStructPtr(InputT input) {
  return (!input.is_null()) ? unchecked::UncheckedConvertPtr(std::move(input))
                            : OutputT();
}

template <class OutputT, class InputT>
std::vector<OutputT> ConvertStructPtrVector(std::vector<InputT> input) {
  std::vector<OutputT> output;
  for (auto&& element : input) {
    DCHECK(!element.is_null());
    output.push_back(unchecked::UncheckedConvertPtr(std::move(element)));
  }
  return output;
}

}  // namespace chromeos::converters::events

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENTS_API_CONVERTERS_H_

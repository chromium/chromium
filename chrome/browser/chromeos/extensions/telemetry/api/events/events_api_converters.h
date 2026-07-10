// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENTS_API_CONVERTERS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENTS_API_CONVERTERS_H_

#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

#include "ash/system/diagnostics/mojom/input.mojom.h"
#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"

namespace chromeos::converters::events {

namespace unchecked {

api::os_events::AudioJackEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::AudioJackEventInfoPtr ptr);

api::os_events::KeyboardInfo UncheckedConvertPtr(
    ash::diagnostics::mojom::KeyboardInfoPtr ptr);

api::os_events::KeyboardDiagnosticEventInfo UncheckedConvertPtr(
    ash::diagnostics::mojom::KeyboardDiagnosticEventInfoPtr ptr);

api::os_events::LidEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::LidEventInfoPtr ptr);

api::os_events::UsbEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::UsbEventInfoPtr ptr);

api::os_events::ExternalDisplayEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::ExternalDisplayEventInfoPtr ptr);

api::os_events::ExternalDisplayInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::ExternalDisplayInfoPtr input);

api::os_events::SdCardEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::SdCardEventInfoPtr ptr);

api::os_events::PowerEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::PowerEventInfoPtr ptr);

api::os_events::StylusGarageEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::StylusGarageEventInfoPtr ptr);

std::optional<uint32_t> UncheckedConvertPtr(
    ash::cros_healthd::mojom::NullableUint32Ptr ptr);

api::os_events::TouchpadButtonEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::TouchpadButtonEventPtr ptr);

api::os_events::TouchpadTouchEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::TouchpadTouchEventPtr ptr);

api::os_events::TouchpadConnectedEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::TouchpadConnectedEventPtr ptr);

api::os_events::TouchscreenTouchEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::TouchscreenTouchEventPtr ptr);

api::os_events::TouchscreenConnectedEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::TouchscreenConnectedEventPtr ptr);

api::os_events::TouchPointInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::TouchPointInfoPtr ptr);

api::os_events::StylusTouchPointInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::StylusTouchPointInfoPtr ptr);

api::os_events::StylusTouchEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::StylusTouchEventPtr ptr);

api::os_events::StylusConnectedEventInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::StylusConnectedEventPtr ptr);

}  // namespace unchecked

api::os_events::AudioJackEvent Convert(
    ash::cros_healthd::mojom::AudioJackEventInfo::State state);

api::os_events::AudioJackDeviceType Convert(
    ash::cros_healthd::mojom::AudioJackEventInfo::DeviceType device_type);

api::os_events::KeyboardConnectionType Convert(
    ash::diagnostics::mojom::ConnectionType input);

api::os_events::PhysicalKeyboardLayout Convert(
    ash::diagnostics::mojom::PhysicalLayout input);

api::os_events::MechanicalKeyboardLayout Convert(
    ash::diagnostics::mojom::MechanicalLayout input);

api::os_events::KeyboardNumberPadPresence Convert(
    ash::diagnostics::mojom::NumberPadPresence input);

api::os_events::KeyboardTopRowKey Convert(
    ash::diagnostics::mojom::TopRowKey input);

api::os_events::KeyboardTopRightKey Convert(
    ash::diagnostics::mojom::TopRightKey input);

api::os_events::LidEvent Convert(
    ash::cros_healthd::mojom::LidEventInfo::State state);

api::os_events::UsbEvent Convert(
    ash::cros_healthd::mojom::UsbEventInfo::State state);

api::os_events::ExternalDisplayEvent Convert(
    ash::cros_healthd::mojom::ExternalDisplayEventInfo::State state);

api::os_events::SdCardEvent Convert(
    ash::cros_healthd::mojom::SdCardEventInfo::State state);

api::os_events::PowerEvent Convert(
    ash::cros_healthd::mojom::PowerEventInfo::State state);

api::os_events::StylusGarageEvent Convert(
    ash::cros_healthd::mojom::StylusGarageEventInfo::State state);

api::os_events::InputTouchButton Convert(
    ash::cros_healthd::mojom::InputTouchButton button);

api::os_events::DisplayInputType Convert(
    ash::cros_healthd::mojom::DisplayInputType input);

ash::cros_healthd::mojom::EventCategoryEnum Convert(
    api::os_events::EventCategory input);

int Convert(uint32_t input);

template <class InputT,
          class OutputT = decltype(Convert(std::declval<InputT>()))>
  requires(std::is_enum_v<InputT> || std::is_integral_v<InputT>)
std::vector<OutputT> ConvertVector(std::vector<InputT> input) {
  std::vector<OutputT> output;
  for (auto elem : input) {
    output.push_back(Convert(std::move(elem)));
  }
  return output;
}

template <class InputT,
          class OutputT =
              decltype(unchecked::UncheckedConvertPtr(std::declval<InputT>()))>
  requires(std::is_default_constructible_v<OutputT>)
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

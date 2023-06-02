// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_EVENTS_TELEMETRY_EVENT_SERVICE_CONVERTERS_H_
#define CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_EVENTS_TELEMETRY_EVENT_SERVICE_CONVERTERS_H_

#include <type_traits>
#include <vector>

#include "ash/system/diagnostics/mojom/input.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_exception.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_keyboard_event.mojom.h"

namespace ash::converters {

// This file contains helper functions used by TelemetryEventServiceAsh to
// convert its types to/from cros_healthd EventService types.

namespace unchecked {

crosapi::mojom::TelemetryAudioJackEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::AudioJackEventInfoPtr input);

crosapi::mojom::TelemetryKeyboardInfoPtr UncheckedConvertPtr(
    diagnostics::mojom::KeyboardInfoPtr input);

crosapi::mojom::TelemetryKeyboardDiagnosticEventInfoPtr UncheckedConvertPtr(
    diagnostics::mojom::KeyboardDiagnosticEventInfoPtr input);

crosapi::mojom::TelemetryLidEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::LidEventInfoPtr input);

crosapi::mojom::TelemetryUsbEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::UsbEventInfoPtr input);

crosapi::mojom::TelemetrySdCardEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::SdCardEventInfoPtr input);

crosapi::mojom::TelemetryPowerEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::PowerEventInfoPtr input);

crosapi::mojom::TelemetryEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::EventInfoPtr input);

crosapi::mojom::TelemetryExtensionExceptionPtr UncheckedConvertPtr(
    cros_healthd::mojom::ExceptionPtr input);

crosapi::mojom::TelemetryExtensionSupportedPtr UncheckedConvertPtr(
    cros_healthd::mojom::SupportedPtr input);

crosapi::mojom::TelemetryExtensionUnsupportedReasonPtr UncheckedConvertPtr(
    cros_healthd::mojom::UnsupportedReasonPtr input);

crosapi::mojom::TelemetryExtensionUnsupportedPtr UncheckedConvertPtr(
    cros_healthd::mojom::UnsupportedPtr input);

crosapi::mojom::TelemetryExtensionSupportStatusPtr UncheckedConvertPtr(
    cros_healthd::mojom::SupportStatusPtr input);

}  // namespace unchecked

crosapi::mojom::TelemetryAudioJackEventInfo::State Convert(
    cros_healthd::mojom::AudioJackEventInfo::State input);

crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType Convert(
    cros_healthd::mojom::AudioJackEventInfo::DeviceType input);

crosapi::mojom::TelemetryKeyboardConnectionType Convert(
    diagnostics::mojom::ConnectionType input);

crosapi::mojom::TelemetryKeyboardPhysicalLayout Convert(
    diagnostics::mojom::PhysicalLayout input);

crosapi::mojom::TelemetryKeyboardMechanicalLayout Convert(
    diagnostics::mojom::MechanicalLayout input);

crosapi::mojom::TelemetryKeyboardNumberPadPresence Convert(
    diagnostics::mojom::NumberPadPresence input);

crosapi::mojom::TelemetryKeyboardTopRowKey Convert(
    diagnostics::mojom::TopRowKey input);

crosapi::mojom::TelemetryKeyboardTopRightKey Convert(
    diagnostics::mojom::TopRightKey input);

crosapi::mojom::TelemetryLidEventInfo::State Convert(
    cros_healthd::mojom::LidEventInfo::State input);

crosapi::mojom::TelemetryUsbEventInfo::State Convert(
    cros_healthd::mojom::UsbEventInfo::State input);

crosapi::mojom::TelemetrySdCardEventInfo::State Convert(
    cros_healthd::mojom::SdCardEventInfo::State input);

crosapi::mojom::TelemetryPowerEventInfo::State Convert(
    cros_healthd::mojom::PowerEventInfo::State input);

crosapi::mojom::TelemetryExtensionException::Reason Convert(
    cros_healthd::mojom::Exception::Reason input);

cros_healthd::mojom::EventCategoryEnum Convert(
    crosapi::mojom::TelemetryEventCategoryEnum input);

template <class OutputT,
          class InputT,
          std::enable_if_t<std::is_enum_v<InputT>, bool> = true>
std::vector<OutputT> ConvertVector(std::vector<InputT> input) {
  std::vector<OutputT> result;
  for (auto elem : input) {
    result.push_back(Convert(elem));
  }
  return result;
}

template <class InputT>
auto ConvertStructPtr(InputT input) {
  return (!input.is_null()) ? unchecked::UncheckedConvertPtr(std::move(input))
                            : nullptr;
}

}  // namespace ash::converters

#endif  // CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_EVENTS_TELEMETRY_EVENT_SERVICE_CONVERTERS_H_

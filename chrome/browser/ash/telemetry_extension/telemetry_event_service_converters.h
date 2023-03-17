// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_TELEMETRY_EVENT_SERVICE_CONVERTERS_H_
#define CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_TELEMETRY_EVENT_SERVICE_CONVERTERS_H_

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_exception.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"

namespace ash::converters {

// This file contains helper functions used by TelemetryEventServiceAsh to
// convert its types to/from cros_healthd EventService types.

namespace unchecked {

crosapi::mojom::TelemetryAudioJackEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::AudioJackEventInfoPtr input);

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

crosapi::mojom::TelemetryExtensionException::Reason Convert(
    cros_healthd::mojom::Exception::Reason input);

cros_healthd::mojom::EventCategoryEnum Convert(
    crosapi::mojom::TelemetryEventCategoryEnum input);

template <class InputT>
auto ConvertEventPtr(InputT input) {
  return (!input.is_null()) ? unchecked::UncheckedConvertPtr(std::move(input))
                            : nullptr;
}

}  // namespace ash::converters

#endif  // CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_TELEMETRY_EVENT_SERVICE_CONVERTERS_H_

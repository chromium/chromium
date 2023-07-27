// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_CONVERTERS_H_
#define CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_CONVERTERS_H_

#include <utility>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"

namespace ash::converters {

// This file contains helper functions used by
// TelemetryDiagnosticsRoutineServiceAsh to convert its types to/from
// cros_healthd routine types.

namespace unchecked {

cros_healthd::mojom::RoutineArgumentPtr UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticRoutineArgumentPtr input);

}  // namespace unchecked

template <class InputT>
auto ConvertRoutinePtr(InputT input) {
  return (!input.is_null()) ? unchecked::UncheckedConvertPtr(std::move(input))
                            : nullptr;
}

}  // namespace ash::converters

#endif  // CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_CONVERTERS_H_

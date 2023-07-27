// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/routines/routine_converters.h"

#include <utility>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"

namespace ash::converters {

namespace {

namespace crosapi = ::crosapi::mojom;
namespace healthd = cros_healthd::mojom;

}  // namespace

namespace unchecked {

healthd::RoutineArgumentPtr UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticRoutineArgumentPtr input) {
  switch (input->which()) {
    case crosapi::internal::TelemetryDiagnosticRoutineArgument_Data::
        TelemetryDiagnosticRoutineArgument_Tag::kUnrecognizedArgument:
      return healthd::RoutineArgument::NewUnrecognizedArgument(
          std::move(input->get_unrecognizedArgument()));
  }
}

}  // namespace unchecked

}  // namespace ash::converters

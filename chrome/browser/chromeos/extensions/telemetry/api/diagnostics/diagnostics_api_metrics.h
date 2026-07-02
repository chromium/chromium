// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_DIAGNOSTICS_API_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_DIAGNOSTICS_API_METRICS_H_

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"

namespace chromeos {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange
enum class DiagnosticRoutineCategoryHistogramValue {
  kUnknown = 0,
  kMemory = 1,
  kVolumeButton = 2,
  kFan = 3,
  kLedLitUp = 4,
  kNetworkBandwidth = 5,
  kCameraFrameAnalysis = 6,
  kKeyboardBacklight = 7,
  kMaxValue = kKeyboardBacklight,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/chromeos/enums.xml)

// Logs routine creation for each routine category `tag`.
void RecordRoutineCreation(ash::cros_healthd::mojom::RoutineArgument::Tag tag);

// Logs routine supported status query for each routine category `tag`.
void RecordRoutineSupportedStatusQuery(
    ash::cros_healthd::mojom::RoutineArgument::Tag tag);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_DIAGNOSTICS_API_METRICS_H_

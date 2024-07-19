// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_DIAGNOSTICS_API_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_DIAGNOSTICS_API_METRICS_H_

#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"

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
void RecordRoutineCreation(
    crosapi::mojom::TelemetryDiagnosticRoutineArgument::Tag tag);

// Logs routine supported status query for each routine category `tag`.
void RecordRoutineSupportedStatusQuery(
    crosapi::mojom::TelemetryDiagnosticRoutineArgument::Tag tag);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_DIAGNOSTICS_API_METRICS_H_

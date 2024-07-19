// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/diagnostics_api_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"

namespace chromeos {

namespace {

DiagnosticRoutineCategoryHistogramValue ConvertToHistogramValue(
    crosapi::mojom::TelemetryDiagnosticRoutineArgument::Tag tag) {
  switch (tag) {
    case crosapi::mojom::TelemetryDiagnosticRoutineArgument::Tag::
        kUnrecognizedArgument:
      return DiagnosticRoutineCategoryHistogramValue::kUnknown;
    case crosapi::mojom::TelemetryDiagnosticRoutineArgument::Tag::kMemory:
      return DiagnosticRoutineCategoryHistogramValue::kMemory;
    case crosapi::mojom::TelemetryDiagnosticRoutineArgument::Tag::kVolumeButton:
      return DiagnosticRoutineCategoryHistogramValue::kVolumeButton;
    case crosapi::mojom::TelemetryDiagnosticRoutineArgument::Tag::kFan:
      return DiagnosticRoutineCategoryHistogramValue::kFan;
    case crosapi::mojom::TelemetryDiagnosticRoutineArgument::Tag::kLedLitUp:
      return DiagnosticRoutineCategoryHistogramValue::kLedLitUp;
    case crosapi::mojom::TelemetryDiagnosticRoutineArgument::Tag::
        kNetworkBandwidth:
      return DiagnosticRoutineCategoryHistogramValue::kNetworkBandwidth;
    case crosapi::mojom::TelemetryDiagnosticRoutineArgument::Tag::
        kCameraFrameAnalysis:
      return DiagnosticRoutineCategoryHistogramValue::kCameraFrameAnalysis;
    case crosapi::mojom::TelemetryDiagnosticRoutineArgument::Tag::
        kKeyboardBacklight:
      return DiagnosticRoutineCategoryHistogramValue::kKeyboardBacklight;
  }
}

}  // namespace

void RecordRoutineCreation(
    crosapi::mojom::TelemetryDiagnosticRoutineArgument::Tag tag) {
  base::UmaHistogramEnumeration("ChromeOS.TelemetryExtension.RoutineCreation",
                                ConvertToHistogramValue(tag));
}

void RecordRoutineSupportedStatusQuery(
    crosapi::mojom::TelemetryDiagnosticRoutineArgument::Tag tag) {
  base::UmaHistogramEnumeration(
      "ChromeOS.TelemetryExtension.RoutineSupportedStatusQuery",
      ConvertToHistogramValue(tag));
}

}  // namespace chromeos

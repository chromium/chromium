// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/diagnostics_api_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"

namespace chromeos {

namespace {

DiagnosticRoutineCategoryHistogramValue ConvertToHistogramValue(
    ash::cros_healthd::mojom::RoutineArgument::Tag tag) {
  switch (tag) {
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kUnrecognizedArgument:
      return DiagnosticRoutineCategoryHistogramValue::kUnknown;
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kMemory:
      return DiagnosticRoutineCategoryHistogramValue::kMemory;
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kVolumeButton:
      return DiagnosticRoutineCategoryHistogramValue::kVolumeButton;
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kFan:
      return DiagnosticRoutineCategoryHistogramValue::kFan;
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kLedLitUp:
      return DiagnosticRoutineCategoryHistogramValue::kLedLitUp;
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kNetworkBandwidth:
      return DiagnosticRoutineCategoryHistogramValue::kNetworkBandwidth;
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kCameraFrameAnalysis:
      return DiagnosticRoutineCategoryHistogramValue::kCameraFrameAnalysis;
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kKeyboardBacklight:
      return DiagnosticRoutineCategoryHistogramValue::kKeyboardBacklight;
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kAudioDriver:
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kBatteryDischarge:
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kBluetoothDiscovery:
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kBluetoothPairing:
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kBluetoothPower:
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kBluetoothScanning:
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kCameraAvailability:
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kCpuCache:
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kCpuStress:
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kDiskRead:
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kFloatingPoint:
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kPrimeSearch:
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kSensitiveSensor:
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kUfsLifetime:
    case ash::cros_healthd::mojom::RoutineArgument::Tag::kUrandom:
      NOTREACHED();
  }
}

}  // namespace

void RecordRoutineCreation(ash::cros_healthd::mojom::RoutineArgument::Tag tag) {
  base::UmaHistogramEnumeration("ChromeOS.TelemetryExtension.RoutineCreation",
                                ConvertToHistogramValue(tag));
}

void RecordRoutineSupportedStatusQuery(
    ash::cros_healthd::mojom::RoutineArgument::Tag tag) {
  base::UmaHistogramEnumeration(
      "ChromeOS.TelemetryExtension.RoutineSupportedStatusQuery",
      ConvertToHistogramValue(tag));
}

}  // namespace chromeos

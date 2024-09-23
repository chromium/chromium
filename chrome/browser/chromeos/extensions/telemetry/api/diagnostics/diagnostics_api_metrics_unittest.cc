// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/diagnostics_api_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

using HistogramValue = DiagnosticRoutineCategoryHistogramValue;
using RoutineTag = crosapi::mojom::TelemetryDiagnosticRoutineArgument::Tag;

constexpr std::pair<RoutineTag, HistogramValue> kAllRoutineCategories[] = {
    {RoutineTag::kUnrecognizedArgument, HistogramValue::kUnknown},
    {RoutineTag::kMemory, HistogramValue::kMemory},
    {RoutineTag::kVolumeButton, HistogramValue::kVolumeButton},
    {RoutineTag::kFan, HistogramValue::kFan},
    {RoutineTag::kLedLitUp, HistogramValue::kLedLitUp},
    {RoutineTag::kNetworkBandwidth, HistogramValue::kNetworkBandwidth},
    {RoutineTag::kCameraFrameAnalysis, HistogramValue::kCameraFrameAnalysis},
    {RoutineTag::kKeyboardBacklight, HistogramValue::kKeyboardBacklight}};

static_assert(
    std::size(kAllRoutineCategories) ==
    static_cast<int>(DiagnosticRoutineCategoryHistogramValue::kMaxValue) + 1);

class RoutineCategoryTest
    : public testing::TestWithParam<std::pair<RoutineTag, HistogramValue>> {};

// Test routine creation can be recorded.
TEST_P(RoutineCategoryTest, RecordRoutineCreation) {
  constexpr std::string_view name =
      "ChromeOS.TelemetryExtension.RoutineCreation";
  auto sample = GetParam().second;
  base::HistogramTester tester;

  tester.ExpectBucketCount(name, sample, 0);
  RecordRoutineCreation(GetParam().first);
  tester.ExpectBucketCount(name, sample, 1);
}

// Test routine supported status query can be recorded.
TEST_P(RoutineCategoryTest, RecordRoutineSupportedStatusQuery) {
  constexpr std::string_view name =
      "ChromeOS.TelemetryExtension.RoutineSupportedStatusQuery";
  auto sample = GetParam().second;
  base::HistogramTester tester;

  tester.ExpectBucketCount(name, sample, 0);
  RecordRoutineSupportedStatusQuery(GetParam().first);
  tester.ExpectBucketCount(name, sample, 1);
}

INSTANTIATE_TEST_SUITE_P(TelemetryExtensionDiagnosticsApiMetricsUnitTest,
                         RoutineCategoryTest,
                         testing::ValuesIn(kAllRoutineCategories));

}  // namespace

}  // namespace chromeos

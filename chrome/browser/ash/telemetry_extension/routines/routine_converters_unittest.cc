// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/routines/routine_converters.h"

#include <utility>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::converters {

namespace {

namespace crosapi = ::crosapi::mojom;
namespace healthd = cros_healthd::mojom;

}  // namespace

// Tests that `ConvertRoutinePtr` function returns nullptr if input is
// nullptr. `ConvertRoutinePtr` is a template, so we can test this function
// with any valid type.
TEST(TelemetryDiagnosticRoutineConvertersTest, ConvertRoutinePtrTakesNullPtr) {
  EXPECT_TRUE(
      ConvertRoutinePtr(crosapi::TelemetryDiagnosticRoutineArgumentPtr())
          .is_null());
}

TEST(TelemetryDiagnosticRoutineConvertersTest, ConvertRoutineArgumentPtr) {
  auto input =
      crosapi::TelemetryDiagnosticRoutineArgument::NewUnrecognizedArgument(
          true);

  const auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_unrecognizedArgument());
  EXPECT_TRUE(result->get_unrecognizedArgument());
}

}  // namespace ash::converters

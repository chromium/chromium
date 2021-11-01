// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/diagnostics_metrics.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {
namespace metrics {
namespace {
class DiagnosticsMetricsTest : public testing::Test {
 public:
  DiagnosticsMetricsTest() = default;

  ~DiagnosticsMetricsTest() override = default;
};
}  // namespace

// DiagnosticsMetricsTest is part of the ash_unittests and will only be called
// when is_chromeos_ash is true. Eligible and enabled currently will always be
// true.
TEST_F(DiagnosticsMetricsTest, IsEligibleAndEnabled) {
  DiagnosticsMetrics metrics;

  EXPECT_TRUE(metrics.IsEligible());
  EXPECT_TRUE(metrics.IsEnabled());
}
}  // namespace metrics
}  // namespace diagnostics
}  // namespace ash

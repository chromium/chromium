// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

TEST(GlicMetrics, Basic) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  GlicMetrics metrics;
  metrics.OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics.OnResponseStarted();
  metrics.OnResponseStopped();
  metrics.OnResponseRated(/*positive=*/true);
  metrics.OnSessionTerminated();

  histogram_tester.ExpectTotalCount("Glic.Response.StartTime", 1);
  histogram_tester.ExpectTotalCount("Glic.Response.StopTime", 1);
  EXPECT_EQ(user_action_tester.GetActionCount("GlicResponse"), 1);
  EXPECT_EQ(user_action_tester.GetActionCount("GlicSessionEnd"), 1);
}

}  // namespace
}  // namespace glic

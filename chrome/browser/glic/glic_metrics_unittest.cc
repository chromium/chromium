// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

TEST(GlicMetrics, Basic) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  content::BrowserTaskEnvironment task_environment;

  TestingProfile profile;
  signin::IdentityTestEnvironment identity_env;
  // This code does not initialize the controller to show UI, so not all metrics
  // will be emitted.
  GlicWindowController controller(&profile, identity_env.identity_manager(),
                                  /*service=*/nullptr);
  GlicMetrics metrics(&controller);
  metrics.OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics.OnResponseStarted();
  metrics.OnResponseStopped();
  metrics.OnResponseRated(/*positive=*/true);
  metrics.OnSessionTerminated();

  histogram_tester.ExpectTotalCount("Glic.Response.StopTime", 1);
  EXPECT_EQ(user_action_tester.GetActionCount("GlicResponseInputSubmit"), 1);
  EXPECT_EQ(user_action_tester.GetActionCount("GlicResponseStart"), 1);
  EXPECT_EQ(user_action_tester.GetActionCount("GlicResponseStop"), 1);
}

}  // namespace
}  // namespace glic

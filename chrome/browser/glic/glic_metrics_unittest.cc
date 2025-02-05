// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

// This mock is a wrapper around the API in GlicWindowController which is
// exposed to GlicMetrics. It doesn't do anything.
class MockWindowController : public GlicWindowController {
 public:
  MockWindowController()
      : GlicWindowController(/*profile=*/nullptr, /*service=*/nullptr) {}
  ~MockWindowController() override = default;

  bool IsShowing() const override { return showing_; }
  bool IsAttached() override { return attached_; }
  bool showing_ = false;
  bool attached_ = false;
};

class GlicMetricsTest : public testing::Test {
 public:
  void SetUp() override {
    controller_ = std::make_unique<MockWindowController>();
    metrics_ = std::make_unique<GlicMetrics>();
    metrics_->SetWindowController(controller_.get());
  }

 protected:
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;

  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;

  std::unique_ptr<MockWindowController> controller_;
  std::unique_ptr<GlicMetrics> metrics_;
};

TEST_F(GlicMetricsTest, Basic) {
  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_->OnResponseStarted();
  metrics_->OnResponseStopped();
  metrics_->OnResponseRated(/*positive=*/true);
  metrics_->OnSessionTerminated();

  histogram_tester_.ExpectTotalCount("Glic.Response.StopTime", 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponseInputSubmit"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponseStart"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponseStop"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponse"), 0);
}

TEST_F(GlicMetricsTest, BasicVisible) {
  controller_->showing_ = true;
  controller_->attached_ = true;

  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_->OnResponseStarted();
  metrics_->OnResponseStopped();
  metrics_->OnResponseRated(/*positive=*/true);
  metrics_->OnSessionTerminated();

  histogram_tester_.ExpectTotalCount("Glic.Response.StopTime", 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponseInputSubmit"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponseStart"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponseStop"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponse"), 1);
}

}  // namespace
}  // namespace glic

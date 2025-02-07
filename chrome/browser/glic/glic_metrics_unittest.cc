// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

// This mock is a wrapper around the API in GlicWindowController which is
// exposed to GlicMetrics. It doesn't do anything.
class MockWindowController : public GlicWindowController {
 public:
  MockWindowController(Profile* profile,
                       signin::IdentityManager* identity_manager)
      : GlicWindowController(profile,
                             identity_manager,
                             /*service=*/nullptr) {}
  ~MockWindowController() override = default;

  bool IsShowing() const override { return showing_; }
  bool IsAttached() override { return attached_; }
  bool showing_ = false;
  bool attached_ = false;
};

class GlicMetricsTest : public testing::Test {
 public:
  GlicMetricsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  void SetUp() override {
    controller_ = std::make_unique<MockWindowController>(
        &profile_, identity_env_.identity_manager());
    metrics_ = std::make_unique<GlicMetrics>(&profile_);
    metrics_->SetWindowController(controller_.get());
  }

 protected:
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;

  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
  signin::IdentityTestEnvironment identity_env_;

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

  metrics_->OnGlicWindowOpen(/*attached=*/true, InvocationSource::kOsButton);
  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_->OnResponseStarted();
  metrics_->OnResponseStopped();
  metrics_->OnResponseRated(/*positive=*/true);
  metrics_->OnSessionTerminated();
  metrics_->OnGlicWindowClose();

  histogram_tester_.ExpectTotalCount("Glic.Response.StopTime", 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponseInputSubmit"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponseStart"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponseStop"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponse"), 1);
}

TEST_F(GlicMetricsTest, ImpressionBeforeFre) {
  profile_.GetPrefs()->SetBoolean(prefs::kGlicCompletedFre, false);

  task_environment_.FastForwardBy(base::Minutes(16));
  histogram_tester_.ExpectTotalCount("Glic.EntryPoint.Impression", 1);
  histogram_tester_.ExpectBucketCount("Glic.EntryPoint.Impression",
                                      /*kBeforeFre=*/0, /*expected_count=*/1);
}

TEST_F(GlicMetricsTest, ImpressionAfterFre) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {
          features::kGlic,
          features::kTabstripComboButton,
      },
      {});
  profile_.GetPrefs()->SetBoolean(prefs::kGlicCompletedFre, true);
  profile_.GetPrefs()->SetInteger(
      prefs::kGlicSettingsPolicy,
      static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled));

  task_environment_.FastForwardBy(base::Minutes(16));
  histogram_tester_.ExpectTotalCount("Glic.EntryPoint.Impression", 1);
  histogram_tester_.ExpectBucketCount("Glic.EntryPoint.Impression",
                                      /*kAfterFreGlicEnabled=*/1,
                                      /*expected_count=*/1);
}

}  // namespace
}  // namespace glic

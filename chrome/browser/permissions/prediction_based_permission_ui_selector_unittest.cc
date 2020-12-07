// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_based_permission_ui_selector.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using Decision = PredictionBasedPermissionUiSelector::Decision;
}  // namespace

class PredictionBasedPermissionUiSelectorTest : public testing::Test {
 public:
  PredictionBasedPermissionUiSelectorTest()
      : testing_profile_(std::make_unique<TestingProfile>()) {}

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kQuietNotificationPrompts, features::kPermissionPredictions},
        {});

    safe_browsing::SetSafeBrowsingState(
        testing_profile_->GetPrefs(),
        safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  }

  TestingProfile* profile() { return testing_profile_.get(); }

  Decision SelectUiToUseAndGetDecision(
      PredictionBasedPermissionUiSelector* selector) {
    base::Optional<Decision> actual_decision;
    base::RunLoop run_loop;

    permissions::MockPermissionRequest request(
        "request", permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
        permissions::PermissionRequestGestureType::GESTURE);

    selector->SelectUiToUse(
        &request, base::BindLambdaForTesting([&](const Decision& decision) {
          actual_decision = decision;
          run_loop.Quit();
        }));
    run_loop.Run();

    return actual_decision.value();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> testing_profile_;
};

TEST_F(PredictionBasedPermissionUiSelectorTest,
       CommandLineMocksDecisionCorrectly) {
  struct {
    const char* command_line_value;
    const Decision expected_decision;
  } const kTests[] = {
      {"very-unlikely", Decision(PredictionBasedPermissionUiSelector::
                                     QuietUiReason::kPredictedVeryUnlikelyGrant,
                                 Decision::ShowNoWarning())},
      {"unlikely", Decision::UseNormalUiAndShowNoWarning()},
      {"neutral", Decision::UseNormalUiAndShowNoWarning()},
      {"likely", Decision::UseNormalUiAndShowNoWarning()},
      {"very-likely", Decision::UseNormalUiAndShowNoWarning()},
  };

  for (const auto& test : kTests) {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "prediction-service-mock-likelihood", test.command_line_value);

    PredictionBasedPermissionUiSelector prediction_selector(profile());

    Decision decision = SelectUiToUseAndGetDecision(&prediction_selector);

    EXPECT_EQ(test.expected_decision.quiet_ui_reason, decision.quiet_ui_reason);
    EXPECT_EQ(test.expected_decision.warning_reason, decision.warning_reason);
  }
}

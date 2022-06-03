// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_based_permission_ui_selector.h"
#include <memory>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/permissions/permission_actions_history_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/request_type.h"
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
    InitFeatureList();

    safe_browsing::SetSafeBrowsingState(
        testing_profile_->GetPrefs(),
        safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  }

  void InitFeatureList(const std::string holdback_chance_string = "0") {
    if (feature_list_)
      feature_list_->Reset();
    feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    feature_list_->InitWithFeaturesAndParameters(
        {{features::kQuietNotificationPrompts, {}},
         {features::kPermissionPredictions,
          {{features::kPermissionPredictionsHoldbackChance.name,
            holdback_chance_string}}},
         {features::kPermissionGeolocationPredictions,
          {{features::kPermissionGeolocationPredictionsHoldbackChance.name,
            holdback_chance_string}}},
         {permissions::features::kPermissionQuietChip, {}}},
        {} /* disabled_features */);
  }

  void RecordHistoryActions(size_t action_count,
                            permissions::RequestType request_type) {
    while (action_count--) {
      PermissionActionsHistoryFactory::GetForProfile(profile())->RecordAction(
          permissions::PermissionAction::DENIED, request_type,
          permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);
    }
  }

  TestingProfile* profile() { return testing_profile_.get(); }

  Decision SelectUiToUseAndGetDecision(
      PredictionBasedPermissionUiSelector* selector,
      permissions::RequestType request_type) {
    absl::optional<Decision> actual_decision;
    base::RunLoop run_loop;

    permissions::MockPermissionRequest request(
        request_type, permissions::PermissionRequestGestureType::GESTURE);

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
  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;
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

  RecordHistoryActions(/*action_count=*/4,
                       permissions::RequestType::kNotifications);

  for (const auto& test : kTests) {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "prediction-service-mock-likelihood", test.command_line_value);

    PredictionBasedPermissionUiSelector prediction_selector(profile());

    Decision decision = SelectUiToUseAndGetDecision(
        &prediction_selector, permissions::RequestType::kNotifications);

    EXPECT_EQ(test.expected_decision.quiet_ui_reason, decision.quiet_ui_reason);
    EXPECT_EQ(test.expected_decision.warning_reason, decision.warning_reason);
  }
}

TEST_F(PredictionBasedPermissionUiSelectorTest,
       RequestsWithFewPromptsAreNotSent) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "prediction-service-mock-likelihood", "very-unlikely");
  PredictionBasedPermissionUiSelector prediction_selector(profile());

  // Requests that have 0-3 previous permission prompts will return "normal"
  // without making a prediction service request.
  for (size_t request_id = 0; request_id < 4; ++request_id) {
    Decision notification_decision = SelectUiToUseAndGetDecision(
        &prediction_selector, permissions::RequestType::kNotifications);

    Decision geolocation_decision = SelectUiToUseAndGetDecision(
        &prediction_selector, permissions::RequestType::kGeolocation);

    EXPECT_EQ(Decision::UseNormalUi(), notification_decision.quiet_ui_reason);
    EXPECT_EQ(Decision::UseNormalUi(), geolocation_decision.quiet_ui_reason);

    RecordHistoryActions(/*action_count=*/1,
                         permissions::RequestType::kNotifications);
    RecordHistoryActions(/*action_count=*/1,
                         permissions::RequestType::kGeolocation);
  }

  // Since there are 4 previous prompts, the prediction service request will be
  // made and will return a "kPredictedVeryUnlikelyGrant" quiet reason.
  Decision notification_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kNotifications);

  Decision geolocation_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kGeolocation);

  EXPECT_EQ(PredictionBasedPermissionUiSelector::QuietUiReason::
                kPredictedVeryUnlikelyGrant,
            notification_decision.quiet_ui_reason);

  EXPECT_EQ(PredictionBasedPermissionUiSelector::QuietUiReason::
                kPredictedVeryUnlikelyGrant,
            geolocation_decision.quiet_ui_reason);
}

TEST_F(PredictionBasedPermissionUiSelectorTest,
       OnlyPromptsForCurrentTypeAreCounted) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "prediction-service-mock-likelihood", "very-unlikely");
  PredictionBasedPermissionUiSelector prediction_selector(profile());

  // Set up history to need one more prompt before we actually send requests.
  RecordHistoryActions(/*action_count=*/3,
                       permissions::RequestType::kNotifications);
  RecordHistoryActions(/*action_count=*/3,
                       permissions::RequestType::kGeolocation);

  Decision notification_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kNotifications);

  Decision geolocation_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kGeolocation);

  EXPECT_EQ(Decision::UseNormalUi(), notification_decision.quiet_ui_reason);
  EXPECT_EQ(Decision::UseNormalUi(), geolocation_decision.quiet_ui_reason);

  // Record a bunch of history actions for other request types.
  RecordHistoryActions(/*action_count=*/2,
                       permissions::RequestType::kCameraStream);
  RecordHistoryActions(/*action_count=*/1,
                       permissions::RequestType::kClipboard);
  RecordHistoryActions(/*action_count=*/10,
                       permissions::RequestType::kMidiSysex);

  // Should still not send requests.
  notification_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kNotifications);
  EXPECT_EQ(Decision::UseNormalUi(), notification_decision.quiet_ui_reason);

  geolocation_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kNotifications);
  EXPECT_EQ(Decision::UseNormalUi(), geolocation_decision.quiet_ui_reason);

  // Record one more notification prompt, now it should send requests.
  RecordHistoryActions(1, permissions::RequestType::kNotifications);

  notification_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kNotifications);
  EXPECT_EQ(PredictionBasedPermissionUiSelector::QuietUiReason::
                kPredictedVeryUnlikelyGrant,
            notification_decision.quiet_ui_reason);

  // Geolocation still has too few actions.
  geolocation_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kGeolocation);
  EXPECT_EQ(Decision::UseNormalUi(), geolocation_decision.quiet_ui_reason);

  // Now both notifications and geolocation send requests.
  RecordHistoryActions(1, permissions::RequestType::kGeolocation);

  notification_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kNotifications);
  EXPECT_EQ(PredictionBasedPermissionUiSelector::QuietUiReason::
                kPredictedVeryUnlikelyGrant,
            notification_decision.quiet_ui_reason);

  geolocation_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kGeolocation);
  EXPECT_EQ(PredictionBasedPermissionUiSelector::QuietUiReason::
                kPredictedVeryUnlikelyGrant,
            geolocation_decision.quiet_ui_reason);
}

TEST_F(PredictionBasedPermissionUiSelectorTest, HoldbackChanceTakesEffect) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "prediction-service-mock-likelihood", "very-unlikely");

  PredictionBasedPermissionUiSelector prediction_selector(profile());

  for (const auto type : {permissions::RequestType::kNotifications,
                          permissions::RequestType::kGeolocation}) {
    RecordHistoryActions(/*action_count=*/4, type);

    EXPECT_EQ(PredictionBasedPermissionUiSelector::QuietUiReason::
                  kPredictedVeryUnlikelyGrant,
              SelectUiToUseAndGetDecision(&prediction_selector, type)
                  .quiet_ui_reason);
  }

  InitFeatureList("1" /* holdback_chance_string */);

  for (const auto type : {permissions::RequestType::kNotifications,
                          permissions::RequestType::kGeolocation}) {
    EXPECT_EQ(Decision::UseNormalUi(),
              SelectUiToUseAndGetDecision(&prediction_selector, type)
                  .quiet_ui_reason);
  }
}

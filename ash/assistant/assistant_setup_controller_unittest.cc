// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/test/test_assistant_setup.h"
#include "ash/public/cpp/assistant/assistant_setup.h"
#include "ui/views/view.h"

namespace ash {

namespace {

class AssistantSetupControllerTest : public AssistantAshTestBase {
 protected:
  // Invoke to finish opt-in flow with the desired state of completion. Note
  // that this API may only be called while opt-in flow is in progress.
  void FinishAssistantOptInFlow(bool completed) {
    DCHECK(AssistantSetup::GetInstance());
    static_cast<TestAssistantSetup*>(AssistantSetup::GetInstance())
        ->FinishAssistantOptInFlow(completed);
  }
};

}  // namespace

TEST_F(AssistantSetupControllerTest, ShouldCloseAssistantUiWhenOnboarding) {
  ShowAssistantUi(AssistantEntryPoint::kUnspecified);
  EXPECT_TRUE(IsVisible());

  // When Launcher Search IPH is enabled and it is not in zero state view, we
  // show the opt-in chips as needed.
  MockTextInteraction().WithTextResponse("The response");

  SetConsentStatus(assistant::prefs::ConsentStatus::kUnknown);
  EXPECT_TRUE(opt_in_view()->GetVisible());

  ClickOnAndWait(opt_in_view());

  EXPECT_FALSE(IsVisible());
}

TEST_F(AssistantSetupControllerTest,
       ShouldCloseAssistantUiWhenOnboardingInTabletMode) {
  SetTabletMode(true);

  ShowAssistantUi(AssistantEntryPoint::kUnspecified);
  EXPECT_TRUE(IsVisible());

  // When Launcher Search IPH is enabled and it is not in zero state view, we
  // show the opt-in chips as needed.
  // Show Assistant UI in text mode, which is required to set text query.
  TapOnAndWait(keyboard_input_toggle());
  MockTextInteraction().WithTextResponse("The response");

  SetConsentStatus(assistant::prefs::ConsentStatus::kUnknown);
  EXPECT_TRUE(opt_in_view()->GetVisible());

  ClickOnAndWait(opt_in_view());

  EXPECT_FALSE(IsVisible());
}

TEST_F(AssistantSetupControllerTest,
       ShouldNotRelaunchAssistantIfOptInFlowAborted) {
  ShowAssistantUi(AssistantEntryPoint::kUnspecified);
  EXPECT_TRUE(IsVisible());

  // When Launcher Search IPH is enabled and it is not in zero state view, we
  // show the opt-in chips as needed.
  MockTextInteraction().WithTextResponse("The response");

  SetConsentStatus(assistant::prefs::ConsentStatus::kUnknown);
  EXPECT_TRUE(opt_in_view()->GetVisible());

  ClickOnAndWait(opt_in_view());

  EXPECT_FALSE(IsVisible());

  FinishAssistantOptInFlow(/*completed=*/false);

  EXPECT_FALSE(IsVisible());
}

TEST_F(AssistantSetupControllerTest,
       ShouldRelaunchAssistantIfOptInFlowCompleted) {
  ShowAssistantUi(AssistantEntryPoint::kUnspecified);
  EXPECT_TRUE(IsVisible());

  // When Launcher Search IPH is enabled and it is not in zero state view, we
  // show the opt-in chips as needed.
  MockTextInteraction().WithTextResponse("The response");

  SetConsentStatus(assistant::prefs::ConsentStatus::kUnknown);
  EXPECT_TRUE(opt_in_view()->GetVisible());

  ClickOnAndWait(opt_in_view());

  EXPECT_FALSE(IsVisible());

  FinishAssistantOptInFlow(/*completed=*/true);

  EXPECT_TRUE(IsVisible());
}

}  // namespace ash

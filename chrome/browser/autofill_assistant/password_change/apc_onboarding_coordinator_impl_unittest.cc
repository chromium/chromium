// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_prompt.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_display_delegate.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_onboarding_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestApcOnboardingCoordinatorImpl : public ApcOnboardingCoordinatorImpl {
 public:
  TestApcOnboardingCoordinatorImpl(Profile* profile,
                                   AssistantDisplayDelegate* display_delegate);

  MOCK_METHOD(std::unique_ptr<AssistantOnboardingController>,
              CreateOnboardingController,
              (const AssistantOnboardingInformation&),
              (override));
  MOCK_METHOD(AssistantOnboardingPrompt*,
              CreateOnboardingPrompt,
              (AssistantOnboardingController*, AssistantDisplayDelegate*),
              (override));
};

TestApcOnboardingCoordinatorImpl::TestApcOnboardingCoordinatorImpl(
    Profile* profile,
    AssistantDisplayDelegate* display_delegate)
    : ApcOnboardingCoordinatorImpl(profile, display_delegate) {}

class ApcOnboardingCoordinatorImplTest : public ::testing::Test {
 public:
  ApcOnboardingCoordinatorImplTest() {
    coordinator_ = std::make_unique<TestApcOnboardingCoordinatorImpl>(
        &profile_, &display_delegate_);
  }

  TestApcOnboardingCoordinatorImpl* coordinator() { return coordinator_.get(); }
  PrefService* GetPrefs() { return profile_.GetPrefs(); }

 private:
  // Testing setup.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  // Mocks
  MockAssistantDisplayDelegate display_delegate_;

  // The object to be tested.
  std::unique_ptr<TestApcOnboardingCoordinatorImpl> coordinator_;
};

TEST_F(ApcOnboardingCoordinatorImplTest,
       PerformOnboardingWithPreviouslyGivenConsent) {
  // Simulate previously given consent.
  GetPrefs()->SetBoolean(prefs::kAutofillAssistantOnDesktopEnabled, true);

  base::MockCallback<ApcOnboardingCoordinator::Callback> callback;
  EXPECT_CALL(callback, Run(true));
  // Since onboarding was previously accepted, no dialog is ever created.
  EXPECT_CALL(*coordinator(), CreateOnboardingController).Times(0);

  coordinator()->PerformOnboarding(callback.Get());

  // Consent is still registered as in the pref.
  EXPECT_TRUE(
      GetPrefs()->GetBoolean(prefs::kAutofillAssistantOnDesktopEnabled));
}

TEST_F(ApcOnboardingCoordinatorImplTest, PerformOnboardingAndAccept) {
  // The default is false.
  EXPECT_FALSE(
      GetPrefs()->GetBoolean(prefs::kAutofillAssistantOnDesktopEnabled));

  // Create a mock controller.
  raw_ptr<MockAssistantOnboardingController> controller =
      new MockAssistantOnboardingController();
  EXPECT_CALL(*coordinator(), CreateOnboardingController)
      .WillOnce([controller]() {
        return base::WrapUnique<AssistantOnboardingController>(controller);
      });
  EXPECT_CALL(*coordinator(), CreateOnboardingPrompt);

  // Prepare to extract the callback to the controller.
  AssistantOnboardingController::Callback controller_callback;
  EXPECT_CALL(*controller, Show).WillOnce(MoveArg<1>(&controller_callback));

  // Start the onboarding.
  base::MockCallback<ApcOnboardingCoordinator::Callback> coordinator_callback;
  coordinator()->PerformOnboarding(coordinator_callback.Get());

  // And call the controller.
  ASSERT_TRUE(controller_callback);
  EXPECT_CALL(coordinator_callback, Run(true));
  std::move(controller_callback).Run(true);

  // Consent is saved in the pref.
  EXPECT_TRUE(
      GetPrefs()->GetBoolean(prefs::kAutofillAssistantOnDesktopEnabled));
}

TEST_F(ApcOnboardingCoordinatorImplTest, PerformOnboardingAndDecline) {
  // The default is false.
  EXPECT_FALSE(
      GetPrefs()->GetBoolean(prefs::kAutofillAssistantOnDesktopEnabled));

  // Create a mock controller.
  raw_ptr<MockAssistantOnboardingController> controller =
      new MockAssistantOnboardingController();
  EXPECT_CALL(*coordinator(), CreateOnboardingController)
      .WillOnce([controller]() {
        return base::WrapUnique<AssistantOnboardingController>(controller);
      });
  EXPECT_CALL(*coordinator(), CreateOnboardingPrompt);

  // Prepare to extract the callback to the controller.
  AssistantOnboardingController::Callback controller_callback;
  EXPECT_CALL(*controller, Show).WillOnce(MoveArg<1>(&controller_callback));

  // Start the onboarding.
  base::MockCallback<ApcOnboardingCoordinator::Callback> coordinator_callback;
  coordinator()->PerformOnboarding(coordinator_callback.Get());

  // And call the controller.
  ASSERT_TRUE(controller_callback);
  EXPECT_CALL(coordinator_callback, Run(false));
  std::move(controller_callback).Run(false);

  // Consent is saved in the pref.
  EXPECT_FALSE(
      GetPrefs()->GetBoolean(prefs::kAutofillAssistantOnDesktopEnabled));
}

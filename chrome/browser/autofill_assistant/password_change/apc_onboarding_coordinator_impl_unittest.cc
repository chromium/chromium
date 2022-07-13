// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_prompt.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_onboarding_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

constexpr char kUrl[] = "https://www.example.com";
constexpr char kOtherUrlWithSameDomain[] = "https://www.example.com/login";

class TestApcOnboardingCoordinatorImpl : public ApcOnboardingCoordinatorImpl {
 public:
  explicit TestApcOnboardingCoordinatorImpl(content::WebContents* web_contents);

  MOCK_METHOD(std::unique_ptr<AssistantOnboardingController>,
              CreateOnboardingController,
              (const AssistantOnboardingInformation&),
              (override));
  MOCK_METHOD(base::WeakPtr<AssistantOnboardingPrompt>,
              CreateOnboardingPrompt,
              (base::WeakPtr<AssistantOnboardingController>),
              (override));
};

TestApcOnboardingCoordinatorImpl::TestApcOnboardingCoordinatorImpl(
    content::WebContents* web_contents)
    : ApcOnboardingCoordinatorImpl(web_contents) {}

}  // namespace

class ApcOnboardingCoordinatorImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ApcOnboardingCoordinatorImplTest() = default;
  ~ApcOnboardingCoordinatorImplTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    coordinator_ =
        std::make_unique<TestApcOnboardingCoordinatorImpl>(web_contents());
  }

  TestApcOnboardingCoordinatorImpl* coordinator() { return coordinator_.get(); }
  PrefService* GetPrefs() { return profile()->GetPrefs(); }

 private:
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

TEST_F(ApcOnboardingCoordinatorImplTest,
       PerformOnboardingDuringOngoingNavigation) {
  // Simulate an ongoing navigation.
  web_contents()->GetController().LoadURL(
      GURL(kUrl), content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

  // Start the onboarding.
  base::MockCallback<ApcOnboardingCoordinator::Callback> coordinator_callback;
  coordinator()->PerformOnboarding(coordinator_callback.Get());

  // Expect these calls to happen once the navigation is finished.
  raw_ptr<MockAssistantOnboardingController> controller =
      new MockAssistantOnboardingController();
  EXPECT_CALL(*coordinator(), CreateOnboardingController)
      .WillOnce([controller]() {
        return base::WrapUnique<AssistantOnboardingController>(controller);
      });
  EXPECT_CALL(*coordinator(), CreateOnboardingPrompt);

  // Commit the navigation.
  content::WebContentsTester::For(web_contents())->CommitPendingNavigation();
}

TEST_F(ApcOnboardingCoordinatorImplTest,
       PerformOnboardingDuringOngoingNavigationToSameDomain) {
  // Simulate a previous navigation.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kUrl), ui::PAGE_TRANSITION_LINK);
  // Simulate an ongoing navigation.
  web_contents()->GetController().LoadURL(
      GURL(kOtherUrlWithSameDomain), content::Referrer(),
      ui::PAGE_TRANSITION_LINK, std::string());

  // Expect these calls to happen immediately sine the navigation is within
  // the same domain.
  raw_ptr<MockAssistantOnboardingController> controller =
      new MockAssistantOnboardingController();
  EXPECT_CALL(*coordinator(), CreateOnboardingController)
      .WillOnce([controller]() {
        return base::WrapUnique<AssistantOnboardingController>(controller);
      });
  EXPECT_CALL(*coordinator(), CreateOnboardingPrompt);

  // Start the onboarding.
  base::MockCallback<ApcOnboardingCoordinator::Callback> coordinator_callback;
  coordinator()->PerformOnboarding(coordinator_callback.Get());
}

TEST_F(ApcOnboardingCoordinatorImplTest,
       PerformOnboardingDuringOngoingNavigationThatDoesNotFinish) {
  // Simulate an ongoing navigation.
  web_contents()->GetController().LoadURL(
      GURL(kUrl), content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

  // Start the onboarding.
  base::MockCallback<ApcOnboardingCoordinator::Callback> coordinator_callback;
  coordinator()->PerformOnboarding(coordinator_callback.Get());

  // No prompt is ever created if the navigation does not finish.
  EXPECT_CALL(*coordinator(), CreateOnboardingController).Times(0);
  EXPECT_CALL(*coordinator(), CreateOnboardingPrompt).Times(0);
}

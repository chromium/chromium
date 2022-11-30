// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_prompt.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_onboarding_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill_assistant/browser/public/prefs.h"
#include "components/consent_auditor/fake_consent_auditor.h"
#include "components/prefs/pref_service.h"
#include "components/sync/protocol/user_consent_specifics.pb.h"
#include "components/sync/protocol/user_consent_types.pb.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

namespace {

constexpr char kUrl[] = "https://www.example.com";
constexpr char kOtherUrlWithSameDomain[] = "https://www.example.com/login";

constexpr int kRevokationDescriptionId1 = 234;
constexpr int kRevokationDescriptionId2 = 356;

using consent_auditor::FakeConsentAuditor;

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

FakeConsentAuditor* CreateAndUseFakeConsentAuditor(Profile* profile) {
  return static_cast<FakeConsentAuditor*>(
      ConsentAuditorFactory::GetInstance()->SetTestingSubclassFactoryAndUse(
          profile, base::BindRepeating([](content::BrowserContext*) {
            return std::make_unique<FakeConsentAuditor>();
          })));
}

}  // namespace

class ApcOnboardingCoordinatorImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ApcOnboardingCoordinatorImplTest() = default;
  ~ApcOnboardingCoordinatorImplTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    consent_auditor_ = CreateAndUseFakeConsentAuditor(profile());
    coordinator_ =
        std::make_unique<TestApcOnboardingCoordinatorImpl>(web_contents());
  }

  FakeConsentAuditor* consent_auditor() { return consent_auditor_; }
  TestApcOnboardingCoordinatorImpl* coordinator() { return coordinator_.get(); }
  PrefService* GetPrefs() { return profile()->GetPrefs(); }

 private:
  // Helper objects.
  raw_ptr<FakeConsentAuditor> consent_auditor_ = nullptr;

  // The object to be tested.
  std::unique_ptr<TestApcOnboardingCoordinatorImpl> coordinator_;
};

TEST_F(ApcOnboardingCoordinatorImplTest,
       PerformOnboardingWithPreviouslyGivenConsent) {
  // Simulate previously given consent.
  GetPrefs()->SetBoolean(autofill_assistant::prefs::kAutofillAssistantConsent,
                         true);

  base::MockCallback<ApcOnboardingCoordinator::Callback> callback;
  EXPECT_CALL(callback, Run(true));
  // Since onboarding was previously accepted, no dialog is ever created.
  EXPECT_CALL(*coordinator(), CreateOnboardingController).Times(0);

  coordinator()->PerformOnboarding(callback.Get());

  // Consent is still registered as in the pref.
  EXPECT_TRUE(GetPrefs()->GetBoolean(
      autofill_assistant::prefs::kAutofillAssistantConsent));
}

TEST_F(ApcOnboardingCoordinatorImplTest, PerformOnboardingAndAccept) {
  // The default is false.
  EXPECT_FALSE(GetPrefs()->GetBoolean(
      autofill_assistant::prefs::kAutofillAssistantConsent));

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
  // Use sample model data for the callback.
  const AssistantOnboardingInformation model =
      ApcOnboardingCoordinator::CreateOnboardingInformation();
  std::move(controller_callback)
      .Run(true, model.button_accept_text_id,
           {model.title_id, model.description_id, model.consent_text_id,
            model.learn_more_title_id});

  // Consent is saved in the pref.
  EXPECT_TRUE(GetPrefs()->GetBoolean(
      autofill_assistant::prefs::kAutofillAssistantConsent));

  // Consent is also recorded via the `ConsentAuditor`.
  ASSERT_THAT(consent_auditor()->recorded_consents(), SizeIs(1));
  const sync_pb::UserConsentSpecifics& consent_specifics =
      consent_auditor()->recorded_consents().front();
  ASSERT_TRUE(consent_specifics.has_autofill_assistant_consent());
  EXPECT_EQ(consent_specifics.autofill_assistant_consent().status(),
            sync_pb::UserConsentTypes::ConsentStatus::
                UserConsentTypes_ConsentStatus_GIVEN);
  EXPECT_TRUE(
      consent_specifics.autofill_assistant_consent().has_confirmation_grd_id());
  EXPECT_THAT(
      consent_specifics.autofill_assistant_consent().description_grd_ids(),
      Not(IsEmpty()));
}

TEST_F(ApcOnboardingCoordinatorImplTest, PerformOnboardingAndDecline) {
  // The default is false.
  EXPECT_FALSE(GetPrefs()->GetBoolean(
      autofill_assistant::prefs::kAutofillAssistantConsent));

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
  std::move(controller_callback).Run(false, absl::nullopt, {});

  // Consent is saved in the pref.
  EXPECT_FALSE(GetPrefs()->GetBoolean(
      autofill_assistant::prefs::kAutofillAssistantConsent));
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

TEST_F(ApcOnboardingCoordinatorImplTest, RevokeConsent) {
  // Simulate previously given consent.
  GetPrefs()->SetBoolean(autofill_assistant::prefs::kAutofillAssistantConsent,
                         true);

  coordinator()->RevokeConsent(
      {kRevokationDescriptionId1, kRevokationDescriptionId2});

  // Consent is now revoked.
  EXPECT_FALSE(GetPrefs()->GetBoolean(
      autofill_assistant::prefs::kAutofillAssistantConsent));

  // Consent is also recorded via the `ConsentAuditor`.
  ASSERT_THAT(consent_auditor()->recorded_consents(), SizeIs(1));
  const sync_pb::UserConsentSpecifics& consent_specifics =
      consent_auditor()->recorded_consents().front();
  ASSERT_TRUE(consent_specifics.has_autofill_assistant_consent());
  EXPECT_EQ(consent_specifics.autofill_assistant_consent().status(),
            sync_pb::UserConsentTypes::ConsentStatus::
                UserConsentTypes_ConsentStatus_NOT_GIVEN);
  EXPECT_FALSE(
      consent_specifics.autofill_assistant_consent().has_confirmation_grd_id());
  EXPECT_THAT(
      consent_specifics.autofill_assistant_consent().description_grd_ids(),
      SizeIs(2));
}

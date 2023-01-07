// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/fast_checkout/fast_checkout_external_action_delegate.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/fast_checkout_delegate.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill_assistant/browser/public/headless_onboarding_result.h"
#include "components/autofill_assistant/browser/public/mock_headless_script_controller.h"
#include "components/autofill_assistant/browser/public/mock_runtime_manager.h"
#include "ui/gfx/native_widget_types.h"

using ::autofill::AutofillDriver;
using ::autofill::AutofillProfile;
using ::autofill::CreditCard;
using ::testing::_;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

namespace {

CreditCard GetEmptyCreditCard() {
  CreditCard credit_card(base::GenerateGUID(), "");
  autofill::test::SetCreditCardInfo(&credit_card, /*name_on_card=*/"",
                                    /*card_number=*/"",
                                    autofill::test::NextMonth().c_str(),
                                    autofill::test::NextYear().c_str(), "1");
  return credit_card;
}

constexpr char kUrl[] = "https://www.example.com";
const AutofillProfile kProfile1 = autofill::test::GetFullProfile();
const AutofillProfile kProfile2 = autofill::test::GetFullProfile2();
const AutofillProfile kIncompleteProfile =
    autofill::test::GetIncompleteProfile1();
const CreditCard kCreditCard1 = autofill::test::GetCreditCard();
const CreditCard kCreditCard2 = autofill::test::GetCreditCard2();
const CreditCard kEmptyCreditCard = GetEmptyCreditCard();

std::unique_ptr<KeyedService> BuildTestPersonalDataManager(
    content::BrowserContext* context) {
  auto personal_data_manager =
      std::make_unique<autofill::TestPersonalDataManager>();
  personal_data_manager->SetAutofillProfileEnabled(true);
  personal_data_manager->SetAutofillCreditCardEnabled(true);
  personal_data_manager->SetAutofillWalletImportEnabled(true);
  personal_data_manager->AddProfile(kProfile1);
  personal_data_manager->AddProfile(kProfile2);
  // Add incomplete autofill profile, should not be shown on the sheet.
  personal_data_manager->AddProfile(kIncompleteProfile);
  personal_data_manager->AddCreditCard(kCreditCard1);
  personal_data_manager->AddCreditCard(kCreditCard2);
  // Add empty credit card, should not be shown on the sheet.
  personal_data_manager->AddCreditCard(kEmptyCreditCard);
  return personal_data_manager;
}

struct SupportsConsentlessExecution {
  bool client_supports_consentless = false;
  bool script_supports_consentless = false;
  bool run_consentless = false;
};

}  // namespace

class MockFastCheckoutController : public FastCheckoutController {
 public:
  MockFastCheckoutController() : FastCheckoutController() {}
  ~MockFastCheckoutController() override = default;

  MOCK_METHOD(void,
              Show,
              (const std::vector<AutofillProfile*>& autofill_profiles,
               const std::vector<CreditCard*>& credit_cards),
              (override));
  MOCK_METHOD(void,
              OnOptionsSelected,
              (std::unique_ptr<AutofillProfile> profile,
               std::unique_ptr<CreditCard> credit_card),
              (override));
  MOCK_METHOD(void, OnDismiss, (), (override));
  MOCK_METHOD(void, OpenAutofillProfileSettings, (), (override));
  MOCK_METHOD(void, OpenCreditCardSettings, (), (override));
  MOCK_METHOD(gfx::NativeView, GetNativeView, (), (override));
};

class MockAutofillDriver : public autofill::TestAutofillDriver {
 public:
  MockAutofillDriver() = default;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;

  // Mock methods to enable testability.
  MOCK_METHOD(void, SetShouldSuppressKeyboard, (bool), (override));
};

class MockFastCheckoutDelegate : public autofill::FastCheckoutDelegate {
 public:
  explicit MockFastCheckoutDelegate(MockAutofillDriver* driver)
      : driver_(driver) {}
  ~MockFastCheckoutDelegate() override = default;

  MOCK_METHOD(bool,
              TryToShowFastCheckout,
              (const autofill::FormData&, const autofill::FormFieldData&),
              (override));
  MOCK_METHOD(bool, IsShowingFastCheckoutUI, (), (const, override));
  MOCK_METHOD(void, HideFastCheckoutUI, (), (override));
  MOCK_METHOD(void, OnFastCheckoutUIHidden, (), (override));
  MOCK_METHOD(void, Reset, (), (override));

  AutofillDriver* GetDriver() override { return driver_; }

  base::WeakPtr<MockFastCheckoutDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  const raw_ptr<MockAutofillDriver> driver_;
  base::WeakPtrFactory<MockFastCheckoutDelegate> weak_factory_{this};
};

class MockFastCheckoutExternalActionDelegate
    : public FastCheckoutExternalActionDelegate {
 public:
  MockFastCheckoutExternalActionDelegate() = default;
  ~MockFastCheckoutExternalActionDelegate() override = default;

  MOCK_METHOD(void,
              SetOptionsSelected,
              (const AutofillProfile& selected_profile,
               const CreditCard& selected_credit_card),
              (override));
};

class TestFastCheckoutClientImpl : public FastCheckoutClientImpl {
 public:
  static TestFastCheckoutClientImpl* CreateForWebContents(
      content::WebContents* web_contents);

  explicit TestFastCheckoutClientImpl(content::WebContents* web_contents)
      : FastCheckoutClientImpl(web_contents) {}

  std::unique_ptr<autofill_assistant::HeadlessScriptController>
  CreateHeadlessScriptController() override {
    return std::move(external_script_controller_);
  }

  void InjectHeadlessScriptControllerForTesting(
      std::unique_ptr<autofill_assistant::HeadlessScriptController>
          external_script_controller) {
    external_script_controller_ = std::move(external_script_controller);
  }

  std::unique_ptr<FastCheckoutController> CreateFastCheckoutController()
      override {
    return std::move(fast_checkout_controller_);
  }

  void InjectFastCheckoutController(
      std::unique_ptr<FastCheckoutController> fast_checkout_controller) {
    fast_checkout_controller_ = std::move(fast_checkout_controller);
  }

  autofill_assistant::RuntimeManager* GetRuntimeManager() override {
    return runtime_manager_;
  }

  // Allows setting an RunTimeManager.
  void InjectRunTimeManagerForTesting(
      autofill_assistant::RuntimeManager* runtime_manager) {
    runtime_manager_ = runtime_manager;
  }

  std::unique_ptr<FastCheckoutExternalActionDelegate>
  CreateFastCheckoutExternalActionDelegate() override {
    return std::move(external_action_delegate_);
  }

  void InjectFastCheckoutExternalActionDelegate(
      std::unique_ptr<FastCheckoutExternalActionDelegate>
          external_action_delegate) {
    external_action_delegate_ = std::move(external_action_delegate);
  }

 private:
  std::unique_ptr<autofill_assistant::HeadlessScriptController>
      external_script_controller_;
  std::unique_ptr<FastCheckoutController> fast_checkout_controller_;
  std::unique_ptr<FastCheckoutExternalActionDelegate> external_action_delegate_;
  autofill_assistant::RuntimeManager* runtime_manager_;
};

// static
TestFastCheckoutClientImpl* TestFastCheckoutClientImpl::CreateForWebContents(
    content::WebContents* web_contents) {
  const void* key = WebContentsUserData<FastCheckoutClientImpl>::UserDataKey();
  web_contents->SetUserData(
      key, std::make_unique<TestFastCheckoutClientImpl>(web_contents));
  return static_cast<TestFastCheckoutClientImpl*>(
      web_contents->GetUserData(key));
}

class FastCheckoutClientImplTest : public ChromeRenderViewHostTestHarness {
 public:
  FastCheckoutClientImplTest() {
    feature_list_.InitWithFeatures({features::kFastCheckout}, {});
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    autofill::PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(),
        base::BindRepeating(&BuildTestPersonalDataManager));

    test_client_ =
        TestFastCheckoutClientImpl::CreateForWebContents(web_contents());

    // Prepare the HeadlessScriptController.
    auto external_script_controller =
        std::make_unique<autofill_assistant::MockHeadlessScriptController>();
    external_script_controller_ = external_script_controller.get();
    test_client_->InjectHeadlessScriptControllerForTesting(
        std::move(external_script_controller));

    // Prepare the FastCheckoutController.
    auto fast_checkout_controller =
        std::make_unique<MockFastCheckoutController>();
    fast_checkout_controller_ = fast_checkout_controller.get();
    test_client_->InjectFastCheckoutController(
        std::move(fast_checkout_controller));

    // Prepare the FastCheckoutExternalActionDelegate.
    auto external_action_delegate =
        std::make_unique<MockFastCheckoutExternalActionDelegate>();
    external_action_delegate_ = external_action_delegate.get();
    test_client_->InjectFastCheckoutExternalActionDelegate(
        std::move(external_action_delegate));

    // Prepare the FastCheckoutDelegate.
    autofill_driver_ = std::make_unique<MockAutofillDriver>();
    fast_checkout_delegate_ =
        std::make_unique<MockFastCheckoutDelegate>(autofill_driver_.get());

    mock_runtime_manager_ =
        std::make_unique<autofill_assistant::MockRuntimeManager>();

    // Prepare the RunTimeManager.
    test_client_->InjectRunTimeManagerForTesting(mock_runtime_manager_.get());
  }

  autofill::TestPersonalDataManager* personal_data_manager() {
    return static_cast<autofill::TestPersonalDataManager*>(
        autofill::PersonalDataManagerFactory::GetForProfile(profile()));
  }

  TestFastCheckoutClientImpl* fast_checkout_client() { return test_client_; }

  autofill_assistant::MockHeadlessScriptController*
  external_script_controller() {
    return external_script_controller_;
  }

  MockFastCheckoutController* fast_checkout_controller() {
    return fast_checkout_controller_;
  }

  MockFastCheckoutExternalActionDelegate* external_action_delegate() {
    return external_action_delegate_;
  }

  MockAutofillDriver* autofill_driver() { return autofill_driver_.get(); }

  base::WeakPtr<MockFastCheckoutDelegate> delegate() {
    return fast_checkout_delegate_->GetWeakPtr();
  }

  autofill_assistant::MockRuntimeManager* runtime_manager() {
    return mock_runtime_manager_.get();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;

  raw_ptr<autofill_assistant::MockHeadlessScriptController>
      external_script_controller_;
  raw_ptr<MockFastCheckoutController> fast_checkout_controller_;
  raw_ptr<MockFastCheckoutExternalActionDelegate> external_action_delegate_;
  std::unique_ptr<MockAutofillDriver> autofill_driver_;
  std::unique_ptr<MockFastCheckoutDelegate> fast_checkout_delegate_;
  std::unique_ptr<autofill_assistant::MockRuntimeManager> mock_runtime_manager_;
  raw_ptr<TestFastCheckoutClientImpl> test_client_;
};

class FastCheckoutClientImplTestParametrized
    : public FastCheckoutClientImplTest,
      public testing::WithParamInterface<SupportsConsentlessExecution> {};

const SupportsConsentlessExecution test_values[] = {
    {/*client_supports_consentless=*/true,
     /*script_supports_consentless=*/true,
     /*run_consentless=*/true},
    {/*client_supports_consentless=*/false,
     /*script_supports_consentless=*/true,
     /*run_consentless=*/false},
    {/*client_supports_consentless=*/false,
     /*script_supports_consentless=*/false,
     /*run_consentless=*/false}};
INSTANTIATE_TEST_SUITE_P(FastCheckoutClientImplTest,
                         FastCheckoutClientImplTestParametrized,
                         ::testing::ValuesIn(test_values));

TEST_F(
    FastCheckoutClientImplTest,
    GetOrCreateForWebContents_ClientWasAlreadyCreated_ReturnsExistingInstance) {
  raw_ptr<FastCheckoutClient> client =
      FastCheckoutClient::GetOrCreateForWebContents(web_contents());

  // There is only one client per `WebContents`.
  EXPECT_EQ(client, fast_checkout_client());
}

TEST_F(FastCheckoutClientImplTest, Start_FeatureDisabled_NoRuns) {
  // Disable Fast Checkout feature
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kFastCheckout});

  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Do not expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(0);

  EXPECT_CALL(*delegate(), OnFastCheckoutUIHidden).Times(0);
  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard).Times(0);

  // Starting is not successful which is also represented by the internal state.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl), false));
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest,
       Start_ConsentlessClientAttempsRunningScriptRequiringConsent_NoRuns) {
  // Enable Fast Checkout feature with consentless execution.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kFastCheckout,
      {{features::kFastCheckoutConsentlessExecutionParam.name, "true"}});

  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Do not expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(0);

  EXPECT_CALL(*delegate(), OnFastCheckoutUIHidden).Times(0);

  // Starting is not successful which is also represented by the internal state.
  EXPECT_FALSE(fast_checkout_client()->Start(
      delegate(), GURL(kUrl), /*script_supports_consentless_execution=*/false));
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_P(FastCheckoutClientImplTestParametrized,
       Start_FeatureEnabled_RunsSuccessfully) {
  // Enable or disable the consentless execution feature flag parameter
  // according to the test parameter. Note that the Fast Checkout feature flag
  // is intended to be always enabled in this test case.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kFastCheckout,
      {{features::kFastCheckoutConsentlessExecutionParam.name,
        GetParam().client_supports_consentless ? "true" : "false"}});

  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Prepare to extract the callbacks to the external script controller.
  base::OnceCallback<void(
      autofill_assistant::HeadlessScriptController::ScriptResult)>
      external_script_controller_callback;
  base::OnceCallback<void()> onboarding_successful_callback;

  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard(true));
  EXPECT_CALL(*external_script_controller(),
              StartScript(_, _, /*use_autofill_assistant_onboarding=*/
                          !GetParam().run_consentless, _,
                          /*suppress_browsing_features=*/false))
      .Times(1)
      .WillOnce(
          [&](const base::flat_map<std::string, std::string>& script_parameters,
              base::OnceCallback<void(
                  autofill_assistant::HeadlessScriptController::ScriptResult)>
                  script_ended_callback,
              bool use_autofill_assistant_onboarding,
              base::OnceCallback<void()>
                  onboarding_successful_callback_parameter,
              bool suppress_browsing_features) {
            external_script_controller_callback =
                std::move(script_ended_callback);
            onboarding_successful_callback =
                std::move(onboarding_successful_callback_parameter);
          });

  // Expect bottomsheet to show up.
  EXPECT_CALL(
      *fast_checkout_controller(),
      Show(UnorderedElementsAre(Pointee(kProfile1), Pointee(kProfile2),
                                Pointee(kIncompleteProfile)),
           UnorderedElementsAre(Pointee(kCreditCard1), Pointee(kCreditCard2))));

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(
      delegate(), GURL(kUrl), GetParam().script_supports_consentless));

  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

  // Cannot start another run.
  EXPECT_FALSE(fast_checkout_client()->Start(
      delegate(), GURL(kUrl), GetParam().script_supports_consentless));

  // After the bottom sheet is dismissed, keyboard suppression is disabled.
  // Normally `OnHidden` would get called, but it is also stopped on script end.
  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard(false)).Times(2);

  // Successful onboarding.
  EXPECT_CALL(*runtime_manager(),
              SetUIState(autofill_assistant::UIState::
                             kShownWithoutBrowsingFeatureSuppression));
  std::move(onboarding_successful_callback).Run();

  // Successful run.
  EXPECT_CALL(*runtime_manager(),
              SetUIState(autofill_assistant::UIState::kNotShown));
  autofill_assistant::HeadlessScriptController::ScriptResult script_result = {
      /* success= */ true};
  std::move(external_script_controller_callback).Run(script_result);

  // `FastCheckoutClient` state was reset after run finished.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  histogram_tester_.ExpectUniqueSample(kUmaKeyFastCheckoutRunOutcome,
                                       FastCheckoutRunOutcome::kSuccess, 1u);
}

TEST_F(FastCheckoutClientImplTest, Start_OnboardingRejected_NotStartableAgain) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Prepare to extract the callbacks to the external script controller.
  base::OnceCallback<void(
      autofill_assistant::HeadlessScriptController::ScriptResult)>
      external_script_controller_callback;

  EXPECT_CALL(*external_script_controller(),
              StartScript(_, _, /*use_autofill_assistant_onboarding=*/
                          true, _,
                          /*suppress_browsing_features=*/false))
      .Times(1)
      .WillOnce(
          [&](const base::flat_map<std::string, std::string>& script_parameters,
              base::OnceCallback<void(
                  autofill_assistant::HeadlessScriptController::ScriptResult)>
                  script_ended_callback,
              bool use_autofill_assistant_onboarding,
              base::OnceCallback<void()>
                  onboarding_successful_callback_parameter,
              bool suppress_browsing_features) {
            external_script_controller_callback =
                std::move(script_ended_callback);
          });

  // Expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show(_, _)).Times(0);

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl), false));

  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

  // Cannot start another run.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl), false));

  // Rejected onboarding.
  autofill_assistant::HeadlessScriptController::ScriptResult script_result = {
      /* success= */ false, /* onboarding_result= */ autofill_assistant::
          HeadlessOnboardingResult::kRejected};
  std::move(external_script_controller_callback).Run(script_result);

  // `FastCheckoutClient` state was reset after onboarding was rejected.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Not startable again.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl), false));
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  histogram_tester_.ExpectUniqueSample(
      kUmaKeyFastCheckoutRunOutcome,
      FastCheckoutRunOutcome::kOnboardingDeclined, 1u);
}

TEST_F(FastCheckoutClientImplTest, Start_FailsIfNoProfilesOnFile) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Remove all profiles.
  personal_data_manager()->ClearProfiles();

  EXPECT_CALL(*external_script_controller(),
              StartScript(_, _, /*use_autofill_assistant_onboarding=*/true, _,
                          /*suppress_browsing_features=*/false))
      .Times(0);
  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard).Times(0);

  // Starting the run unsuccessfully.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl), false));

  // `FastCheckoutClient` is not running.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  histogram_tester_.ExpectUniqueSample(
      autofill::kUmaKeyFastCheckoutTriggerOutcome,
      autofill::FastCheckoutTriggerOutcome::kFailureNoValidAutofillProfile, 1u);
}

TEST_F(FastCheckoutClientImplTest, Start_FailsIfNoCompleteProfile) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Remove all Profiles.
  personal_data_manager()->ClearProfiles();
  personal_data_manager()->AddProfile(autofill::test::GetIncompleteProfile1());
  personal_data_manager()->AddProfile(autofill::test::GetIncompleteProfile2());

  EXPECT_CALL(*external_script_controller(),
              StartScript(_, _, /*use_autofill_assistant_onboarding=*/true, _,
                          /*suppress_browsing_features=*/false))
      .Times(0);
  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard).Times(0);

  // Starting the run unsuccessfully.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl), false));

  // `FastCheckoutClient` is not running.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  histogram_tester_.ExpectUniqueSample(
      autofill::kUmaKeyFastCheckoutTriggerOutcome,
      autofill::FastCheckoutTriggerOutcome::kFailureNoValidAutofillProfile, 1u);
}

TEST_F(FastCheckoutClientImplTest, Start_FailsIfNoCreditCardsOnFile) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Remove all credit cards.
  personal_data_manager()->ClearCreditCards();

  EXPECT_CALL(*external_script_controller(),
              StartScript(_, _, /*use_autofill_assistant_onboarding=*/true, _,
                          /*suppress_browsing_features=*/false))
      .Times(0);
  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard).Times(0);

  // Starting the run unsuccessfully.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl), false));

  // `FastCheckoutClient` is not running.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  histogram_tester_.ExpectUniqueSample(
      autofill::kUmaKeyFastCheckoutTriggerOutcome,
      autofill::FastCheckoutTriggerOutcome::kFailureNoValidCreditCard, 1u);
}

TEST_F(FastCheckoutClientImplTest, Start_FailsIfNoCompleteorValidCreditCard) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Remove all credit Cards.
  personal_data_manager()->ClearCreditCards();
  personal_data_manager()->AddCreditCard(
      autofill::test::GetExpiredCreditCard());
  personal_data_manager()->AddCreditCard(
      autofill::test::GetIncompleteCreditCard());

  EXPECT_CALL(*external_script_controller(),
              StartScript(_, _, /*use_autofill_assistant_onboarding=*/true, _,
                          /*suppress_browsing_features=*/false))
      .Times(0);
  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard).Times(0);

  // Starting the run unsuccessfully.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl), false));

  // `FastCheckoutClient` is not running.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  histogram_tester_.ExpectUniqueSample(
      autofill::kUmaKeyFastCheckoutTriggerOutcome,
      autofill::FastCheckoutTriggerOutcome::kFailureNoValidCreditCard, 1u);
}

TEST_F(FastCheckoutClientImplTest,
       OnPersonalDataChanged_StopIfInvalidProfiles) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Expect bottomsheet to show up.
  EXPECT_CALL(*external_script_controller(),
              StartScript(_, _, /*use_autofill_assistant_onboarding=*/true, _,
                          /*suppress_browsing_features=*/false))
      .Times(1);

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl), false));

  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

  // Bottom sheet UI is showing.
  ON_CALL(*delegate(), IsShowingFastCheckoutUI).WillByDefault(Return(true));

  // User removes all the profiles.
  personal_data_manager()->ClearProfiles();
  // User adds an incomplete profile only.
  personal_data_manager()->AddProfile(autofill::test::GetIncompleteProfile1());

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest,
       OnPersonalDataChanged_StopIfInvalidCreditCards) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Expect bottomsheet to show up.
  EXPECT_CALL(*external_script_controller(),
              StartScript(_, _, /*use_autofill_assistant_onboarding=*/true, _,
                          /*suppress_browsing_features=*/false))
      .Times(1);

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl), false));

  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

  // Bottom sheet UI is showing.
  ON_CALL(*delegate(), IsShowingFastCheckoutUI).WillByDefault(Return(true));

  // User removes all valid credit cards and adds an incomplete one.
  personal_data_manager()->ClearCreditCards();
  personal_data_manager()->AddCreditCard(
      autofill::test::GetIncompleteCreditCard());

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest,
       OnPersonalDataChanged_UpdatesTheUIWithNewData) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  base::OnceCallback<void()> onboarding_successful_callback;
  // Expect bottomsheet to show up.
  EXPECT_CALL(*external_script_controller(),
              StartScript(_, _, /*use_autofill_assistant_onboarding=*/true, _,
                          /*suppress_browsing_features=*/false))
      .Times(1)
      .WillOnce(
          [&](const base::flat_map<std::string, std::string>& script_parameters,
              base::OnceCallback<void(
                  autofill_assistant::HeadlessScriptController::ScriptResult)>
                  script_ended_callback,
              bool use_autofill_assistant_onboarding,
              base::OnceCallback<void()>
                  onboarding_successful_callback_parameter,
              bool suppress_browsing_features) {
            onboarding_successful_callback =
                std::move(onboarding_successful_callback_parameter);
          });

  EXPECT_CALL(
      *fast_checkout_controller(),
      Show(UnorderedElementsAre(Pointee(kProfile1), Pointee(kProfile2),
                                Pointee(kIncompleteProfile)),
           UnorderedElementsAre(Pointee(kCreditCard1), Pointee(kCreditCard2))));

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl), false));

  // User accepts the onboarding.
  std::move(onboarding_successful_callback).Run();

  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

  // Bottom sheet UI is showing.
  ON_CALL(*delegate(), IsShowingFastCheckoutUI).WillByDefault(Return(true));

  // Expect bottomsheet to display the updated info.
  EXPECT_CALL(*fast_checkout_controller(),
              Show(UnorderedElementsAre(Pointee(kProfile1), Pointee(kProfile2),
                                        Pointee(kIncompleteProfile)),
                   UnorderedElementsAre(Pointee(kCreditCard1))));

  // User removes all valid credit cards and adds a valid card.
  personal_data_manager()->ClearCreditCards();
  personal_data_manager()->AddCreditCard(kCreditCard1);

  // `FastCheckoutClient` is still running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest,
       Start_OnboardingNotSuccessful_BottomsheetNotShowing) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Prepare to extract the callbacks to the external script controller.
  base::OnceCallback<void(
      autofill_assistant::HeadlessScriptController::ScriptResult)>
      external_script_controller_callback;
  EXPECT_CALL(*external_script_controller(),
              StartScript(_, _, /*use_autofill_assistant_onboarding=*/true, _,
                          /*suppress_browsing_features=*/false))
      .Times(1)
      .WillOnce(MoveArg<1>(&external_script_controller_callback));

  // Keyboard suppression is turned on and off again.
  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard(true));
  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard(false)).Times(2);

  // Expect bottomsheet NOT to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(0);

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl), false));

  // No onboarding.
  EXPECT_CALL(
      *runtime_manager(),
      SetUIState(
          autofill_assistant::UIState::kShownWithoutBrowsingFeatureSuppression))
      .Times(0);
  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

  // Cannot start another run.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl), false));

  // Failed run.
  EXPECT_CALL(*runtime_manager(),
              SetUIState(autofill_assistant::UIState::kNotShown));
  autofill_assistant::HeadlessScriptController::ScriptResult script_result = {
      /* success= */ false};
  std::move(external_script_controller_callback).Run(script_result);

  // `FastCheckoutClient` state was reset after run finished.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  histogram_tester_.ExpectUniqueSample(kUmaKeyFastCheckoutRunOutcome,
                                       FastCheckoutRunOutcome::kFail, 1u);
}

TEST_F(FastCheckoutClientImplTest, Stop_WhenIsRunning_CancelsTheRun) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl), false));

  fast_checkout_client()->Stop();

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest, OnDismiss_WhenIsRunning_CancelsTheRun) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl), false));

  EXPECT_CALL(*delegate(), OnFastCheckoutUIHidden);

  fast_checkout_client()->OnDismiss();

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest,
       OnOptionsSelected_MovesSelectionsToExternalActionDelegate) {
  EXPECT_CALL(*external_action_delegate(), SetOptionsSelected);

  // Starting the run successfully starts keyboard suppression.
  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard(true));
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl), false));

  // Profile selection turns off keyboard suppression again.
  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard(false));
  EXPECT_CALL(*delegate(), OnFastCheckoutUIHidden);

  // User selected profile and card in bottomsheet.
  fast_checkout_client()->OnOptionsSelected(std::make_unique<AutofillProfile>(),
                                            std::make_unique<CreditCard>());
}

TEST_F(FastCheckoutClientImplTest, RunsSuccessfullyIfDelegateIsDestroyed) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl), false));

  fast_checkout_delegate_.reset();
  fast_checkout_client()->OnDismiss();

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

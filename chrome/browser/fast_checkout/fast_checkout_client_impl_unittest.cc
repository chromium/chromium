// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"

#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/fast_checkout/fast_checkout_external_action_delegate.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/fast_checkout_delegate.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill_assistant/browser/public/mock_headless_script_controller.h"
#include "ui/gfx/native_widget_types.h"

using ::autofill::AutofillProfile;
using ::autofill::CreditCard;
using ::testing::_;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

namespace {
constexpr char kUrl[] = "https://www.example.com";
const AutofillProfile profile1 = autofill::test::GetFullProfile();
const AutofillProfile profile2 = autofill::test::GetFullProfile2();
const CreditCard credit_card1 = autofill::test::GetCreditCard();
const CreditCard credit_card2 = autofill::test::GetCreditCard2();
}

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

class MockFastCheckoutDelegate : public autofill::FastCheckoutDelegate {
 public:
  MockFastCheckoutDelegate() = default;
  ~MockFastCheckoutDelegate() override = default;

  MOCK_METHOD(bool,
              TryToShowFastCheckout,
              (const autofill::FormData&, const autofill::FormFieldData&),
              (override));
  MOCK_METHOD(bool, IsShowingFastCheckoutUI, (), (const, override));
  MOCK_METHOD(void, HideFastCheckoutUI, (), (override));
  MOCK_METHOD(void, OnFastCheckoutUIHidden, (), (override));
  MOCK_METHOD(void, Reset, (), (override));

  base::WeakPtr<MockFastCheckoutDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
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

  autofill::PersonalDataManager* GetPersonalDataManager() override {
    return personal_data_manager_;
  }

  void InjectFastCheckoutController(
      std::unique_ptr<FastCheckoutController> fast_checkout_controller) {
    fast_checkout_controller_ = std::move(fast_checkout_controller);
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

  void InjectPersonalDataManager(
      autofill::PersonalDataManager* personal_data_manager) {
    personal_data_manager_ = personal_data_manager;
  }

 private:
  std::unique_ptr<autofill_assistant::HeadlessScriptController>
      external_script_controller_;
  std::unique_ptr<FastCheckoutController> fast_checkout_controller_;
  std::unique_ptr<FastCheckoutExternalActionDelegate> external_action_delegate_;
  autofill::PersonalDataManager* personal_data_manager_;
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

    // Prepare the PersonalDataManager.
    SetUpPersonalDataManager();

    // Prepare the FastCheckoutDelegate.
    fast_checkout_delegate_ = std::make_unique<MockFastCheckoutDelegate>();
  }

  void SetUpPersonalDataManager() {
    test_personal_data_manager_ =
        std::make_unique<autofill::TestPersonalDataManager>();
    // Set up initial data
    test_personal_data_manager_->SetAutofillProfileEnabled(true);
    test_personal_data_manager_->SetAutofillCreditCardEnabled(true);
    test_personal_data_manager_->SetAutofillWalletImportEnabled(true);
    test_personal_data_manager_->AddProfile(profile1);
    test_personal_data_manager_->AddProfile(profile2);
    test_personal_data_manager_->AddProfile(
        autofill::test::GetIncompleteProfile1());
    // Add incomplete autofill profile, should not be shown on the sheet.
    test_personal_data_manager_->AddCreditCard(credit_card1);
    test_personal_data_manager_->AddCreditCard(credit_card2);
    // Add incomplete credit card, should not be shown on the sheet.
    test_personal_data_manager_->AddCreditCard(
        autofill::test::GetIncompleteCreditCard());
    test_client_->InjectPersonalDataManager(test_personal_data_manager_.get());
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

  base::WeakPtr<MockFastCheckoutDelegate> delegate() {
    return fast_checkout_delegate_->GetWeakPtr();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

  raw_ptr<autofill_assistant::MockHeadlessScriptController>
      external_script_controller_;
  raw_ptr<MockFastCheckoutController> fast_checkout_controller_;
  raw_ptr<MockFastCheckoutExternalActionDelegate> external_action_delegate_;
  std::unique_ptr<MockFastCheckoutDelegate> fast_checkout_delegate_;
  std::unique_ptr<autofill::TestPersonalDataManager>
      test_personal_data_manager_;
  raw_ptr<TestFastCheckoutClientImpl> test_client_;
};

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

  // Starting is not successful which is also represented by the internal state.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest, Start_FeatureEnabled_RunsSuccessfully) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Prepare to extract the callbacks to the external script controller.
  base::OnceCallback<void(
      autofill_assistant::HeadlessScriptController::ScriptResult)>
      external_script_controller_callback;
  base::OnceCallback<void()> onboarding_successful_callback;
  EXPECT_CALL(*external_script_controller(), StartScript(_, _, _, _))
      .Times(1)
      .WillOnce(
          [&](const base::flat_map<std::string, std::string>& script_parameters,
              base::OnceCallback<void(
                  autofill_assistant::HeadlessScriptController::ScriptResult)>
                  script_ended_callback,
              bool use_autofill_assistant_onboarding,
              base::OnceCallback<void()>
                  onboarding_successful_callback_parameter) {
            external_script_controller_callback =
                std::move(script_ended_callback);
            onboarding_successful_callback =
                std::move(onboarding_successful_callback_parameter);
          });

  // Expect bottomsheet to show up.
  EXPECT_CALL(
      *fast_checkout_controller(),
      Show(UnorderedElementsAre(Pointee(profile1), Pointee(profile2)),
           UnorderedElementsAre(Pointee(credit_card1), Pointee(credit_card2))));

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

  // Cannot start another run.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  // Successful run.
  std::move(onboarding_successful_callback).Run();
  autofill_assistant::HeadlessScriptController::ScriptResult script_result = {
      /* success= */ true};
  std::move(external_script_controller_callback).Run(script_result);

  // `FastCheckoutClient` state was reset after run finished.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest, Start_FailsIfNoProfilesOnFile) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Remove all profiles.
  test_personal_data_manager_->ClearProfiles();

  EXPECT_CALL(*external_script_controller(), StartScript(_, _, _, _)).Times(0);

  // Starting the run unsuccessfully.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  // `FastCheckoutClient` is not running.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest, Start_FailsIfNoCompleteProfile) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Remove all Profiles.
  test_personal_data_manager_->ClearProfiles();
  test_personal_data_manager_->AddProfile(
      autofill::test::GetIncompleteProfile1());
  test_personal_data_manager_->AddProfile(
      autofill::test::GetIncompleteProfile2());

  EXPECT_CALL(*external_script_controller(), StartScript(_, _, _, _)).Times(0);

  // Starting the run unsuccessfully.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  // `FastCheckoutClient` is not running.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest, Start_FailsIfNoCreditCardsOnFile) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Remove all credit cards.
  test_personal_data_manager_->ClearCreditCards();

  EXPECT_CALL(*external_script_controller(), StartScript(_, _, _, _)).Times(0);

  // Starting the run unsuccessfully.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  // `FastCheckoutClient` is not running.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest, Start_FailsIfNoCompleteorValidCreditCard) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Remove all credit Cards.
  test_personal_data_manager_->ClearCreditCards();
  test_personal_data_manager_->AddCreditCard(
      autofill::test::GetExpiredCreditCard());
  test_personal_data_manager_->AddCreditCard(
      autofill::test::GetIncompleteCreditCard());

  EXPECT_CALL(*external_script_controller(), StartScript(_, _, _, _)).Times(0);

  // Starting the run unsuccessfully.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  // `FastCheckoutClient` is not running.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest,
       Start_OnboardingNotSuccessful_BottomsheetNotShowing) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Prepare to extract the callbacks to the external script controller.
  base::OnceCallback<void(
      autofill_assistant::HeadlessScriptController::ScriptResult)>
      external_script_controller_callback;
  base::OnceCallback<void()> onboarding_successful_callback;
  EXPECT_CALL(*external_script_controller(), StartScript(_, _, _, _))
      .Times(1)
      .WillOnce(MoveArg<1>(&external_script_controller_callback));

  // Expect bottomsheet NOT to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(0);

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

  // Cannot start another run.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  // Failed run.
  autofill_assistant::HeadlessScriptController::ScriptResult script_result = {
      /* success= */ false};
  std::move(external_script_controller_callback).Run(script_result);

  // `FastCheckoutClient` state was reset after run finished.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest, Stop_WhenIsRunning_CancelsTheRun) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  fast_checkout_client()->Stop();

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest, OnDismiss_WhenIsRunning_CancelsTheRun) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  EXPECT_CALL(*delegate(), OnFastCheckoutUIHidden);

  fast_checkout_client()->OnDismiss();

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest,
       OnOptionsSelected_MovesSelectionsToExternalActionDelegate) {
  EXPECT_CALL(*external_action_delegate(), SetOptionsSelected);

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  EXPECT_CALL(*delegate(), OnFastCheckoutUIHidden);

  // User selected profile and card in bottomsheet.
  fast_checkout_client()->OnOptionsSelected(std::make_unique<AutofillProfile>(),
                                            std::make_unique<CreditCard>());
}

TEST_F(FastCheckoutClientImplTest, RunsSuccessfullyIfDelegateIsDestroyed) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  fast_checkout_delegate_.reset();
  fast_checkout_client()->OnDismiss();

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

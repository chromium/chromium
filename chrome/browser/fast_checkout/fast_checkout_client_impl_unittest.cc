// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "chrome/browser/fast_checkout/fast_checkout_trigger_validator.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/content_autofill_router.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/form_field_data.h"
#include "ui/gfx/native_widget_types.h"

using ::autofill::AutofillDriver;
using ::autofill::AutofillProfile;
using ::autofill::CreditCard;
using ::testing::_;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
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

class TestFastCheckoutClientImpl : public FastCheckoutClientImpl {
 public:
  static TestFastCheckoutClientImpl* CreateForWebContents(
      content::WebContents* web_contents);

  explicit TestFastCheckoutClientImpl(content::WebContents* web_contents)
      : FastCheckoutClientImpl(web_contents) {}

  std::unique_ptr<FastCheckoutController> CreateFastCheckoutController()
      override {
    return std::move(fast_checkout_controller_);
  }

  void InjectFastCheckoutController(
      std::unique_ptr<FastCheckoutController> fast_checkout_controller) {
    fast_checkout_controller_ = std::move(fast_checkout_controller);
  }

 private:
  std::unique_ptr<FastCheckoutController> fast_checkout_controller_;
};

class MockFastCheckoutTriggerValidator : public FastCheckoutTriggerValidator {
 public:
  MockFastCheckoutTriggerValidator() = default;
  ~MockFastCheckoutTriggerValidator() override = default;

  MOCK_METHOD(bool,
              ShouldRun,
              (const autofill::FormData&,
               const autofill::FormFieldData&,
               const FastCheckoutUIState,
               const bool,
               const autofill::ContentAutofillDriver*),
              (const));
  MOCK_METHOD(bool, HasValidPersonalData, (), (const));
};

class MockAutofillClient : public autofill::TestAutofillClient {
 public:
  MOCK_METHOD(void, HideAutofillPopup, (autofill::PopupHidingReason), ());
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

 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    autofill::PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(),
        base::BindRepeating(&BuildTestPersonalDataManager));

    test_client_ =
        TestFastCheckoutClientImpl::CreateForWebContents(web_contents());

    // Prepare the FastCheckoutController.
    auto fast_checkout_controller =
        std::make_unique<MockFastCheckoutController>();
    fast_checkout_controller_ = fast_checkout_controller.get();
    test_client_->InjectFastCheckoutController(
        std::move(fast_checkout_controller));

    // Prepare the AutofillDriver.
    autofill_driver_ = std::make_unique<MockAutofillDriver>();

    // Set AutofillManager on AutofillDriver.
    autofill_client_ = std::make_unique<MockAutofillClient>();
    auto test_browser_autofill_manager =
        std::make_unique<autofill::TestBrowserAutofillManager>(
            autofill_driver_.get(), autofill_client_.get());
    autofill_driver_->set_autofill_manager(
        std::move(test_browser_autofill_manager));

    auto trigger_validator =
        std::make_unique<MockFastCheckoutTriggerValidator>();
    validator_ = trigger_validator.get();
    test_client_->set_trigger_validator_for_test(std::move(trigger_validator));
    ON_CALL(*validator(), ShouldRun).WillByDefault(Return(true));

    test_client_->set_autofill_client_for_test(autofill_client_.get());
  }

  autofill::TestPersonalDataManager* personal_data_manager() {
    return static_cast<autofill::TestPersonalDataManager*>(
        autofill::PersonalDataManagerFactory::GetForProfile(profile()));
  }

  TestFastCheckoutClientImpl* fast_checkout_client() { return test_client_; }

  MockFastCheckoutController* fast_checkout_controller() {
    return fast_checkout_controller_;
  }

  MockAutofillDriver* autofill_driver() { return autofill_driver_.get(); }

  MockFastCheckoutTriggerValidator* validator() { return validator_.get(); }

  MockAutofillClient* autofill_client() { return autofill_client_.get(); }

  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<MockAutofillClient> autofill_client_;
  raw_ptr<MockFastCheckoutController> fast_checkout_controller_;
  std::unique_ptr<MockAutofillDriver> autofill_driver_;
  raw_ptr<TestFastCheckoutClientImpl> test_client_;
  raw_ptr<MockFastCheckoutTriggerValidator> validator_;
};

TEST_F(
    FastCheckoutClientImplTest,
    GetOrCreateForWebContents_ClientWasAlreadyCreated_ReturnsExistingInstance) {
  raw_ptr<FastCheckoutClient> client =
      FastCheckoutClient::GetOrCreateForWebContents(web_contents());

  // There is only one client per `WebContents`.
  EXPECT_EQ(client, fast_checkout_client());
}

TEST_F(FastCheckoutClientImplTest, Start_InvalidAutofillDriver_NoRun) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Do not expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(0);
  // Do not expect keyboard to be suppressed.
  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard).Times(0);
  // Do not expect Autofill popups to be hidden.
  EXPECT_CALL(*autofill_client(), HideAutofillPopup).Times(0);

  EXPECT_FALSE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(), nullptr));
}

TEST_F(FastCheckoutClientImplTest, Start_ShouldRunReturnsFalse_NoRun) {
  ON_CALL(*validator(), ShouldRun).WillByDefault(Return(false));

  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Do not expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(0);
  // Do not expect keyboard to be suppressed.
  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard).Times(0);
  // Do not expect Autofill popups to be hidden.
  EXPECT_CALL(*autofill_client(), HideAutofillPopup).Times(0);

  EXPECT_FALSE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_driver()));
}

TEST_F(FastCheckoutClientImplTest, Start_ShouldRunReturnsTrue_Run) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Expect the bottomsheet to show up.
  EXPECT_CALL(
      *fast_checkout_controller(),
      Show(UnorderedElementsAre(Pointee(kProfile1), Pointee(kProfile2),
                                Pointee(kIncompleteProfile)),
           UnorderedElementsAre(Pointee(kCreditCard1), Pointee(kCreditCard2))));
  // Expect keyboard suppression.
  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard(true));
  // Expect call to `HideAutofillPopup`.
  EXPECT_CALL(
      *autofill_client(),
      HideAutofillPopup(
          autofill::PopupHidingReason::kOverlappingWithFastCheckoutSurface));

  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_driver()));

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  EXPECT_TRUE(fast_checkout_client()->IsShowing());
}

TEST_F(FastCheckoutClientImplTest,
       OnPersonalDataChanged_StopIfInvalidPersonalData) {
  ON_CALL(*validator(), HasValidPersonalData).WillByDefault(Return(false));

  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(1);

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_driver()));

  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

  // User removes all the profiles.
  personal_data_manager()->ClearProfiles();
  // User adds an incomplete profile only.
  personal_data_manager()->AddProfile(autofill::test::GetIncompleteProfile1());

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest,
       OnPersonalDataChanged_UpdatesTheUIWithNewData) {
  ON_CALL(*validator(), HasValidPersonalData).WillByDefault(Return(true));

  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  EXPECT_CALL(
      *fast_checkout_controller(),
      Show(UnorderedElementsAre(Pointee(kProfile1), Pointee(kProfile2),
                                Pointee(kIncompleteProfile)),
           UnorderedElementsAre(Pointee(kCreditCard1), Pointee(kCreditCard2))));

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_driver()));

  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

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

TEST_F(FastCheckoutClientImplTest, Stop_WhenIsRunning_CancelsTheRun) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  EXPECT_FALSE(fast_checkout_client()->IsShowing());

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_driver()));

  // Fast Checkout is running and showing the bottomsheet.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  EXPECT_TRUE(fast_checkout_client()->IsShowing());

  // Stopping the run.
  fast_checkout_client()->Stop(/*allow_further_runs=*/false);

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  EXPECT_FALSE(fast_checkout_client()->IsShowing());
}

TEST_F(FastCheckoutClientImplTest, OnDismiss_WhenIsRunning_CancelsTheRun) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_driver()));

  fast_checkout_client()->OnDismiss();

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest,
       DestroyingAutofillDriver_ResetsAutofillDriverPointer) {
  // Set up Autofill instances so that `FastCheckoutClient::Stop(..)` will be
  // called when `autofill_driver` is destroyed below. `Stop(..)` is supposed to
  // reset `FastCheckoutClientImpl::autofill_driver_`.
  // The expected stack trace is:
  //   `FastCheckoutClientImpl::Stop(/*allow_further_runs=*/true)`
  //   `ChromeAutofillClient::HideFastCheckout(/*allow_further_runs=*/true)`
  //   `~BrowserAutofillManager()`
  //   `autofill_driver.reset()`
  autofill::ChromeAutofillClient::CreateForWebContents(web_contents());
  auto autofill_router = std::make_unique<autofill::ContentAutofillRouter>();
  auto autofill_driver = std::make_unique<autofill::ContentAutofillDriver>(
      web_contents()->GetPrimaryMainFrame(), autofill_router.get());
  auto browser_autofill_manager =
      std::make_unique<autofill::BrowserAutofillManager>(
          autofill_driver.get(),
          autofill::ChromeAutofillClient::FromWebContents(web_contents()),
          "en-US", autofill::AutofillManager::EnableDownloadManager(false));
  autofill_driver->set_autofill_manager(std::move(browser_autofill_manager));

  // `FastCheckoutClientImpl::autofill_driver_` is `nullptr` initially.
  EXPECT_FALSE(fast_checkout_client()->get_autofill_driver_for_test());

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_driver.get()));

  // `FastCheckoutClientImpl::autofill_driver_` is not `nullptr` anymore.
  EXPECT_TRUE(fast_checkout_client()->get_autofill_driver_for_test());

  // Destroy `ContentAutofillDriver` instance, invoking
  // `~BrowserAutofillManager()` and thus `FastCheckoutClient::Stop(..)`.
  autofill_driver.reset();

  // `FastCheckoutClientImpl::autofill_driver_` is `nullptr` again.
  EXPECT_FALSE(fast_checkout_client()->get_autofill_driver_for_test());

  // Expect this `Stop(..)` call to not crash the test.
  fast_checkout_client()->Stop(/*allow_further_runs=*/true);
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_factory.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "chrome/browser/fast_checkout/fast_checkout_trigger_validator.h"
#include "chrome/browser/fast_checkout/mock_fast_checkout_capabilities_fetcher.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/content_autofill_router.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/form_field_data.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/gfx/native_widget_types.h"

using ::autofill::AutofillDriver;
using ::autofill::AutofillProfile;
using ::autofill::CreditCard;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::UnorderedElementsAre;
using ::ukm::builders::Autofill_FastCheckoutFormStatus;
using ::ukm::builders::Autofill_FastCheckoutRunOutcome;

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
constexpr char kOtherUrl[] = "https://www.example2.com";
const std::u16string kAutofillProfileLabel = u"Home";
const std::u16string kCreditCardNickname = u"Card's nickname";
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

class MockBrowserAutofillManager : public autofill::TestBrowserAutofillManager {
 public:
  MockBrowserAutofillManager(autofill::TestAutofillDriver* driver,
                             autofill::TestAutofillClient* client)
      : autofill::TestBrowserAutofillManager(driver, client) {}
  ~MockBrowserAutofillManager() override = default;

  MOCK_METHOD(void, SetShouldSuppressKeyboard, (bool), (override));
  MOCK_METHOD(void,
              TriggerReparseInAllFrames,
              (base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void,
              FillProfileFormImpl,
              (const autofill::FormData&,
               const autofill::FormFieldData&,
               const autofill::AutofillProfile&),
              (override));
  MOCK_METHOD(void,
              FillCreditCardFormImpl,
              (const autofill::FormData&,
               const autofill::FormFieldData&,
               const autofill::CreditCard&,
               const std::u16string&),
              (override));
  MOCK_METHOD(void,
              SetFastCheckoutRunId,
              (autofill::FieldTypeGroup, int64_t),
              (override));
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
               const base::WeakPtr<autofill::AutofillManager>),
              (const));
  MOCK_METHOD(bool, HasValidPersonalData, (), (const));
};

class MockAutofillClient : public autofill::TestAutofillClient {
 public:
  MOCK_METHOD(void, HideAutofillPopup, (autofill::PopupHidingReason), ());
};

class MockFastCheckoutAccessibilityService
    : public FastCheckoutAccessibilityService {
 public:
  MockFastCheckoutAccessibilityService() = default;
  ~MockFastCheckoutAccessibilityService() override = default;

  MOCK_METHOD(void, Announce, (const std::u16string&), (override));
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
  FastCheckoutClientImplTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitWithFeatures({features::kFastCheckout}, {});
  }

 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    autofill::PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(),
        base::BindRepeating(&BuildTestPersonalDataManager));
    FastCheckoutCapabilitiesFetcherFactory::GetInstance()
        ->SetTestingSubclassFactoryAndUse(
            profile(), base::BindRepeating([](content::BrowserContext*) {
              return std::make_unique<
                  NiceMock<MockFastCheckoutCapabilitiesFetcher>>();
            }));

    test_client_ =
        TestFastCheckoutClientImpl::CreateForWebContents(web_contents());

    // Prepare the FastCheckoutController.
    auto fast_checkout_controller =
        std::make_unique<NiceMock<MockFastCheckoutController>>();
    fast_checkout_controller_ = fast_checkout_controller.get();
    test_client_->InjectFastCheckoutController(
        std::move(fast_checkout_controller));

    // Prepare the AutofillDriver.
    autofill_driver_ = std::make_unique<autofill::TestAutofillDriver>();

    // Set AutofillManager on AutofillDriver.
    autofill_client_ = std::make_unique<NiceMock<MockAutofillClient>>();
    autofill_client_->set_test_payments_client(
        std::make_unique<autofill::payments::TestPaymentsClient>(
            autofill_client_->GetURLLoaderFactory(),
            autofill_client_->GetIdentityManager(),
            autofill_client_->GetPersonalDataManager()));
    auto test_browser_autofill_manager =
        std::make_unique<NiceMock<MockBrowserAutofillManager>>(
            autofill_driver_.get(), autofill_client_.get());
    autofill_manager_ = test_browser_autofill_manager.get();
    autofill_driver_->set_autofill_manager(
        std::move(test_browser_autofill_manager));

    auto trigger_validator =
        std::make_unique<NiceMock<MockFastCheckoutTriggerValidator>>();
    validator_ = trigger_validator.get();
    test_client_->trigger_validator_ = std::move(trigger_validator);
    ON_CALL(*validator(), ShouldRun).WillByDefault(Return(true));

    test_client_->autofill_client_ = autofill_client_.get();

    auto accessibility_service =
        std::make_unique<NiceMock<MockFastCheckoutAccessibilityService>>();
    accessibility_service_ = accessibility_service.get();
    fast_checkout_client()->accessibility_service_ =
        std::move(accessibility_service);
  }

  autofill::TestPersonalDataManager* personal_data_manager() {
    return static_cast<autofill::TestPersonalDataManager*>(
        autofill::PersonalDataManagerFactory::GetForProfile(profile()));
  }

  FastCheckoutClientImpl* fast_checkout_client() { return test_client_; }

  MockFastCheckoutController* fast_checkout_controller() {
    return fast_checkout_controller_;
  }

  MockFastCheckoutTriggerValidator* validator() { return validator_.get(); }

  MockAutofillClient* autofill_client() { return autofill_client_.get(); }

  MockBrowserAutofillManager* autofill_manager() { return autofill_manager_; }

  MockFastCheckoutAccessibilityService* accessibility_service() {
    return accessibility_service_;
  }

  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;

  // Sets up test data, calls `TryToStart(..)` and `OnOptionsSelected(..)`.
  std::tuple<autofill::AutofillProfile*, autofill::CreditCard*>
  StartRunAndSelectOptions(
      const base::flat_set<autofill::FormSignature>& forms_to_fill,
      bool local_card = false) {
    auto autofill_profile_unique_ptr =
        std::make_unique<autofill::AutofillProfile>(
            autofill::test::GetFullProfile());
    autofill_profile_unique_ptr->set_profile_label(
        base::UTF16ToUTF8(kAutofillProfileLabel));
    personal_data_manager()->AddProfile(*autofill_profile_unique_ptr);
    auto credit_card_unique_ptr = std::make_unique<autofill::CreditCard>(
        local_card ? autofill::test::GetCreditCard()
                   : autofill::test::GetMaskedServerCard());
    credit_card_unique_ptr->SetNickname(kCreditCardNickname);
    if (local_card) {
      personal_data_manager()->AddCreditCard(*credit_card_unique_ptr);
    } else {
      personal_data_manager()->AddServerCreditCard(*credit_card_unique_ptr);
    }

    MockFastCheckoutCapabilitiesFetcher* fetcher =
        static_cast<MockFastCheckoutCapabilitiesFetcher*>(
            FastCheckoutCapabilitiesFetcherFactory::GetForBrowserContext(
                profile()));

    EXPECT_CALL(*fetcher, GetFormsToFill(url::Origin::Create(GURL(kUrl))))
        .WillOnce(Return(forms_to_fill));
    EXPECT_TRUE(fast_checkout_client()->TryToStart(
        GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
        autofill_manager()->GetWeakPtr()));
    fast_checkout_client()->OnOptionsSelected(
        std::move(autofill_profile_unique_ptr),
        std::move(credit_card_unique_ptr));
    return {
        personal_data_manager()->GetProfileByGUID(
            fast_checkout_client()->selected_autofill_profile_guid_.value()),
        (local_card
             ? personal_data_manager()->GetCreditCardByGUID(
                   fast_checkout_client()->selected_credit_card_id_.value())
             : personal_data_manager()->GetCreditCardByServerId(
                   fast_checkout_client()->selected_credit_card_id_.value()))};
  }

  std::unique_ptr<autofill::FormStructure> SetUpCreditCardForm() {
    autofill::FormData credit_card_form_data;
    autofill::test::CreateTestCreditCardFormData(&credit_card_form_data, true,
                                                 false, true);
    auto credit_card_form_structure =
        std::make_unique<autofill::FormStructure>(credit_card_form_data);
    credit_card_form_structure->field(0)->set_heuristic_type(
        autofill::PatternSource::kLegacy,
        autofill::ServerFieldType::CREDIT_CARD_NUMBER);
    return credit_card_form_structure;
  }

  std::unique_ptr<autofill::FormStructure> SetUpAddressForm() {
    autofill::FormData address_form_data;
    autofill::test::CreateTestAddressFormData(&address_form_data);
    auto address_form_structure =
        std::make_unique<autofill::FormStructure>(address_form_data);
    address_form_structure->field(0)->set_heuristic_type(
        autofill::PatternSource::kLegacy,
        autofill::ServerFieldType::NAME_FIRST);
    return address_form_structure;
  }

  autofill::FormStructure* AddFormToAutofillManagerCache(
      std::unique_ptr<autofill::FormStructure> form) {
    autofill::FormStructure* form_ptr = form.get();
    autofill_manager()->AddSeenFormStructure(std::move(form));
    return form_ptr;
  }

  void ExpectRunOutcomeUkm(FastCheckoutRunOutcome run_outcome) {
    auto ukm_entries = ukm_recorder_.GetEntries(
        Autofill_FastCheckoutRunOutcome::kEntryName,
        {Autofill_FastCheckoutRunOutcome::kRunOutcomeName,
         Autofill_FastCheckoutRunOutcome::kRunIdName});
    EXPECT_EQ(ukm_entries.size(), 1UL);
    EXPECT_EQ(ukm_entries[0].metrics.at("RunOutcome"),
              static_cast<long>(run_outcome));
    EXPECT_NE(ukm_entries[0].metrics.at("RunId"), 0L);
  }

 private:
  // Required for using some `autofill::test` functions inside the test class.
  autofill::test::AutofillEnvironment autofill_environment_;
  std::unique_ptr<MockAutofillClient> autofill_client_;
  raw_ptr<MockFastCheckoutController> fast_checkout_controller_;
  std::unique_ptr<autofill::TestAutofillDriver> autofill_driver_;
  raw_ptr<TestFastCheckoutClientImpl> test_client_;
  raw_ptr<MockFastCheckoutTriggerValidator> validator_;
  raw_ptr<MockBrowserAutofillManager> autofill_manager_;
  raw_ptr<MockFastCheckoutAccessibilityService> accessibility_service_;
};

MATCHER_P(FormDataEqualTo,
          form_data,
          "Compares two autofill::FormData instances with their DeepEqual "
          "function.") {
  return autofill::FormData::DeepEqual(arg, form_data);
}

MATCHER_P(FormFieldDataEqualTo,
          form_data,
          "Compares two autofill::FormFieldData instances with their DeepEqual "
          "function.") {
  return autofill::FormFieldData::DeepEqual(arg, form_data);
}

TEST_F(
    FastCheckoutClientImplTest,
    GetOrCreateForWebContents_ClientWasAlreadyCreated_ReturnsExistingInstance) {
  raw_ptr<FastCheckoutClient> client =
      FastCheckoutClient::GetOrCreateForWebContents(web_contents());

  // There is only one client per `WebContents`.
  EXPECT_EQ(client, fast_checkout_client());
}

TEST_F(FastCheckoutClientImplTest, Start_InvalidAutofillManager_NoRun) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Do not expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(0);
  // Do not expect keyboard to be suppressed.
  EXPECT_CALL(*autofill_manager(), SetShouldSuppressKeyboard).Times(0);
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
  EXPECT_CALL(*autofill_manager(), SetShouldSuppressKeyboard).Times(0);
  // Do not expect Autofill popups to be hidden.
  EXPECT_CALL(*autofill_client(), HideAutofillPopup).Times(0);

  EXPECT_FALSE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));
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
  // Expect keyboard suppression from `TryToStart`.
  EXPECT_CALL(*autofill_manager(), SetShouldSuppressKeyboard(true));
  // Expect call to `HideAutofillPopup`.
  EXPECT_CALL(
      *autofill_client(),
      HideAutofillPopup(
          autofill::PopupHidingReason::kOverlappingWithFastCheckoutSurface));

  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));

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
      autofill_manager()->GetWeakPtr()));

  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

  // User removes all the profiles.
  personal_data_manager()->ClearProfiles();
  // User adds an incomplete profile only.
  personal_data_manager()->AddProfile(autofill::test::GetIncompleteProfile1());

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  ExpectRunOutcomeUkm(FastCheckoutRunOutcome::kInvalidPersonalData);
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
      autofill_manager()->GetWeakPtr()));

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
      autofill_manager()->GetWeakPtr()));

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
      autofill_manager()->GetWeakPtr()));

  fast_checkout_client()->OnDismiss();

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  ExpectRunOutcomeUkm(FastCheckoutRunOutcome::kBottomsheetDismissed);
}

TEST_F(FastCheckoutClientImplTest,
       DestroyingAutofillDriver_ResetsAutofillManagerPointer) {
  // Set up Autofill instances so that `FastCheckoutClient::Stop(..)` will be
  // called when `autofill_driver` is destroyed below. `Stop(..)` is supposed to
  // reset `FastCheckoutClientImpl::autofill_manager_`.
  autofill::ChromeAutofillClient::CreateForWebContents(web_contents());
  auto autofill_router = std::make_unique<autofill::ContentAutofillRouter>();
  auto autofill_driver = std::make_unique<autofill::ContentAutofillDriver>(
      web_contents()->GetPrimaryMainFrame(), autofill_router.get());
  auto browser_autofill_manager =
      std::make_unique<autofill::BrowserAutofillManager>(
          autofill_driver.get(),
          autofill::ChromeAutofillClient::FromWebContents(web_contents()),
          "en-US");
  autofill::BrowserAutofillManager* autofill_manager =
      browser_autofill_manager.get();
  autofill_driver->set_autofill_manager(std::move(browser_autofill_manager));

  // `FastCheckoutClientImpl::autofill_manager_` is `nullptr` initially.
  EXPECT_FALSE(fast_checkout_client()->autofill_manager_);

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager->GetWeakPtr()));

  // `FastCheckoutClientImpl::autofill_manager_` is not `nullptr` anymore.
  EXPECT_TRUE(fast_checkout_client()->autofill_manager_);

  // Destroy `ContentAutofillDriver` instance, invoking
  // `~BrowserAutofillManager()` and thus `FastCheckoutClient::Stop(..)`.
  autofill_driver.reset();

  // `FastCheckoutClientImpl::autofill_manager_` is `nullptr` again.
  EXPECT_FALSE(fast_checkout_client()->autofill_manager_);

  // Expect this `Stop(..)` call to not crash the test.
  fast_checkout_client()->Stop(/*allow_further_runs=*/true);
  ExpectRunOutcomeUkm(FastCheckoutRunOutcome::kAutofillManagerDestroyed);
}

TEST_F(FastCheckoutClientImplTest,
       OnOptionsSelected_ServerCard_SavesFormsAndAutofillDataSelections) {
  std::unique_ptr<autofill::FormStructure> address_form = SetUpAddressForm();
  std::unique_ptr<autofill::FormStructure> credit_card_form =
      SetUpCreditCardForm();

  EXPECT_CALL(*autofill_manager(), TriggerReparseInAllFrames);

  auto [autofill_profile, credit_card] = StartRunAndSelectOptions(
      {address_form->form_signature(), credit_card_form->form_signature()});

  EXPECT_TRUE(fast_checkout_client()->selected_autofill_profile_guid_);
  EXPECT_EQ(autofill_profile->guid(),
            fast_checkout_client()->selected_autofill_profile_guid_.value());
  EXPECT_TRUE(fast_checkout_client()->selected_credit_card_id_);
  EXPECT_EQ(credit_card->server_id(),
            fast_checkout_client()->selected_credit_card_id_.value());
  EXPECT_THAT(fast_checkout_client()->form_signatures_to_fill_,
              UnorderedElementsAre(address_form->form_signature(),
                                   credit_card_form->form_signature()));
}

TEST_F(FastCheckoutClientImplTest,
       OnOptionsSelected_LocalCard_SavesFormsAndAutofillDataSelections) {
  autofill::FormStructure* credit_card_form =
      AddFormToAutofillManagerCache(SetUpCreditCardForm());

  EXPECT_CALL(*autofill_manager(), TriggerReparseInAllFrames);

  auto [autofill_profile, credit_card] = StartRunAndSelectOptions(
      {credit_card_form->form_signature()}, /*local_card=*/true);

  EXPECT_TRUE(fast_checkout_client()->selected_autofill_profile_guid_);
  EXPECT_EQ(autofill_profile->guid(),
            fast_checkout_client()->selected_autofill_profile_guid_.value());
  EXPECT_TRUE(fast_checkout_client()->selected_credit_card_id_);
  EXPECT_EQ(credit_card->guid(),
            fast_checkout_client()->selected_credit_card_id_.value());
  EXPECT_THAT(fast_checkout_client()->form_signatures_to_fill_,
              ElementsAre(credit_card_form->form_signature()));
  EXPECT_THAT(fast_checkout_client()->form_filling_states_,
              UnorderedElementsAre(
                  Pair(Pair(credit_card_form->form_signature(),
                            autofill::FormType::kCreditCardForm),
                       FastCheckoutClientImpl::FillingState::kFilling)));
}

TEST_F(FastCheckoutClientImplTest, OnAfterLoadedServerPredictions_FillsForms) {
  std::unique_ptr<autofill::FormStructure> address_form = SetUpAddressForm();
  std::unique_ptr<autofill::FormStructure> credit_card_form =
      SetUpCreditCardForm();
  autofill::FormSignature address_form_signature =
      address_form->form_signature();
  autofill::FormSignature credit_card_form_signature =
      credit_card_form->form_signature();
  const autofill::FormData& address_form_data = address_form->ToFormData();
  const autofill::FormFieldData& address_form_field_data =
      *address_form->field(0);

  auto [autofill_profile, credit_card] = StartRunAndSelectOptions(
      {address_form_signature, credit_card_form_signature});

  AddFormToAutofillManagerCache(std::move(address_form));
  AddFormToAutofillManagerCache(std::move(credit_card_form));

  // Reset filling states.
  for (auto& [form_id, filling_state] :
       fast_checkout_client()->form_filling_states_) {
    filling_state = FastCheckoutClientImpl::FillingState::kNotFilled;
  }

  EXPECT_CALL(*autofill_manager(),
              FillProfileFormImpl(FormDataEqualTo(address_form_data),
                                  FormFieldDataEqualTo(address_form_field_data),
                                  Eq(*autofill_profile)));
  EXPECT_CALL(*autofill_manager(),
              SetFastCheckoutRunId(autofill::FieldTypeGroup::kAddressHome,
                                   fast_checkout_client()->run_id_));
  fast_checkout_client()->OnAfterLoadedServerPredictions(*autofill_manager());
  EXPECT_THAT(
      fast_checkout_client()->form_filling_states_,
      UnorderedElementsAre(
          Pair(Pair(address_form_signature, autofill::FormType::kAddressForm),
               FastCheckoutClientImpl::FillingState::kFilling),
          Pair(Pair(credit_card_form_signature,
                    autofill::FormType::kCreditCardForm),
               FastCheckoutClientImpl::FillingState::kNotFilled)));
  EXPECT_TRUE(fast_checkout_client()->credit_card_form_global_id_.has_value());
}

TEST_F(FastCheckoutClientImplTest,
       OnAfterDidFillAutofillFormData_SetsFillingFormsToFilledAndStops) {
  autofill::FormStructure* address_form =
      AddFormToAutofillManagerCache(SetUpAddressForm());
  autofill::FormStructure* credit_card_form =
      AddFormToAutofillManagerCache(SetUpCreditCardForm());

  auto [autofill_profile, credit_card] = StartRunAndSelectOptions(
      {address_form->form_signature(), credit_card_form->form_signature()});

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  int64_t run_id = fast_checkout_client()->run_id_;

  autofill::payments::FullCardRequest* full_card_request =
      autofill_client()->GetCvcAuthenticator()->GetFullCardRequest();
  std::u16string cvc = u"123";
  const autofill::FormFieldData& field = *credit_card_form->field(0);

  EXPECT_CALL(*autofill_manager(),
              FillCreditCardFormImpl(
                  FormDataEqualTo(credit_card_form->ToFormData()),
                  FormFieldDataEqualTo(field), Eq(*credit_card), Eq(cvc)));
  EXPECT_CALL(*autofill_manager(),
              SetFastCheckoutRunId(autofill::FieldTypeGroup::kCreditCard,
                                   fast_checkout_client()->run_id_));
  fast_checkout_client()->OnFullCardRequestSucceeded(*full_card_request,
                                                     *credit_card, cvc);

  EXPECT_THAT(fast_checkout_client()->form_filling_states_,
              UnorderedElementsAre(
                  Pair(Pair(address_form->form_signature(),
                            autofill::FormType::kAddressForm),
                       FastCheckoutClientImpl::FillingState::kFilling),
                  Pair(Pair(credit_card_form->form_signature(),
                            autofill::FormType::kCreditCardForm),
                       FastCheckoutClientImpl::FillingState::kFilling)));

  fast_checkout_client()->OnAfterDidFillAutofillFormData(
      *autofill_manager(), credit_card_form->global_id());

  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  EXPECT_EQ(fast_checkout_client()->fast_checkout_ui_state_,
            FastCheckoutUIState::kWasShown);
  ExpectRunOutcomeUkm(FastCheckoutRunOutcome::kSuccess);
  auto ukm_entries = ukm_recorder_.GetEntries(
      Autofill_FastCheckoutFormStatus::kEntryName,
      {Autofill_FastCheckoutFormStatus::kRunIdName,
       Autofill_FastCheckoutFormStatus::kFilledName,
       Autofill_FastCheckoutFormStatus::kFormSignatureName,
       Autofill_FastCheckoutFormStatus::kFormTypesName});
  EXPECT_EQ(ukm_entries.size(), 2UL);
  base::flat_set<ukm::TestAutoSetUkmRecorder::HumanReadableUkmMetrics> metrics;
  metrics.emplace(ukm_entries[0].metrics);
  metrics.emplace(ukm_entries[1].metrics);
  EXPECT_THAT(
      metrics,
      UnorderedElementsAre(
          UnorderedElementsAre(
              Pair(Autofill_FastCheckoutFormStatus::kRunIdName, run_id),
              Pair(Autofill_FastCheckoutFormStatus::kFilledName, 1),
              Pair(Autofill_FastCheckoutFormStatus::kFormSignatureName,
                   autofill::HashFormSignature(address_form->form_signature())),
              Pair(Autofill_FastCheckoutFormStatus::kFormTypesName, 3)),
          UnorderedElementsAre(
              Pair(Autofill_FastCheckoutFormStatus::kRunIdName, run_id),
              Pair(Autofill_FastCheckoutFormStatus::kFilledName, 1),
              Pair(Autofill_FastCheckoutFormStatus::kFormSignatureName,
                   autofill::HashFormSignature(
                       credit_card_form->form_signature())),
              Pair(Autofill_FastCheckoutFormStatus::kFormTypesName, 5))));
}

TEST_F(FastCheckoutClientImplTest,
       OnAutofillManagerReset_IsShowing_ResetsState) {
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  EXPECT_TRUE(fast_checkout_client()->IsShowing());
  fast_checkout_client()->OnAutofillManagerReset(*autofill_manager());
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  ExpectRunOutcomeUkm(
      FastCheckoutRunOutcome::kNavigationWhileBottomsheetWasShown);
}

TEST_F(FastCheckoutClientImplTest,
       OnAutofillManagerReset_IsNotShowing_ResetsState) {
  std::unique_ptr<autofill::FormStructure> address_form = SetUpAddressForm();

  StartRunAndSelectOptions({address_form->form_signature()});
  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  EXPECT_FALSE(fast_checkout_client()->IsShowing());
  fast_checkout_client()->OnAutofillManagerReset(*autofill_manager());
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  ExpectRunOutcomeUkm(FastCheckoutRunOutcome::kPageRefreshed);
}

TEST_F(FastCheckoutClientImplTest, OnAutofillManagerDestroyed_ResetsState) {
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  fast_checkout_client()->OnAutofillManagerDestroyed(*autofill_manager());
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  ExpectRunOutcomeUkm(FastCheckoutRunOutcome::kAutofillManagerDestroyed);
}

TEST_F(FastCheckoutClientImplTest, TimeoutTimer_ThirtyMinutesPassed_StopsRun) {
  std::unique_ptr<autofill::FormStructure> address_form = SetUpAddressForm();
  std::unique_ptr<autofill::FormStructure> credit_card_form =
      SetUpCreditCardForm();

  StartRunAndSelectOptions(
      {address_form->form_signature(), credit_card_form->form_signature()});

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  task_environment()->FastForwardBy(base::Minutes(30));
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  ExpectRunOutcomeUkm(FastCheckoutRunOutcome::kTimeout);
}

TEST_F(FastCheckoutClientImplTest, OnNavigation_OtherUrl_StopsRun) {
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  fast_checkout_client()->OnNavigation(GURL(kOtherUrl), false);
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  ExpectRunOutcomeUkm(FastCheckoutRunOutcome::kOriginChange);
}

TEST_F(FastCheckoutClientImplTest,
       OnNavigation_SameUrlButNoCartOrCheckoutPage_StopsRun) {
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  fast_checkout_client()->OnNavigation(GURL(kUrl), false);
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  ExpectRunOutcomeUkm(FastCheckoutRunOutcome::kNonCheckoutPage);
}

TEST_F(FastCheckoutClientImplTest,
       OnNavigation_SameUrlAndCartOrCheckoutPage_DoesNotStopRun) {
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  fast_checkout_client()->OnNavigation(GURL(kUrl), true);
  EXPECT_TRUE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest,
       OnFullCardRequestSucceeded_InvokesCreditCardFormFill) {
  autofill::FormStructure* address_form =
      AddFormToAutofillManagerCache(SetUpAddressForm());
  autofill::FormStructure* credit_card_form =
      AddFormToAutofillManagerCache(SetUpCreditCardForm());

  auto [autofill_profile, credit_card] = StartRunAndSelectOptions(
      {address_form->form_signature(), credit_card_form->form_signature()});
  autofill::payments::FullCardRequest* full_card_request =
      autofill_client()->GetCvcAuthenticator()->GetFullCardRequest();
  const autofill::FormFieldData& field = *credit_card_form->field(0);
  std::u16string cvc = u"123";

  EXPECT_CALL(*autofill_manager(),
              FillCreditCardFormImpl(
                  FormDataEqualTo(credit_card_form->ToFormData()),
                  FormFieldDataEqualTo(field), Eq(*credit_card), Eq(cvc)));
  EXPECT_CALL(*autofill_manager(),
              SetFastCheckoutRunId(autofill::FieldTypeGroup::kCreditCard,
                                   fast_checkout_client()->run_id_));
  fast_checkout_client()->OnFullCardRequestSucceeded(*full_card_request,
                                                     *credit_card, cvc);
}

TEST_F(FastCheckoutClientImplTest, OnFullCardRequestFailed_StopsRun) {
  autofill::FormStructure* credit_card_form =
      AddFormToAutofillManagerCache(SetUpCreditCardForm());
  auto card_type = autofill::CreditCard::RecordType::FULL_SERVER_CARD;
  auto failure_type =
      autofill::payments::FullCardRequest::FailureType::GENERIC_FAILURE;
  StartRunAndSelectOptions({credit_card_form->form_signature()});

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  fast_checkout_client()->OnFullCardRequestFailed(card_type, failure_type);
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  ExpectRunOutcomeUkm(FastCheckoutRunOutcome::kCvcPopupError);
}

TEST_F(
    FastCheckoutClientImplTest,
    OnAfterDidFillAutofillFormData_AddressForm_MakesAddressFormA11yAnnouncement) {
  autofill::FormStructure* address_form =
      AddFormToAutofillManagerCache(SetUpAddressForm());
  StartRunAndSelectOptions({address_form->form_signature()});
  std::u16string announcement_text =
      kAutofillProfileLabel + u" address form filled.";

  EXPECT_CALL(*accessibility_service(), Announce(announcement_text));
  fast_checkout_client()->OnAfterDidFillAutofillFormData(
      *autofill_manager(), address_form->global_id());
}

TEST_F(
    FastCheckoutClientImplTest,
    OnAfterDidFillAutofillFormData_EmailForm_MakesEmailFormA11yAnnouncement) {
  autofill::FormStructure* address_form =
      AddFormToAutofillManagerCache(SetUpAddressForm());
  address_form->field(0)->set_heuristic_type(
      autofill::PatternSource::kLegacy,
      autofill::ServerFieldType::EMAIL_ADDRESS);
  StartRunAndSelectOptions({address_form->form_signature()});
  std::u16string announcement_text = u"Email filled.";

  EXPECT_CALL(*accessibility_service(), Announce(announcement_text));
  fast_checkout_client()->OnAfterDidFillAutofillFormData(
      *autofill_manager(), address_form->global_id());
}

TEST_F(
    FastCheckoutClientImplTest,
    OnAfterDidFillAutofillFormData_CreditCardForm_MakesCreditCardFormA11yAnnouncement) {
  autofill::FormStructure* credit_card_form =
      AddFormToAutofillManagerCache(SetUpCreditCardForm());
  auto [autofill_profile, credit_card] =
      StartRunAndSelectOptions({credit_card_form->form_signature()});
  autofill::payments::FullCardRequest* full_card_request =
      autofill_client()->GetCvcAuthenticator()->GetFullCardRequest();
  fast_checkout_client()->OnFullCardRequestSucceeded(*full_card_request,
                                                     *credit_card, u"123");
  std::u16string announcement_text = kCreditCardNickname + u" filled.";

  EXPECT_CALL(*accessibility_service(), Announce(announcement_text));
  fast_checkout_client()->OnAfterDidFillAutofillFormData(
      *autofill_manager(), credit_card_form->global_id());
}

TEST_F(FastCheckoutClientImplTest,
       GetSelectedAutofillProfile_ProfileDeletedSinceSelection_StopsRun) {
  std::unique_ptr<autofill::FormStructure> address_form = SetUpAddressForm();

  auto [autofill_profile, credit_card] =
      StartRunAndSelectOptions({address_form->form_signature()});
  AddFormToAutofillManagerCache(std::move(address_form));

  personal_data_manager()->RemoveByGUID(autofill_profile->guid());

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  EXPECT_CALL(*autofill_manager(), FillProfileFormImpl).Times(0);

  fast_checkout_client()->OnAfterLoadedServerPredictions(*autofill_manager());

  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest,
       GetSelectedCreditCard_CardDeletedSinceSelection_StopsRun) {
  std::unique_ptr<autofill::FormStructure> credit_card_form =
      SetUpCreditCardForm();

  StartRunAndSelectOptions({credit_card_form->form_signature()});
  AddFormToAutofillManagerCache(std::move(credit_card_form));

  personal_data_manager()->ClearCreditCards();

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  // `FillCreditCardForm` is currently only called after the CVC popup was
  // resolved. This assertion is a safeguard against potential future changes.
  // E.g. having the popup only for server and masked cards, like in the
  // `BrowserAutofillManager`.
  EXPECT_CALL(*autofill_manager(), FillCreditCardFormImpl).Times(0);

  fast_checkout_client()->OnAfterLoadedServerPredictions(*autofill_manager());

  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest,
       TryToFillForms_LocalCreditCard_ImmediatelyFillsCreditCardForm) {
  autofill::FormStructure* credit_card_form =
      AddFormToAutofillManagerCache(SetUpCreditCardForm());
  const autofill::FormFieldData& field = *credit_card_form->field(0);

  EXPECT_CALL(
      *autofill_manager(),
      SetFastCheckoutRunId(autofill::FieldTypeGroup::kCreditCard, Ne(0)));
  EXPECT_CALL(
      *autofill_manager(),
      FillCreditCardFormImpl(FormDataEqualTo(credit_card_form->ToFormData()),
                             FormFieldDataEqualTo(field), _, Eq(u"")));
  StartRunAndSelectOptions({credit_card_form->form_signature()},
                           /*local_card=*/true);
}

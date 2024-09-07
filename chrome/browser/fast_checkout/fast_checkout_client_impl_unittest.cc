// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/uuid.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_factory.h"
#include "chrome/browser/fast_checkout/fast_checkout_trigger_validator.h"
#include "chrome/browser/fast_checkout/mock_fast_checkout_capabilities_fetcher.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/fast_checkout_delegate.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/gfx/native_widget_types.h"

using ::autofill::AutofillDriver;
using ::autofill::AutofillManager;
using ::autofill::AutofillProfile;
using ::autofill::CreditCard;
using ::autofill::FastCheckoutRunOutcome;
using ::autofill::FastCheckoutTriggerOutcome;
using ::autofill::FastCheckoutUIState;
using ::autofill::FieldGlobalId;
using ::autofill::FormData;
using ::autofill::FormFieldData;
using ::autofill::FormGlobalId;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::UnorderedElementsAre;
using ::ukm::builders::FastCheckout_FormStatus;
using ::ukm::builders::FastCheckout_RunOutcome;

namespace {

Matcher<const autofill::AutofillTriggerDetails&> EqualsAutofilltriggerDetails(
    autofill::AutofillTriggerDetails details) {
  return AllOf(Field(&autofill::AutofillTriggerDetails::trigger_source,
                     details.trigger_source));
}

CreditCard GetEmptyCreditCard() {
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         "");
  autofill::test::SetCreditCardInfo(&credit_card, /*name_on_card=*/"",
                                    /*card_number=*/"",
                                    autofill::test::NextMonth().c_str(),
                                    autofill::test::NextYear().c_str(), "1");
  return credit_card;
}

constexpr char kUrl[] = "https://www.example.com";
constexpr char kOtherUrl[] = "https://www.example2.com";

// The index of the `FormFieldData` in the test credit card form created in
// autofill::test::CreateTestCreditCardForm.
constexpr size_t kCreditCardFieldIndexInForm = 2;
constexpr size_t kFirstNameFieldIndexInForm = 0;

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
  personal_data_manager->test_address_data_manager().SetAutofillProfileEnabled(
      true);
  personal_data_manager->test_payments_data_manager()
      .SetAutofillPaymentMethodsEnabled(true);
  personal_data_manager->test_payments_data_manager()
      .SetAutofillWalletImportEnabled(true);
  personal_data_manager->address_data_manager().AddProfile(kProfile1);
  personal_data_manager->address_data_manager().AddProfile(kProfile2);
  // Add incomplete autofill profile, should not be shown on the sheet.
  personal_data_manager->address_data_manager().AddProfile(kIncompleteProfile);
  personal_data_manager->payments_data_manager().AddCreditCard(kCreditCard1);
  personal_data_manager->payments_data_manager().AddCreditCard(kCreditCard2);
  // Add empty credit card, should not be shown on the sheet.
  personal_data_manager->payments_data_manager().AddCreditCard(
      kEmptyCreditCard);
  return personal_data_manager;
}

class MockFastCheckoutController : public FastCheckoutController {
 public:
  MockFastCheckoutController() : FastCheckoutController() {}
  ~MockFastCheckoutController() override = default;

  MOCK_METHOD(void,
              Show,
              (const std::vector<const AutofillProfile*>& autofill_profiles,
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
              (const FormData&,
               const FormFieldData&,
               base::WeakPtr<AutofillManager>),
              (override));
  MOCK_METHOD(bool,
              IntendsToShowFastCheckout,
              (AutofillManager&, FormGlobalId, FieldGlobalId, const FormData&),
              (const, override));
  MOCK_METHOD(bool, IsShowingFastCheckoutUI, (), (const, override));
  MOCK_METHOD(void, HideFastCheckout, (bool), (override));
};

class MockBrowserAutofillManager : public autofill::TestBrowserAutofillManager {
 public:
  using autofill::TestBrowserAutofillManager::TestBrowserAutofillManager;

  MOCK_METHOD(void,
              TriggerFormExtractionInAllFrames,
              (base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void,
              FillOrPreviewProfileForm,
              (autofill::mojom::ActionPersistence,
               const FormData&,
               const FormFieldData&,
               const autofill::AutofillProfile&,
               const autofill::AutofillTriggerDetails&),
              (override));
  MOCK_METHOD(void,
              FillOrPreviewCreditCardForm,
              (autofill::mojom::ActionPersistence action_persistence,
               const FormData&,
               const FormFieldData&,
               const autofill::CreditCard&,
               const std::u16string&,
               const autofill::AutofillTriggerDetails&),
              (override));
  MOCK_METHOD(void,
              SetFastCheckoutRunId,
              (autofill::FieldTypeGroup, int64_t),
              (override));
};

class TestFastCheckoutClientImpl : public FastCheckoutClientImpl {
 public:
  explicit TestFastCheckoutClientImpl(autofill::ContentAutofillClient* client)
      : FastCheckoutClientImpl(client) {}

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

  MOCK_METHOD(FastCheckoutTriggerOutcome,
              ShouldRun,
              (const FormData&,
               const FormFieldData&,
               const FastCheckoutUIState,
               const bool,
               const AutofillManager&),
              (const));
  MOCK_METHOD(FastCheckoutTriggerOutcome, HasValidPersonalData, (), (const));
};

class MockAutofillClient : public autofill::TestContentAutofillClient {
 public:
  using autofill::TestContentAutofillClient::TestContentAutofillClient;
  MOCK_METHOD(void,
              HideAutofillSuggestions,
              (autofill::SuggestionHidingReason),
              ());
};

class MockFastCheckoutAccessibilityService
    : public FastCheckoutAccessibilityService {
 public:
  MockFastCheckoutAccessibilityService() = default;
  ~MockFastCheckoutAccessibilityService() override = default;

  MOCK_METHOD(void, Announce, (const std::u16string&), (override));
};

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

// TODO(crbug.com/348576043) : Tests are failing on android-arm64-tests builder.
class DISABLED_FastCheckoutClientImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  DISABLED_FastCheckoutClientImplTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    autofill::PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(),
        base::BindRepeating(&BuildTestPersonalDataManager));
    FastCheckoutCapabilitiesFetcherFactory::GetInstance()
        ->SetTestingSubclassFactoryAndUse(
            profile(), base::BindOnce([](content::BrowserContext*) {
              return std::make_unique<
                  NiceMock<MockFastCheckoutCapabilitiesFetcher>>();
            }));

    test_client_ =
        std::make_unique<TestFastCheckoutClientImpl>(autofill_client());

    // Prepare the FastCheckoutController.
    auto fast_checkout_controller =
        std::make_unique<NiceMock<MockFastCheckoutController>>();
    fast_checkout_controller_ = fast_checkout_controller.get();
    test_client_->InjectFastCheckoutController(
        std::move(fast_checkout_controller));

    autofill_client()
        ->GetPaymentsAutofillClient()
        ->set_test_payments_network_interface(
            std::make_unique<autofill::payments::TestPaymentsNetworkInterface>(
                autofill_client()->GetURLLoaderFactory(),
                autofill_client()->GetIdentityManager(),
                autofill_client()->GetPersonalDataManager()));
    auto trigger_validator =
        std::make_unique<NiceMock<MockFastCheckoutTriggerValidator>>();
    validator_ = trigger_validator.get();
    test_client_->trigger_validator_ = std::move(trigger_validator);
    ON_CALL(*validator(), ShouldRun)
        .WillByDefault(Return(FastCheckoutTriggerOutcome::kSuccess));

    test_client_->autofill_client_ = autofill_client();

    auto accessibility_service =
        std::make_unique<NiceMock<MockFastCheckoutAccessibilityService>>();
    accessibility_service_ = accessibility_service.get();
    fast_checkout_client()->accessibility_service_ =
        std::move(accessibility_service);

    // Creates the AutofillDriver and AutofillManager.
    NavigateAndCommit(GURL("about:blank"));
    autofill_manager()->set_fast_checkout_delegate(
        std::make_unique<MockFastCheckoutDelegate>());
  }

  autofill::TestPersonalDataManager* personal_data_manager() {
    return static_cast<autofill::TestPersonalDataManager*>(
        autofill::PersonalDataManagerFactory::GetForBrowserContext(profile()));
  }

  FastCheckoutClientImpl* fast_checkout_client() { return test_client_.get(); }

  MockFastCheckoutController* fast_checkout_controller() {
    return fast_checkout_controller_;
  }

  MockFastCheckoutTriggerValidator* validator() { return validator_.get(); }

  MockAutofillClient* autofill_client() {
    return autofill_client_injector_[web_contents()];
  }

  MockBrowserAutofillManager* autofill_manager() {
    return autofill_manager_injector_[web_contents()];
  }

  MockFastCheckoutAccessibilityService* accessibility_service() {
    return accessibility_service_;
  }

  MockFastCheckoutDelegate& fast_checkout_delegate() {
    return *static_cast<MockFastCheckoutDelegate*>(
        autofill_manager()->fast_checkout_delegate());
  }

  ukm::TestAutoSetUkmRecorder ukm_recorder_;

  // Sets up test data, calls `TryToStart(..)` and `OnOptionsSelected(..)`.
  std::tuple<const autofill::AutofillProfile*, autofill::CreditCard*>
  StartRunAndSelectOptions(
      const base::flat_set<autofill::FormSignature>& forms_to_fill,
      bool local_card = false) {
    auto autofill_profile_unique_ptr =
        std::make_unique<autofill::AutofillProfile>(
            autofill::test::GetFullProfile());
    autofill_profile_unique_ptr->set_profile_label(
        base::UTF16ToUTF8(kAutofillProfileLabel));
    personal_data_manager()->address_data_manager().AddProfile(
        *autofill_profile_unique_ptr);
    auto credit_card_unique_ptr = std::make_unique<autofill::CreditCard>(
        local_card ? autofill::test::GetCreditCard()
                   : autofill::test::GetMaskedServerCard());
    credit_card_unique_ptr->SetNickname(kCreditCardNickname);
    if (local_card) {
      personal_data_manager()->payments_data_manager().AddCreditCard(
          *credit_card_unique_ptr);
    } else {
      personal_data_manager()->test_payments_data_manager().AddServerCreditCard(
          *credit_card_unique_ptr);
    }

    MockFastCheckoutCapabilitiesFetcher* fetcher =
        static_cast<MockFastCheckoutCapabilitiesFetcher*>(
            FastCheckoutCapabilitiesFetcherFactory::GetForBrowserContext(
                profile()));

    EXPECT_CALL(*fetcher, GetFormsToFill(url::Origin::Create(GURL(kUrl))))
        .WillOnce(Return(forms_to_fill));
    OnBeforeAskForValuesToFill();
    EXPECT_TRUE(fast_checkout_client()->TryToStart(
        GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
        autofill_manager()->GetWeakPtr()));
    OnAfterAskForValuesToFill();
    fast_checkout_client()->OnOptionsSelected(
        std::move(autofill_profile_unique_ptr),
        std::move(credit_card_unique_ptr));
    return {
        personal_data_manager()->address_data_manager().GetProfileByGUID(
            fast_checkout_client()->selected_autofill_profile_guid_.value()),
        (local_card
             ? personal_data_manager()
                   ->payments_data_manager()
                   .GetCreditCardByGUID(
                       fast_checkout_client()->selected_credit_card_id_.value())
             : personal_data_manager()
                   ->payments_data_manager()
                   .GetCreditCardByServerId(
                       fast_checkout_client()
                           ->selected_credit_card_id_.value()))};
  }

  std::unique_ptr<autofill::FormStructure> SetUpCreditCardForm() {
    auto credit_card_form_structure = std::make_unique<autofill::FormStructure>(
        autofill::test::CreateTestCreditCardFormData(true, false, true));
    credit_card_form_structure->field(kCreditCardFieldIndexInForm)
        ->set_heuristic_type(autofill::GetActiveHeuristicSource(),
                             autofill::FieldType::CREDIT_CARD_NUMBER);
    return credit_card_form_structure;
  }

  std::unique_ptr<autofill::FormStructure> SetUpAddressForm() {
    auto address_form_structure = std::make_unique<autofill::FormStructure>(
        autofill::test::CreateTestAddressFormData());
    address_form_structure->field(kFirstNameFieldIndexInForm)
        ->set_heuristic_type(autofill::GetActiveHeuristicSource(),
                             autofill::FieldType::NAME_FIRST);
    return address_form_structure;
  }

  autofill::FormStructure* AddFormToAutofillManagerCache(
      std::unique_ptr<autofill::FormStructure> form) {
    autofill::FormStructure* form_ptr = form.get();
    autofill_manager()->AddSeenFormStructure(std::move(form));
    return form_ptr;
  }

  void ExpectRunOutcomeUkm(FastCheckoutRunOutcome run_outcome) {
    auto ukm_entries =
        ukm_recorder_.GetEntries(FastCheckout_RunOutcome::kEntryName,
                                 {FastCheckout_RunOutcome::kRunOutcomeName,
                                  FastCheckout_RunOutcome::kRunIdName});
    EXPECT_EQ(ukm_entries.size(), 1UL);
    EXPECT_EQ(ukm_entries[0].metrics.at("RunOutcome"),
              static_cast<long>(run_outcome));
    EXPECT_NE(ukm_entries[0].metrics.at("RunId"), 0L);
  }

  void OnBeforeAskForValuesToFill() {
    EXPECT_CALL(fast_checkout_delegate(), IsShowingFastCheckoutUI)
        .WillOnce(Return(false));
    EXPECT_CALL(fast_checkout_delegate(), IntendsToShowFastCheckout)
        .WillOnce(Return(true));
    fast_checkout_client()
        ->keyboard_suppressor_for_test()
        .OnBeforeAskForValuesToFill(*autofill_manager(), some_form_,
                                    some_field_, some_form_data_);
    EXPECT_TRUE(fast_checkout_client()
                    ->keyboard_suppressor_for_test()
                    .is_suppressing());
  }

  void OnAfterAskForValuesToFill() {
    EXPECT_TRUE(fast_checkout_client()
                    ->keyboard_suppressor_for_test()
                    .is_suppressing());
    EXPECT_CALL(fast_checkout_delegate(), IsShowingFastCheckoutUI)
        .WillOnce(Return(true));
    fast_checkout_client()
        ->keyboard_suppressor_for_test()
        .OnAfterAskForValuesToFill(*autofill_manager(), some_form_,
                                   some_field_);
    EXPECT_TRUE(fast_checkout_client()
                    ->keyboard_suppressor_for_test()
                    .is_suppressing());
  }

 private:
  // Required for using some `autofill::test` functions inside the test class.
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  autofill::TestAutofillClientInjector<NiceMock<MockAutofillClient>>
      autofill_client_injector_;
  autofill::TestAutofillDriverInjector<autofill::TestContentAutofillDriver>
      autofill_driver_injector_;
  autofill::TestAutofillManagerInjector<NiceMock<MockBrowserAutofillManager>>
      autofill_manager_injector_;
  std::unique_ptr<TestFastCheckoutClientImpl> test_client_;
  raw_ptr<MockFastCheckoutController> fast_checkout_controller_;
  raw_ptr<MockFastCheckoutTriggerValidator> validator_;
  raw_ptr<MockFastCheckoutAccessibilityService> accessibility_service_;
  FormData some_form_data_ =
      autofill::test::CreateTestCreditCardFormData(/*is_https=*/true,
                                                   /*use_month_type=*/false);
  FormGlobalId some_form_ = some_form_data_.global_id();
  FieldGlobalId some_field_ = autofill::test::MakeFieldGlobalId();
};

MATCHER_P(FormDataEqualTo,
          form_data,
          "Compares two autofill::FormData instances with their DeepEqual "
          "function.") {
  return FormData::DeepEqual(arg, form_data);
}

MATCHER_P(FormFieldDataEqualTo,
          form_data,
          "Compares two autofill::FormFieldData instances with their DeepEqual "
          "function.") {
  return FormFieldData::DeepEqual(arg, form_data);
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       Start_InvalidAutofillManager_NoRun) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Do not expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(0);
  // Do not expect Autofill popups to be hidden.
  EXPECT_CALL(*autofill_client(), HideAutofillSuggestions).Times(0);

  OnBeforeAskForValuesToFill();
  EXPECT_FALSE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(), nullptr));
  EXPECT_TRUE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       Start_ShouldRunReturnsInvalidData_NoRun) {
  ON_CALL(*validator(), ShouldRun)
      .WillByDefault(
          Return(FastCheckoutTriggerOutcome::kFailureNoValidAutofillProfile));

  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  OnBeforeAskForValuesToFill();
  EXPECT_FALSE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));
  EXPECT_TRUE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       Start_ShouldRunReturnsUnsupportedFile_NoRun) {
  ON_CALL(*validator(), ShouldRun)
      .WillByDefault(Return(FastCheckoutTriggerOutcome::kUnsupportedFieldType));

  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Do not expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(0);
  // Do not expect Autofill popups to be hidden.
  EXPECT_CALL(*autofill_client(), HideAutofillSuggestions).Times(0);

  EXPECT_FALSE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));
  EXPECT_TRUE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest, Start_ShouldRunReturnsSuccess_Run) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  OnBeforeAskForValuesToFill();

  // Expect the bottomsheet to show up.
  EXPECT_CALL(
      *fast_checkout_controller(),
      Show(UnorderedElementsAre(Pointee(kProfile1), Pointee(kProfile2),
                                Pointee(kIncompleteProfile)),
           UnorderedElementsAre(Pointee(kCreditCard1), Pointee(kCreditCard2))));
  // Expect call to `HideAutofillSuggestions`.
  EXPECT_CALL(*autofill_client(),
              HideAutofillSuggestions(autofill::SuggestionHidingReason::
                                          kOverlappingWithFastCheckoutSurface));

  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));
  OnAfterAskForValuesToFill();

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  EXPECT_TRUE(fast_checkout_client()->IsShowing());
  EXPECT_FALSE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       OnPersonalDataChanged_StopIfInvalidPersonalData) {
  ON_CALL(*validator(), HasValidPersonalData)
      .WillByDefault(
          Return(FastCheckoutTriggerOutcome::kFailureNoValidAutofillProfile));

  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  OnBeforeAskForValuesToFill();

  // Expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(1);

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));
  OnAfterAskForValuesToFill();

  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

  // User removes all the profiles.
  personal_data_manager()->test_address_data_manager().ClearProfiles();
  // User adds an incomplete profile only.
  personal_data_manager()->address_data_manager().AddProfile(
      autofill::test::GetIncompleteProfile1());

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  ExpectRunOutcomeUkm(FastCheckoutRunOutcome::kInvalidPersonalData);
  EXPECT_FALSE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       OnPersonalDataChanged_UpdatesTheUIWithNewData) {
  ON_CALL(*validator(), HasValidPersonalData)
      .WillByDefault(Return(FastCheckoutTriggerOutcome::kSuccess));

  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  OnBeforeAskForValuesToFill();

  EXPECT_CALL(
      *fast_checkout_controller(),
      Show(UnorderedElementsAre(Pointee(kProfile1), Pointee(kProfile2),
                                Pointee(kIncompleteProfile)),
           UnorderedElementsAre(Pointee(kCreditCard1), Pointee(kCreditCard2))));

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));
  OnAfterAskForValuesToFill();

  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

  // Expect bottomsheet to display the updated info.
  EXPECT_CALL(*fast_checkout_controller(),
              Show(UnorderedElementsAre(Pointee(kProfile1), Pointee(kProfile2),
                                        Pointee(kIncompleteProfile)),
                   UnorderedElementsAre(Pointee(kCreditCard1))));

  // User removes all valid credit cards and adds a valid card.
  personal_data_manager()->test_payments_data_manager().ClearCreditCards();
  personal_data_manager()->payments_data_manager().AddCreditCard(kCreditCard1);

  // `FastCheckoutClient` is still running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  EXPECT_FALSE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest, Stop_WhenIsRunning_CancelsTheRun) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  EXPECT_FALSE(fast_checkout_client()->IsShowing());
  EXPECT_TRUE(fast_checkout_client()->IsNotShownYet());

  OnBeforeAskForValuesToFill();
  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));
  OnAfterAskForValuesToFill();

  // Fast Checkout is running and showing the bottomsheet.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  EXPECT_TRUE(fast_checkout_client()->IsShowing());

  // Stopping the run.
  fast_checkout_client()->Stop(/*allow_further_runs=*/false);

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  EXPECT_FALSE(fast_checkout_client()->IsShowing());
  EXPECT_FALSE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       OnDismiss_WhenIsRunning_CancelsTheRun) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  OnBeforeAskForValuesToFill();
  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));
  OnAfterAskForValuesToFill();

  fast_checkout_client()->OnDismiss();

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  ExpectRunOutcomeUkm(FastCheckoutRunOutcome::kBottomsheetDismissed);
  EXPECT_FALSE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       DestroyingAutofillDriver_ResetsAutofillManagerPointer) {
  // Set up Autofill instances so that `FastCheckoutClient::Stop(..)` will be
  // called when `autofill_driver` is destroyed below. `Stop(..)` is supposed to
  // reset `FastCheckoutClientImpl::autofill_manager_`.
  autofill::ChromeAutofillClient::CreateForWebContents(web_contents());
  auto autofill_driver = std::make_unique<autofill::ContentAutofillDriver>(
      web_contents()->GetPrimaryMainFrame(),
      &autofill::ContentAutofillClient::FromWebContents(web_contents())
           ->GetAutofillDriverFactory());
  autofill::BrowserAutofillManager& autofill_manager =
      static_cast<autofill::BrowserAutofillManager&>(
          autofill_driver->GetAutofillManager());

  // `FastCheckoutClientImpl::autofill_manager_` is `nullptr` initially.
  EXPECT_FALSE(fast_checkout_client()->autofill_manager_);
  OnBeforeAskForValuesToFill();

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager.GetWeakPtr()));
  OnAfterAskForValuesToFill();

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
  EXPECT_TRUE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       OnOptionsSelected_ServerCard_SavesFormsAndAutofillDataSelections) {
  std::unique_ptr<autofill::FormStructure> address_form = SetUpAddressForm();
  std::unique_ptr<autofill::FormStructure> credit_card_form =
      SetUpCreditCardForm();

  EXPECT_CALL(*autofill_manager(), TriggerFormExtractionInAllFrames);

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
  EXPECT_FALSE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       OnOptionsSelected_LocalCard_SavesFormsAndAutofillDataSelections) {
  autofill::FormStructure* credit_card_form =
      AddFormToAutofillManagerCache(SetUpCreditCardForm());

  EXPECT_CALL(*autofill_manager(), TriggerFormExtractionInAllFrames);

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
  EXPECT_FALSE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       OnAfterLoadedServerPredictions_FillsForms) {
  std::unique_ptr<autofill::FormStructure> address_form = SetUpAddressForm();
  std::unique_ptr<autofill::FormStructure> credit_card_form =
      SetUpCreditCardForm();
  autofill::FormSignature address_form_signature =
      address_form->form_signature();
  autofill::FormSignature credit_card_form_signature =
      credit_card_form->form_signature();
  FormData address_form_data = address_form->ToFormData();
  FormFieldData address_form_field_data =
      *address_form->field(kFirstNameFieldIndexInForm);

  auto [autofill_profile, credit_card] = StartRunAndSelectOptions(
      {address_form_signature, credit_card_form_signature});

  AddFormToAutofillManagerCache(std::move(address_form));
  AddFormToAutofillManagerCache(std::move(credit_card_form));

  // Reset filling states.
  for (auto& [form_id, filling_state] :
       fast_checkout_client()->form_filling_states_) {
    filling_state = FastCheckoutClientImpl::FillingState::kNotFilled;
  }

  EXPECT_CALL(
      *autofill_manager(),
      FillOrPreviewProfileForm(
          autofill::mojom::ActionPersistence::kFill,
          FormDataEqualTo(address_form_data),
          FormFieldDataEqualTo(address_form_field_data), Eq(*autofill_profile),
          EqualsAutofilltriggerDetails(
              {.trigger_source =
                   autofill::AutofillTriggerSource::kFastCheckout})));
  EXPECT_CALL(*autofill_manager(),
              SetFastCheckoutRunId(autofill::FieldTypeGroup::kAddress,
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
  EXPECT_FALSE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
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
      autofill_client()
          ->GetPaymentsAutofillClient()
          ->GetCvcAuthenticator()
          .GetFullCardRequest();
  std::u16string cvc = u"123";
  const FormFieldData& field =
      *credit_card_form->field(kCreditCardFieldIndexInForm);

  EXPECT_CALL(*autofill_manager(),
              FillOrPreviewCreditCardForm(
                  autofill::mojom::ActionPersistence::kFill,
                  FormDataEqualTo(credit_card_form->ToFormData()),
                  FormFieldDataEqualTo(field), Eq(*credit_card), Eq(cvc),
                  EqualsAutofilltriggerDetails(
                      {.trigger_source =
                           autofill::AutofillTriggerSource::kFastCheckout})));
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
  EXPECT_FALSE(fast_checkout_client()->IsNotShownYet());
  ExpectRunOutcomeUkm(FastCheckoutRunOutcome::kSuccess);
  auto ukm_entries =
      ukm_recorder_.GetEntries(FastCheckout_FormStatus::kEntryName,
                               {FastCheckout_FormStatus::kRunIdName,
                                FastCheckout_FormStatus::kFilledName,
                                FastCheckout_FormStatus::kFormSignatureName,
                                FastCheckout_FormStatus::kFormTypesName});
  EXPECT_EQ(ukm_entries.size(), 2UL);
  base::flat_set<ukm::TestAutoSetUkmRecorder::HumanReadableUkmMetrics> metrics;
  metrics.emplace(ukm_entries[0].metrics);
  metrics.emplace(ukm_entries[1].metrics);
  EXPECT_THAT(
      metrics,
      UnorderedElementsAre(
          UnorderedElementsAre(
              Pair(FastCheckout_FormStatus::kRunIdName, run_id),
              Pair(FastCheckout_FormStatus::kFilledName, 1),
              Pair(FastCheckout_FormStatus::kFormSignatureName,
                   autofill::HashFormSignature(address_form->form_signature())),
              Pair(FastCheckout_FormStatus::kFormTypesName, 2)),
          UnorderedElementsAre(
              Pair(FastCheckout_FormStatus::kRunIdName, run_id),
              Pair(FastCheckout_FormStatus::kFilledName, 1),
              Pair(FastCheckout_FormStatus::kFormSignatureName,
                   autofill::HashFormSignature(
                       credit_card_form->form_signature())),
              Pair(FastCheckout_FormStatus::kFormTypesName, 4))));
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       OnAutofillManagerReset_IsShowing_ResetsState) {
  using enum autofill::AutofillManager::LifecycleState;
  OnBeforeAskForValuesToFill();
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));
  OnAfterAskForValuesToFill();

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  EXPECT_TRUE(fast_checkout_client()->IsShowing());
  fast_checkout_client()->OnAutofillManagerStateChanged(*autofill_manager(),
                                                        kActive, kPendingReset);
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  ExpectRunOutcomeUkm(
      FastCheckoutRunOutcome::kNavigationWhileBottomsheetWasShown);
  EXPECT_TRUE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       OnAutofillManagerReset_IsNotShowing_ResetsState) {
  using enum autofill::AutofillManager::LifecycleState;
  std::unique_ptr<autofill::FormStructure> address_form = SetUpAddressForm();

  StartRunAndSelectOptions({address_form->form_signature()});
  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  EXPECT_FALSE(fast_checkout_client()->IsShowing());
  fast_checkout_client()->OnAutofillManagerStateChanged(*autofill_manager(),
                                                        kActive, kPendingReset);
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  ExpectRunOutcomeUkm(FastCheckoutRunOutcome::kPageRefreshed);
  EXPECT_TRUE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       OnAutofillManagerDestroyed_ResetsState) {
  using enum autofill::AutofillManager::LifecycleState;
  OnBeforeAskForValuesToFill();
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));
  OnAfterAskForValuesToFill();

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  fast_checkout_client()->OnAutofillManagerStateChanged(
      *autofill_manager(), kActive, kPendingDeletion);
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  ExpectRunOutcomeUkm(FastCheckoutRunOutcome::kAutofillManagerDestroyed);
  EXPECT_TRUE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       TimeoutTimer_ThirtyMinutesPassed_StopsRun) {
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
  EXPECT_TRUE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest, OnNavigation_OtherUrl_StopsRun) {
  OnBeforeAskForValuesToFill();
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));
  OnAfterAskForValuesToFill();

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  fast_checkout_client()->OnNavigation(GURL(kOtherUrl), false);
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  ExpectRunOutcomeUkm(FastCheckoutRunOutcome::kOriginChange);
  EXPECT_TRUE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       OnNavigation_SameUrlButNoCartOrCheckoutPage_StopsRun) {
  OnBeforeAskForValuesToFill();
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));
  OnAfterAskForValuesToFill();

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  fast_checkout_client()->OnNavigation(GURL(kUrl), false);
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  ExpectRunOutcomeUkm(FastCheckoutRunOutcome::kNonCheckoutPage);
  EXPECT_TRUE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       OnNavigation_SameUrlAndCartOrCheckoutPage_DoesNotStopRun) {
  OnBeforeAskForValuesToFill();
  EXPECT_TRUE(fast_checkout_client()->TryToStart(
      GURL(kUrl), autofill::FormData(), autofill::FormFieldData(),
      autofill_manager()->GetWeakPtr()));
  OnAfterAskForValuesToFill();

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  fast_checkout_client()->OnNavigation(GURL(kUrl), true);
  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  EXPECT_FALSE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       OnFullCardRequestSucceeded_InvokesCreditCardFormFill) {
  autofill::FormStructure* address_form =
      AddFormToAutofillManagerCache(SetUpAddressForm());
  autofill::FormStructure* credit_card_form =
      AddFormToAutofillManagerCache(SetUpCreditCardForm());

  auto [autofill_profile, credit_card] = StartRunAndSelectOptions(
      {address_form->form_signature(), credit_card_form->form_signature()});
  autofill::payments::FullCardRequest* full_card_request =
      autofill_client()
          ->GetPaymentsAutofillClient()
          ->GetCvcAuthenticator()
          .GetFullCardRequest();
  const FormFieldData& field =
      *credit_card_form->field(kCreditCardFieldIndexInForm);
  std::u16string cvc = u"123";

  EXPECT_CALL(*autofill_manager(),
              FillOrPreviewCreditCardForm(
                  autofill::mojom::ActionPersistence::kFill,
                  FormDataEqualTo(credit_card_form->ToFormData()),
                  FormFieldDataEqualTo(field), Eq(*credit_card), Eq(cvc),
                  EqualsAutofilltriggerDetails(
                      {.trigger_source =
                           autofill::AutofillTriggerSource::kFastCheckout})));
  EXPECT_CALL(*autofill_manager(),
              SetFastCheckoutRunId(autofill::FieldTypeGroup::kCreditCard,
                                   fast_checkout_client()->run_id_));
  fast_checkout_client()->OnFullCardRequestSucceeded(*full_card_request,
                                                     *credit_card, cvc);
  EXPECT_FALSE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(
    DISABLED_FastCheckoutClientImplTest,
    OnAfterDidFillAutofillFormData_AddressForm_MakesAddressFormA11yAnnouncement) {
  autofill::FormStructure* address_form =
      AddFormToAutofillManagerCache(SetUpAddressForm());
  StartRunAndSelectOptions({address_form->form_signature()});
  std::u16string announcement_text =
      kAutofillProfileLabel + u" address form filled.";

  EXPECT_CALL(*accessibility_service(), Announce(announcement_text));
  fast_checkout_client()->OnAfterDidFillAutofillFormData(
      *autofill_manager(), address_form->global_id());
  EXPECT_FALSE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(
    DISABLED_FastCheckoutClientImplTest,
    OnAfterDidFillAutofillFormData_EmailForm_MakesEmailFormA11yAnnouncement) {
  autofill::FormStructure* address_form =
      AddFormToAutofillManagerCache(SetUpAddressForm());
  address_form->field(0)->set_heuristic_type(
      autofill::GetActiveHeuristicSource(), autofill::FieldType::EMAIL_ADDRESS);
  StartRunAndSelectOptions({address_form->form_signature()});
  std::u16string announcement_text = u"Email filled.";

  EXPECT_CALL(*accessibility_service(), Announce(announcement_text));
  fast_checkout_client()->OnAfterDidFillAutofillFormData(
      *autofill_manager(), address_form->global_id());
  EXPECT_FALSE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(
    DISABLED_FastCheckoutClientImplTest,
    OnAfterDidFillAutofillFormData_CreditCardForm_MakesCreditCardFormA11yAnnouncement) {
  autofill::FormStructure* credit_card_form =
      AddFormToAutofillManagerCache(SetUpCreditCardForm());
  auto [autofill_profile, credit_card] =
      StartRunAndSelectOptions({credit_card_form->form_signature()});
  autofill::payments::FullCardRequest* full_card_request =
      autofill_client()
          ->GetPaymentsAutofillClient()
          ->GetCvcAuthenticator()
          .GetFullCardRequest();
  fast_checkout_client()->OnFullCardRequestSucceeded(*full_card_request,
                                                     *credit_card, u"123");
  std::u16string announcement_text = kCreditCardNickname + u" filled.";

  EXPECT_CALL(*accessibility_service(), Announce(announcement_text));
  fast_checkout_client()->OnAfterDidFillAutofillFormData(
      *autofill_manager(), credit_card_form->global_id());
  EXPECT_FALSE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       GetSelectedAutofillProfile_ProfileDeletedSinceSelection_StopsRun) {
  std::unique_ptr<autofill::FormStructure> address_form = SetUpAddressForm();

  auto [autofill_profile, credit_card] =
      StartRunAndSelectOptions({address_form->form_signature()});
  AddFormToAutofillManagerCache(std::move(address_form));

  personal_data_manager()->RemoveByGUID(autofill_profile->guid());

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  EXPECT_CALL(*autofill_manager(), FillOrPreviewProfileForm).Times(0);

  fast_checkout_client()->OnAfterLoadedServerPredictions(*autofill_manager());

  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  EXPECT_TRUE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       GetSelectedCreditCard_CardDeletedSinceSelection_StopsRun) {
  std::unique_ptr<autofill::FormStructure> credit_card_form =
      SetUpCreditCardForm();

  StartRunAndSelectOptions({credit_card_form->form_signature()});
  AddFormToAutofillManagerCache(std::move(credit_card_form));

  personal_data_manager()->test_payments_data_manager().ClearCreditCards();

  EXPECT_TRUE(fast_checkout_client()->IsRunning());
  // `FillCreditCardForm` is currently only called after the CVC popup was
  // resolved. This assertion is a safeguard against potential future changes.
  // E.g. having the popup only for server and masked cards, like in the
  // `BrowserAutofillManager`.
  EXPECT_CALL(*autofill_manager(), FillOrPreviewCreditCardForm).Times(0);

  fast_checkout_client()->OnAfterLoadedServerPredictions(*autofill_manager());

  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  EXPECT_TRUE(fast_checkout_client()->IsNotShownYet());
}

TEST_F(DISABLED_FastCheckoutClientImplTest,
       TryToFillForms_LocalCreditCard_ImmediatelyFillsCreditCardForm) {
  autofill::FormStructure* credit_card_form =
      AddFormToAutofillManagerCache(SetUpCreditCardForm());
  const FormFieldData& field =
      *credit_card_form->field(kCreditCardFieldIndexInForm);

  EXPECT_CALL(
      *autofill_manager(),
      SetFastCheckoutRunId(autofill::FieldTypeGroup::kCreditCard, Ne(0)));
  EXPECT_CALL(*autofill_manager(),
              FillOrPreviewCreditCardForm(
                  autofill::mojom::ActionPersistence::kFill,
                  FormDataEqualTo(credit_card_form->ToFormData()),
                  FormFieldDataEqualTo(field), _, Eq(u""),
                  EqualsAutofilltriggerDetails(
                      {.trigger_source =
                           autofill::AutofillTriggerSource::kFastCheckout})));
  StartRunAndSelectOptions({credit_card_form->form_signature()},
                           /*local_card=*/true);
  EXPECT_FALSE(fast_checkout_client()->IsNotShownYet());
}

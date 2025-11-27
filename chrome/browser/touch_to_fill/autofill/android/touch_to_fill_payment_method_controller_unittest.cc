// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_delegate_android_impl.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_controller_impl.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_delegate.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/grit/components_scaled_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/range/range.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::Matcher;
using ::testing::Ref;
using ::testing::Return;

Matcher<const payments::BnplIssuerContext&> EqualBnplIssuerContext(
    const payments::BnplIssuerContext& bnpl_issuer_context) {
  return AllOf(
      Field(&payments::BnplIssuerContext::issuer, bnpl_issuer_context.issuer),
      Field(&payments::BnplIssuerContext::eligibility,
            bnpl_issuer_context.eligibility));
}

class MockTouchToFillPaymentMethodViewImpl : public TouchToFillPaymentMethodView {
 public:
  MockTouchToFillPaymentMethodViewImpl() {
    ON_CALL(*this, ShowPaymentMethods).WillByDefault(Return(true));
    ON_CALL(*this, ShowIbans).WillByDefault(Return(true));
    ON_CALL(*this, ShowBnplIssuerTos).WillByDefault(Return(true));
    ON_CALL(*this, ShowBnplIssuers).WillByDefault(Return(true));
  }
  ~MockTouchToFillPaymentMethodViewImpl() override = default;

  MOCK_METHOD(bool,
              ShowPaymentMethods,
              ((TouchToFillPaymentMethodViewController * controller),
               (base::span<const Suggestion> suggestions),
               (bool should_show_scan_credit_card)));
  MOCK_METHOD(bool,
              ShowIbans,
              (TouchToFillPaymentMethodViewController * controller,
               base::span<const Iban> ibans_to_suggest));
  MOCK_METHOD(bool,
              ShowLoyaltyCards,
              (TouchToFillPaymentMethodViewController * controller,
               base::span<const LoyaltyCard> affiliated_loyalty_cards,
               base::span<const LoyaltyCard> all_loyalty_cards,
               bool first_time_usage));
  MOCK_METHOD(
      bool,
      OnPurchaseAmountExtracted,
      (const TouchToFillPaymentMethodViewController& controller,
       base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
       std::optional<int64_t> extracted_amount,
       bool is_amount_supported_by_any_issuer,
       const std::optional<std::string>& app_locale));
  MOCK_METHOD(bool,
              ShowProgressScreen,
              (TouchToFillPaymentMethodViewController * controller));
  MOCK_METHOD(
      bool,
      ShowBnplIssuers,
      (const TouchToFillPaymentMethodViewController& controller,
       base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
       const std::string& app_locale));
  MOCK_METHOD(bool,
              ShowErrorScreen,
              (TouchToFillPaymentMethodViewController * controller,
               const std::u16string& title,
               const std::u16string& description));
  MOCK_METHOD(bool,
              ShowBnplIssuerTos,
              (const TouchToFillPaymentMethodViewController& controller,
               const payments::BnplIssuerTosDetail& bnpl_issuer_tos_detail));
  MOCK_METHOD(void, Hide, ());
  MOCK_METHOD(void, SetVisible, (bool visible));
};

class MockTouchToFillDelegateAndroidImpl
    : public TouchToFillDelegateAndroidImpl {
 public:
  explicit MockTouchToFillDelegateAndroidImpl(
      TestBrowserAutofillManager* autofill_manager)
      : TouchToFillDelegateAndroidImpl(autofill_manager) {
    ON_CALL(*this, ShouldShowScanCreditCard).WillByDefault(Return(true));
  }
  ~MockTouchToFillDelegateAndroidImpl() override = default;

  base::WeakPtr<MockTouchToFillDelegateAndroidImpl> GetWeakPointer() {
    return weak_factory_.GetWeakPtr();
  }

  MOCK_METHOD(bool, IsShowingTouchToFill, (), (override));
  MOCK_METHOD(bool,
              IntendsToShowTouchToFill,
              (FormGlobalId, FieldGlobalId),
              (override));
  MOCK_METHOD(bool, ShouldShowScanCreditCard, (), (override));
  MOCK_METHOD(void, ScanCreditCard, (), (override));
  MOCK_METHOD(void, OnCreditCardScanned, (const CreditCard& card), (override));
  MOCK_METHOD(void, ShowPaymentMethodSettings, (), (override));
  MOCK_METHOD(void,
              CreditCardSuggestionSelected,
              (std::string unique_id, bool is_virtual),
              (override));
  MOCK_METHOD(void,
              BnplSuggestionSelected,
              (std::optional<int64_t> extracted_amount),
              (override));
  MOCK_METHOD(void,
              OnDismissed,
              (bool dismissed_by_user, bool should_reshow),
              (override));
  MOCK_METHOD(void,
              SetCancelCallback,
              (base::OnceClosure cancel_callback),
              (override));
  MOCK_METHOD(void,
              SetSelectedIssuerCallback,
              (base::OnceCallback<void(BnplIssuer)> selected_issuer_callback),
              (override));
  MOCK_METHOD(void, SetBnplTosAcceptCallback, (base::OnceClosure), (override));
  MOCK_METHOD(void,
              OnBnplIssuerSuggestionSelected,
              (const std::string& issuer_id),
              (override));
  MOCK_METHOD(void, OnBnplTosAccepted, (), (override));

 private:
  std::unique_ptr<TouchToFillKeyboardSuppressor> suppressor_;
  base::WeakPtrFactory<MockTouchToFillDelegateAndroidImpl> weak_factory_{this};
};

class TestContentAutofillClientWithTouchToFillPaymentMethodController
    : public TestContentAutofillClient {
 public:
  using TestContentAutofillClient::TestContentAutofillClient;

  TouchToFillPaymentMethodControllerImpl& payment_method_controller() {
    return payment_method_controller_;
  }

 private:
  TouchToFillPaymentMethodControllerImpl payment_method_controller_{this};
};

class TouchToFillPaymentMethodControllerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  TouchToFillPaymentMethodControllerTest() = default;
  ~TouchToFillPaymentMethodControllerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("about:blank"));
    autofill_manager().set_touch_to_fill_delegate(
        std::make_unique<MockTouchToFillDelegateAndroidImpl>(
            &autofill_manager()));
    mock_view_ = std::make_unique<MockTouchToFillPaymentMethodViewImpl>();
  }

  void SetUpIbanFormField() {
    some_form_data_ = autofill::test::CreateTestIbanFormData();
    some_form_ = some_form_data_.global_id();
    some_field_ = test::MakeFieldGlobalId();
  }

  void SetUpLoyaltyCardFormField() {
    some_form_data_ = test::CreateTestLoyaltyCardFormData();
    some_form_ = some_form_data_.global_id();
    some_field_ = test::MakeFieldGlobalId();
  }

  void TearDown() override {
    mock_view_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestContentAutofillClientWithTouchToFillPaymentMethodController&
  autofill_client() {
    return *autofill_client_injector_[web_contents()];
  }

  TestBrowserAutofillManager& autofill_manager() {
    return *autofill_manager_injector_[web_contents()];
  }

  TouchToFillPaymentMethodControllerImpl& payment_method_controller() {
    return autofill_client().payment_method_controller();
  }

  MockTouchToFillDelegateAndroidImpl& ttf_delegate() {
    return *static_cast<MockTouchToFillDelegateAndroidImpl*>(
        autofill_manager().touch_to_fill_delegate());
  }

  const std::vector<CreditCard> credit_cards_ = {test::GetCreditCard(),
                                                 test::GetCreditCard2()};
  const std::vector<Iban> ibans_ = {test::GetLocalIban(),
                                    test::GetServerIban()};
  const std::vector<LoyaltyCard> all_loyalty_cards_ = {
      test::CreateLoyaltyCard(), test::CreateLoyaltyCard2()};
  const std::vector<LoyaltyCard> affiliated_loyalty_cards_ = {
      test::CreateLoyaltyCard()};
  const std::vector<Suggestion> suggestions_{
      test::CreateAutofillSuggestion(
          SuggestionType::kCreditCardEntry,
          credit_cards_[0].CardNameForAutofillDisplay(),
          credit_cards_[0].ObfuscatedNumberWithVisibleLastFourDigits(),
          /*has_deactivated_style=*/false),
      test::CreateAutofillSuggestion(
          SuggestionType::kCreditCardEntry,
          credit_cards_[1].CardNameForAutofillDisplay(),
          credit_cards_[1].ObfuscatedNumberWithVisibleLastFourDigits(),
          /*has_deactivated_style=*/false)};
  const std::vector<payments::BnplIssuerContext> bnpl_issuer_contexts_ = {
      payments::BnplIssuerContext(test::GetTestLinkedBnplIssuer(),
                                  payments::BnplIssuerEligibilityForPage::
                                      kNotEligibleIssuerDoesNotSupportMerchant),
      payments::BnplIssuerContext(
          test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplZip),
          payments::BnplIssuerEligibilityForPage::
              kNotEligibleCheckoutAmountTooLow),
      payments::BnplIssuerContext(
          test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplAfterpay),
          payments::BnplIssuerEligibilityForPage::
              kNotEligibleCheckoutAmountTooHigh)};
  std::unique_ptr<MockTouchToFillPaymentMethodViewImpl> mock_view_;

  void OnBeforeAskForValuesToFill() {
    EXPECT_CALL(ttf_delegate(), IsShowingTouchToFill).WillOnce(Return(false));
    EXPECT_CALL(ttf_delegate(), IntendsToShowTouchToFill)
        .WillOnce(Return(true));
    payment_method_controller()
        .keyboard_suppressor_for_test()
        .OnBeforeAskForValuesToFill(autofill_manager(), some_form_, some_field_,
                                    some_form_data_);
    EXPECT_TRUE(payment_method_controller()
                    .keyboard_suppressor_for_test()
                    .is_suppressing());
  }

  void OnAfterAskForValuesToFill() {
    EXPECT_TRUE(payment_method_controller()
                    .keyboard_suppressor_for_test()
                    .is_suppressing());
    EXPECT_CALL(ttf_delegate(), IsShowingTouchToFill).WillOnce(Return(true));
    payment_method_controller()
        .keyboard_suppressor_for_test()
        .OnAfterAskForValuesToFill(autofill_manager(), some_form_, some_field_);
    EXPECT_TRUE(payment_method_controller()
                    .keyboard_suppressor_for_test()
                    .is_suppressing());
  }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClientInjector<
      TestContentAutofillClientWithTouchToFillPaymentMethodController>
      autofill_client_injector_;
  TestAutofillManagerInjector<TestBrowserAutofillManager>
      autofill_manager_injector_;
  FormData some_form_data_ =
      autofill::test::CreateTestCreditCardFormData(/*is_https=*/true,
                                                   /*use_month_type=*/false);
  FormGlobalId some_form_ = some_form_data_.global_id();
  FieldGlobalId some_field_ = test::MakeFieldGlobalId();
};

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowPaymentMethodsPassesCreditCardsToTheView) {
  // Test that the cards have propagated to the view.
  EXPECT_CALL(*mock_view_,
              ShowPaymentMethods(&payment_method_controller(),
                                 ElementsAreArray(suggestions_),
                                 /*should_show_scan_credit_card=*/true));
  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest, ShowIbansPassesIbansToTheView) {
  SetUpIbanFormField();
  // Test that the IBANs have propagated to the view.
  EXPECT_CALL(*mock_view_, ShowIbans(&payment_method_controller(),
                                     ElementsAreArray(ibans_)));
  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowIbans(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), ibans_);
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowLoyaltyCardsPassesLoyaltyCardsToTheView) {
  SetUpLoyaltyCardFormField();
  // Test that the loyalty cards have propagated to the view.
  EXPECT_CALL(*mock_view_,
              ShowLoyaltyCards(&payment_method_controller(),
                               ElementsAreArray(affiliated_loyalty_cards_),
                               ElementsAreArray(all_loyalty_cards_),
                               /*first_time_usage*/ true));
  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowLoyaltyCards(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(),
      affiliated_loyalty_cards_, all_loyalty_cards_, /*first_time_usage*/ true);
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest, ScanCreditCardIsCalled) {
  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  OnAfterAskForValuesToFill();
  EXPECT_CALL(ttf_delegate(), ScanCreditCard);
  payment_method_controller().ScanCreditCard(nullptr);
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowPaymentMethodSettingsIsCalledForCards) {
  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  OnAfterAskForValuesToFill();
  EXPECT_CALL(ttf_delegate(), ShowPaymentMethodSettings);
  payment_method_controller().ShowPaymentMethodSettings(nullptr);
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowPaymentMethodSettingsIsCalledForIbans) {
  SetUpIbanFormField();
  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowIbans(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), ibans_);
  OnAfterAskForValuesToFill();
  EXPECT_CALL(ttf_delegate(), ShowPaymentMethodSettings);
  payment_method_controller().ShowPaymentMethodSettings(nullptr);
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       OnPurchaseAmountExtractedOnPreexistingView) {
  std::optional<int64_t> extracted_amount = 12345;
  std::optional<std::string> app_locale = "en-US";
  EXPECT_CALL(*mock_view_,
              ShowPaymentMethods(&payment_method_controller(),
                                 ElementsAreArray(suggestions_),
                                 /*should_show_scan_credit_card=*/true));
  EXPECT_CALL(*mock_view_,
              OnPurchaseAmountExtracted(
                  Ref(payment_method_controller()),
                  testing::ElementsAre(
                      EqualBnplIssuerContext(bnpl_issuer_contexts_[0]),
                      EqualBnplIssuerContext(bnpl_issuer_contexts_[1]),
                      EqualBnplIssuerContext(bnpl_issuer_contexts_[2])),
                  extracted_amount,
                  /*is_amount_supported_by_any_issuer=*/true, app_locale));

  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  payment_method_controller().OnPurchaseAmountExtracted(
      bnpl_issuer_contexts_, extracted_amount,
      /*is_amount_supported_by_any_issuer=*/true, app_locale,
      /*selected_issuer_callback=*/base::DoNothing(),
      /*cancel_callback=*/base::DoNothing());
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       OnPurchaseAmountExtractedAbortsIfNoViewAvailable) {
  EXPECT_CALL(*mock_view_, OnPurchaseAmountExtracted).Times(0);

  OnBeforeAskForValuesToFill();
  payment_method_controller().OnPurchaseAmountExtracted(
      bnpl_issuer_contexts_, /*extracted_amount=*/12345,
      /*is_amount_supported_by_any_issuer=*/true,
      /*app_locale=*/"en-US", /*selected_issuer_callback=*/base::DoNothing(),
      /*cancel_callback=*/base::DoNothing());
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowProgressScreenOnPreexistingView) {
  EXPECT_CALL(*mock_view_,
              ShowPaymentMethods(&payment_method_controller(),
                                 ElementsAreArray(suggestions_),
                                 /*should_show_scan_credit_card=*/true));
  EXPECT_CALL(*mock_view_, ShowProgressScreen(&payment_method_controller()))
      .WillOnce(Return(true));

  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  payment_method_controller().ShowProgressScreen(
      /*view=*/nullptr, /*cancel_callback=*/base::DoNothing());
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest, ShowProgressScreenOnNewView) {
  EXPECT_CALL(*mock_view_, ShowProgressScreen(&payment_method_controller()))
      .WillOnce(Return(true));

  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowProgressScreen(
      std::move(mock_view_), /*cancel_callback=*/base::DoNothing());
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowProgressScreenAbortsIfNoViewAvailable) {
  EXPECT_CALL(*mock_view_, ShowProgressScreen).Times(0);

  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowProgressScreen(
      /*view=*/nullptr, /*cancel_callback=*/base::DoNothing());
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowProgressScreenPrefersUsingNewViewOverPreexistingView) {
  std::unique_ptr<MockTouchToFillPaymentMethodViewImpl> new_mock_view =
      std::make_unique<MockTouchToFillPaymentMethodViewImpl>();

  EXPECT_CALL(*mock_view_,
              ShowPaymentMethods(&payment_method_controller(),
                                 ElementsAreArray(suggestions_),
                                 /*should_show_scan_credit_card=*/true));
  EXPECT_CALL(*mock_view_, ShowProgressScreen).Times(0);
  EXPECT_CALL(*new_mock_view, ShowProgressScreen(&payment_method_controller()))
      .WillOnce(Return(true));

  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  payment_method_controller().ShowProgressScreen(
      std::move(new_mock_view), /*cancel_callback=*/base::DoNothing());
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowBnplIssuersPassesContextsToTheView) {
  // Test that the BNPL issuer contexts have propagated to the view.
  ASSERT_EQ(3U, bnpl_issuer_contexts_.size());
  EXPECT_CALL(
      *mock_view_,
      ShowBnplIssuers(Ref(payment_method_controller()),
                      testing::ElementsAre(
                          EqualBnplIssuerContext(bnpl_issuer_contexts_[0]),
                          EqualBnplIssuerContext(bnpl_issuer_contexts_[1]),
                          EqualBnplIssuerContext(bnpl_issuer_contexts_[2])),
                      "en-US"));

  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  payment_method_controller().ShowBnplIssuers(
      bnpl_issuer_contexts_, /*app_locale=*/"en-US",
      /*selected_issuer_callback=*/base::DoNothing(),
      /*cancel_callback=*/base::DoNothing());
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowBnplIssuersOnPreexistingView) {
  base::MockOnceClosure mock_cancel_callback;
  base::MockOnceCallback<void(autofill::BnplIssuer)>
      mock_selected_issuer_callback;
  EXPECT_CALL(*mock_view_,
              ShowPaymentMethods(&payment_method_controller(),
                                 ElementsAreArray(suggestions_),
                                 /*should_show_scan_credit_card=*/true));
  EXPECT_CALL(*mock_view_,
              ShowBnplIssuers(Ref(payment_method_controller()),
                              ElementsAreArray(bnpl_issuer_contexts_), "en-US"))
      .WillOnce(Return(true));
  EXPECT_CALL(ttf_delegate(), SetCancelCallback);
  EXPECT_CALL(ttf_delegate(), SetSelectedIssuerCallback);

  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  payment_method_controller().ShowBnplIssuers(
      bnpl_issuer_contexts_, /*app_locale=*/"en-US",
      mock_selected_issuer_callback.Get(), mock_cancel_callback.Get());
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowBnplIssuersAbortsIfNoViewAvailable) {
  base::MockOnceClosure mock_cancel_callback;
  base::MockOnceCallback<void(autofill::BnplIssuer)>
      mock_selected_issuer_callback;
  EXPECT_CALL(*mock_view_, ShowBnplIssuers).Times(0);
  EXPECT_CALL(ttf_delegate(), SetCancelCallback).Times(0);
  EXPECT_CALL(ttf_delegate(), SetSelectedIssuerCallback).Times(0);

  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowBnplIssuers(
      bnpl_issuer_contexts_, /*app_locale=*/"en-US",
      mock_selected_issuer_callback.Get(), mock_cancel_callback.Get());
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowBnplIssuerTosPassesTextsAndIconsToTheView_LinkedIssuer) {
  BnplTosModel bnpl_tos_model;
  bnpl_tos_model.issuer =
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplAffirm);
  bnpl_tos_model.legal_message_lines = {
      TestLegalMessageLine("This is the entire message.")};

  // Test that the BNPL issuer ToS info have propagated to the view.
  EXPECT_CALL(*mock_view_,
              ShowBnplIssuerTos(
                  Ref(payment_method_controller()),
                  FieldsAre(BnplIssuer::IssuerId::kBnplAffirm,
                            IDR_AUTOFILL_GOOGLE_PAY_AFFIRM,
                            IDR_AUTOFILL_GOOGLE_PAY_AFFIRM_DARK, true,
                            u"Affirm", bnpl_tos_model.legal_message_lines)));
  EXPECT_CALL(ttf_delegate(), SetCancelCallback);
  EXPECT_CALL(ttf_delegate(), SetBnplTosAcceptCallback);

  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  OnAfterAskForValuesToFill();
  payment_method_controller().ShowBnplIssuerTos(
      bnpl_tos_model, /*accept_callback=*/base::DoNothing(),
      /*cancel_callback=*/base::DoNothing());
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowBnplIssuerTosPassesTextsAndIconsToTheView_UnlinkedIssuer) {
  BnplTosModel bnpl_tos_model;
  bnpl_tos_model.issuer = test::GetTestUnlinkedBnplIssuer();
  bnpl_tos_model.legal_message_lines = {
      TestLegalMessageLine("This is the entire message.")};
  bnpl_tos_model.issuer.set_issuer_id(BnplIssuer::IssuerId::kBnplZip);

  // Test that the BNPL issuer ToS info have propagated to the view.
  EXPECT_CALL(
      *mock_view_,
      ShowBnplIssuerTos(
          Ref(payment_method_controller()),
          FieldsAre(BnplIssuer::IssuerId::kBnplZip, IDR_AUTOFILL_GOOGLE_PAY_ZIP,
                    IDR_AUTOFILL_GOOGLE_PAY_ZIP_DARK, false, u"Zip",
                    bnpl_tos_model.legal_message_lines)));
  EXPECT_CALL(ttf_delegate(), SetCancelCallback);
  EXPECT_CALL(ttf_delegate(), SetBnplTosAcceptCallback);

  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  OnAfterAskForValuesToFill();
  payment_method_controller().ShowBnplIssuerTos(
      bnpl_tos_model, /*accept_callback=*/base::DoNothing(),
      /*cancel_callback=*/base::DoNothing());
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest, BnplSuggestionSelected) {
  std::optional<int64_t> extracted_amount = 12345;
  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  OnAfterAskForValuesToFill();

  EXPECT_CALL(ttf_delegate(), BnplSuggestionSelected(extracted_amount));
  payment_method_controller().BnplSuggestionSelected(/*JNIEnv*=*/nullptr,
                                                     extracted_amount);
}

TEST_F(TouchToFillPaymentMethodControllerTest, ShowErrorScreenOnNewView) {
  const std::u16string title = u"Error Title";
  const std::u16string description = u"Error Description";

  EXPECT_CALL(*mock_view_,
              ShowErrorScreen(&payment_method_controller(), title, description))
      .WillOnce(Return(true));

  OnBeforeAskForValuesToFill();
  EXPECT_TRUE(payment_method_controller().ShowErrorScreen(std::move(mock_view_),
                                                          title, description));
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowErrorScreenOnPreexistingView) {
  const std::u16string title = u"Error Title";
  const std::u16string description = u"Error Description";

  EXPECT_CALL(*mock_view_,
              ShowPaymentMethods(&payment_method_controller(),
                                 ElementsAreArray(suggestions_),
                                 /*should_show_scan_credit_card=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_view_,
              ShowErrorScreen(&payment_method_controller(), title, description))
      .WillOnce(Return(true));

  OnBeforeAskForValuesToFill();
  EXPECT_TRUE(payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_));
  EXPECT_TRUE(payment_method_controller().ShowErrorScreen(
      /*view=*/nullptr, title, description));
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowErrorScreenAbortsIfNoViewAvailable) {
  EXPECT_CALL(*mock_view_, ShowErrorScreen).Times(0);

  OnBeforeAskForValuesToFill();
  EXPECT_FALSE(payment_method_controller().ShowErrorScreen(
      /*view=*/nullptr, u"Error Title", u"Error Description"));
  OnAfterAskForValuesToFill();
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowErrorScreenPrefersUsingNewViewOverPreexistingView) {
  std::unique_ptr<MockTouchToFillPaymentMethodViewImpl> new_mock_view =
      std::make_unique<MockTouchToFillPaymentMethodViewImpl>();
  const std::u16string title = u"Error Title";
  const std::u16string description = u"Error Description";

  EXPECT_CALL(*mock_view_,
              ShowPaymentMethods(&payment_method_controller(),
                                 ElementsAreArray(suggestions_),
                                 /*should_show_scan_credit_card=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_view_, ShowErrorScreen).Times(0);
  EXPECT_CALL(*new_mock_view,
              ShowErrorScreen(&payment_method_controller(), title, description))
      .WillOnce(Return(true));

  OnBeforeAskForValuesToFill();
  EXPECT_TRUE(payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_));
  EXPECT_TRUE(payment_method_controller().ShowErrorScreen(
      std::move(new_mock_view), title, description));
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest, OnDismissedIsCalled) {
  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  OnAfterAskForValuesToFill();

  EXPECT_CALL(ttf_delegate(), OnDismissed);
  payment_method_controller().OnDismissed(nullptr, true, false);
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       OnDismissedPassesDismissedByUserToDelegate) {
  EXPECT_CALL(*mock_view_, ShowProgressScreen(&payment_method_controller()))
      .WillOnce(Return(true));
  EXPECT_CALL(ttf_delegate(), OnDismissed(/*dismissed_by_user=*/true,
                                          /*should_reshow=*/true));

  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  payment_method_controller().ShowProgressScreen(
      /*view=*/nullptr,
      /*cancel_callback=*/base::DoNothing());
  OnAfterAskForValuesToFill();

  payment_method_controller().OnDismissed(nullptr, /*dismissed_by_user=*/true,
                                          /*should_reshow=*/true);
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       OnDismissedPassesNotDismissedByUserToDelegate) {
  EXPECT_CALL(*mock_view_, ShowProgressScreen(&payment_method_controller()))
      .WillOnce(Return(true));
  EXPECT_CALL(ttf_delegate(), OnDismissed(/*dismissed_by_user=*/false,
                                          /*should_reshow=*/false));

  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  payment_method_controller().ShowProgressScreen(
      /*view=*/nullptr,
      /*cancel_callback=*/base::DoNothing());
  OnAfterAskForValuesToFill();

  payment_method_controller().OnDismissed(nullptr, /*dismissed_by_user=*/false,
                                          /*should_reshow=*/false);
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       OnBnplIssuerSuggestionSelected_ForwardsCallToDelegate) {
  EXPECT_CALL(*mock_view_,
              ShowBnplIssuers(Ref(payment_method_controller()),
                              ElementsAreArray(bnpl_issuer_contexts_), "en-US"))
      .WillOnce(Return(true));
  EXPECT_CALL(ttf_delegate(),
              OnBnplIssuerSuggestionSelected(/*issuer_id=*/"affirm"));

  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  payment_method_controller().ShowBnplIssuers(
      bnpl_issuer_contexts_,
      /*app_locale=*/"en-US",
      /*selected_issuer_callback=*/base::DoNothing(),
      /*cancel_callback=*/base::DoNothing());
  OnAfterAskForValuesToFill();

  payment_method_controller().OnBnplIssuerSuggestionSelected(
      nullptr, /*issuer_id=*/"affirm");
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       OnBnplTosAccepted_ForwardsCallToDelegate) {
  BnplTosModel bnpl_tos_model;
  bnpl_tos_model.issuer = test::GetTestUnlinkedBnplIssuer();
  bnpl_tos_model.legal_message_lines = {
      TestLegalMessageLine("This is the entire message.")};

  // Test that the BNPL issuer ToS info have propagated to the view.
  EXPECT_CALL(*mock_view_, ShowBnplIssuerTos).WillOnce(Return(true));
  EXPECT_CALL(ttf_delegate(), OnBnplTosAccepted());

  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  OnAfterAskForValuesToFill();
  payment_method_controller().ShowBnplIssuerTos(
      bnpl_tos_model, /*accept_callback=*/base::DoNothing(),
      /*cancel_callback=*/base::DoNothing());
  OnAfterAskForValuesToFill();

  payment_method_controller().OnBnplTosAccepted(nullptr);
}

TEST_F(TouchToFillPaymentMethodControllerTest, SetVisibleHidesSheet) {
  EXPECT_CALL(*mock_view_, SetVisible(false));

  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  OnAfterAskForValuesToFill();

  payment_method_controller().SetVisible(false);
}

TEST_F(TouchToFillPaymentMethodControllerTest, SetVisibleShowsSheet) {
  EXPECT_CALL(*mock_view_, SetVisible(true));

  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowPaymentMethods(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  OnAfterAskForValuesToFill();

  payment_method_controller().SetVisible(true);
}

}  // namespace
}  // namespace autofill

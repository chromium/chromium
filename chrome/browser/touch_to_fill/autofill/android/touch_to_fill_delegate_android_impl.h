// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_DELEGATE_ANDROID_IMPL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_DELEGATE_ANDROID_IMPL_H_

#include <optional>
#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/integrators/fast_checkout/fast_checkout_client.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_delegate.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// Enum that describes different outcomes to an attempt of triggering the
// Touch To Fill bottom sheet for credit cards or IBANs.
// The enum values are not exhaustive to avoid excessive metric collection.
// The cases where TTF is not shown because of other form type (not payment
// method) or TTF being not supported are skipped.
// Do not remove or renumber entries in this enum. It needs to be kept in
// sync with the enum of the same name in `enums.xml`.
enum class TouchToFillPaymentMethodTriggerOutcome {
  // The sheet was shown.
  kShown = 0,
  // The sheet was not shown because the clicked field was not focusable or
  // already had a value.
  kFieldNotEmptyOrNotFocusable = 1,
  // The sheet was not shown because there were no valid credit cards or IBANs
  // to suggest.
  kNoValidPaymentMethods = 2,
  // The sheet was not shown because either the client or the form was not
  // secure.
  kFormOrClientNotSecure = 3,
  // The sheet was not shown because it has already been shown before and should
  // not be shown again.
  kShownBeforeAndShouldNotBeShownAgain = 4,
  // The sheet was not shown because Autofill UI cannot be shown.
  kCannotShowAutofillUi = 5,
  // There was a try to display the bottom sheet, but it failed due to unknown
  // reason.
  // A particular potential reason is that the keyboard is not suppressed
  // (anymore). This can happen due to race conditions involving the form parse
  // that may happen between
  // OnBeforeAskForValuesToFill() and TryToShowTouchToFill(). In particular, the
  // focused field's type may change (changing DryRun()'s value from a
  // non-`kShown` value to `kShown`).
  kFailedToDisplayBottomSheet = 6,
  // The sheet was not shown because the payment form was incomplete.
  kIncompleteForm = 7,
  // The form or field is not known to the form cache.
  kUnknownForm = 8,
  // The form is known to the form cache, but it doesn't contain the field.
  kUnknownField = 9,
  // TouchToFill is not supported for this field type. This value is not logged
  // to UMA.
  kUnsupportedFieldType = 10,
  // Fast Checkout was shown before TouchToFill could be triggered.
  kFastCheckoutWasShown = 11,
  // Form is considered to be already filled if fields of payment method info
  // already have non-empty values.
  kFormAlreadyFilled = 12,
  kMaxValue = kFormAlreadyFilled
};

inline constexpr const char kUmaTouchToFillCreditCardTriggerOutcome[] =
    "Autofill.TouchToFill.CreditCard.TriggerOutcome";
inline constexpr const char kUmaTouchToFillIbanTriggerOutcome[] =
    "Autofill.TouchToFill.Iban.TriggerOutcome";
inline constexpr const char kUmaTouchToFillLoyaltyCardTriggerOutcome[] =
    "Autofill.TouchToFill.LoyaltyCard.TriggerOutcome";

class BrowserAutofillManager;
class FormStructure;

// Delegate for in-browser Touch To Fill (TTF) surface display and selection.
//
// If the surface was shown once, it won't be triggered again on the same page.
// But calling |Reset()| on navigation restores such showing eligibility.
//
// Due to asynchronous parsing, showing the TTF surface proceeds in two stages:
// IntendsToShowTouchToFill() is called before parsing, and only if this returns
// true, TryToShowTouchToFill() is called after parsing. This is necessary for
// the keyboard suppression mechanism to work; see
// `TouchToFillKeyboardSuppressor` for details.
//
// It is supposed to be owned by the given |BrowserAutofillManager|, and
// interact with it and its |AutofillClient| and |AutofillDriver|.
//
// TODO(crbug.com/40839529): Consider using more descriptive name.
class TouchToFillDelegateAndroidImpl : public TouchToFillDelegate {
 public:
  explicit TouchToFillDelegateAndroidImpl(BrowserAutofillManager* manager);
  TouchToFillDelegateAndroidImpl(const TouchToFillDelegateAndroidImpl&) =
      delete;
  TouchToFillDelegateAndroidImpl& operator=(
      const TouchToFillDelegateAndroidImpl&) = delete;
  ~TouchToFillDelegateAndroidImpl() override;

  // Checks whether TTF is eligible for the given web form data.
  // Only if this is true, the controller will show the view.
  bool IntendsToShowTouchToFill(FormGlobalId form_id,
                                FieldGlobalId field_id) override;

  // Checks whether TTF is eligible for the given web form data and, if
  // successful, triggers the corresponding surface and returns |true|.
  bool TryToShowTouchToFill(const FormData& form,
                            const FormFieldData& field) override;

  // Returns whether the TTF surface is currently being shown.
  bool IsShowingTouchToFill() override;

  // Hides the TTF surface if one is shown.
  void HideTouchToFill() override;

  // Resets the delegate to its starting state (e.g. on navigation).
  void Reset() override;

  // TouchToFillDelegate:
  bool ShouldShowScanCreditCard() override;
  void ScanCreditCard() override;
  void OnCreditCardScanned(const CreditCard& card) override;
  void ShowPaymentMethodSettings() override;
  void CreditCardSuggestionSelected(std::string unique_id,
                                    bool is_virtual) override;
  void BnplSuggestionSelected(std::optional<int64_t> extracted_amount) override;
  void OnBnplTosAccepted() override;
  void IbanSuggestionSelected(
      std::variant<Iban::Guid, Iban::InstrumentId> backend_id) override;
  void LoyaltyCardSuggestionSelected(const LoyaltyCard& loyalty_card) override;
  void OnDismissed(bool dismissed_by_user, bool should_reshow) override;
  void OnBnplIssuerSuggestionSelected(const std::string& issuer_id) override;

  void LogMetricsAfterSubmission(const FormStructure& submitted_form) override;

  void SetCancelCallback(base::OnceClosure cancel_callback) override;
  void SetSelectedIssuerCallback(
      base::OnceCallback<void(BnplIssuer)> selected_issuer_callback) override;
  void SetBnplTosAcceptCallback(base::OnceClosure accept_tos_callback) override;

  base::WeakPtr<TouchToFillDelegateAndroidImpl> GetWeakPtr();

 private:
  enum class TouchToFillState {
    kShouldShow,
    kIsShowing,
    kShownAndShouldBeShownAgain,
    kShownAndShouldNotBeShownAgain
  };

  using TriggerOutcome = TouchToFillPaymentMethodTriggerOutcome;

  struct DryRunResult {
    DryRunResult(TriggerOutcome outcome,
                 std::variant<std::vector<CreditCard>,
                              std::vector<Iban>,
                              std::vector<LoyaltyCard>> items_to_suggest);
    DryRunResult(DryRunResult&&);
    DryRunResult& operator=(DryRunResult&&);
    ~DryRunResult();

    TriggerOutcome outcome;
    std::variant<std::vector<CreditCard>,
                 std::vector<Iban>,
                 std::vector<LoyaltyCard>>
        items_to_suggest;
  };

  // This groups the callbacks needed to handle user actions in the BNPL flow.
  // When a bottom sheet is shown for the Issuer Selection, Terms of Service,
  // or Progress screen, these callbacks are set here and are then triggered
  // when the user interacts with that screen.
  struct BnplCallbacks {
    BnplCallbacks();
    BnplCallbacks(BnplCallbacks&&);
    BnplCallbacks& operator=(BnplCallbacks&&);
    ~BnplCallbacks();

    // This callback is set when the issuer selection screen is shown,
    // and it runs when the user selects a BNPL issuer.
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback;
    // This callback is set when the issuer ToS screen is shown, and it runs
    // when the user accepts the ToS.
    base::OnceClosure accept_tos_callback;
    // This callback runs when the user dismisses the bottom sheet. It is set
    // for multiple screens, including the Issuer Selection, Terms of Service,
    // and Progress screens.
    base::OnceClosure cancel_callback;
  };

  // Checks all preconditions for showing the TTF, that is, for calling
  // PaymentsAutofillClient::ShowTouchToFillCreditCard().
  //
  // If the DryRunResult::outcome is TriggerOutcome::kShown,
  // DryRunResult::items_to_suggest is populated; otherwise it is empty.
  DryRunResult DryRun(FormGlobalId form_id, FieldGlobalId field_id);

  // Returns a DryRunResult with the user's fillable IBANs, or
  // `kNoValidPaymentMethods` if no IBANs are available.
  DryRunResult DryRunForIban();

  // Returns a DryRunResult with the user's fillable credit cards, or
  // an error reason if TTF should not be triggered.
  DryRunResult DryRunForCreditCard(const AutofillField& field,
                                   const FormStructure& form);

  // Returns a DryRunResult with the user's fillable loyalty cards, or
  // an error reason if TTF should not be triggered.
  DryRunResult DryRunForLoyaltyCard();

  // Creates a list of booleans which denotes if credit cards are acceptable by
  // the merchant. The returned list has the same size as `credit_cards`, and
  // the indices match (the acceptability of credit_cards[i] ==
  // card_acceptability[i]).
  std::vector<bool> GetCardAcceptabilities(
      base::span<const CreditCard> credit_cards);

  void LogTriggerOutcomeMetrics(const FormGlobalId& form_id,
                                const FieldGlobalId& field_id,
                                TriggerOutcome outcome);

  TouchToFillState ttf_payment_method_state_ = TouchToFillState::kShouldShow;

  const raw_ref<BrowserAutofillManager> manager_;
  FormData query_form_;
  FormFieldData query_field_;
  bool dismissed_by_user_ = false;

  BnplCallbacks bnpl_callbacks_;

  base::WeakPtrFactory<TouchToFillDelegateAndroidImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_DELEGATE_ANDROID_IMPL_H_

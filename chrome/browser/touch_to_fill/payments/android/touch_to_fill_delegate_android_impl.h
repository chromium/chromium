// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_DELEGATE_ANDROID_IMPL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_DELEGATE_ANDROID_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/ui/fast_checkout_client.h"
#include "components/autofill/core/browser/ui/touch_to_fill_delegate.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// Enum that describes different outcomes to an attempt of triggering the
// Touch To Fill bottom sheet for credit cards.
// The enum values are not exhaustive to avoid excessive metric collection.
// The cases where TTF is not shown because of other form type (not credit card)
// or TTF being not supported are skipped.
// Do not remove or renumber entries in this enum. It needs to be kept in
// sync with the enum of the same name in `enums.xml`.
enum class TouchToFillCreditCardTriggerOutcome {
  // The sheet was shown.
  kShown = 0,
  // The sheet was not shown because the clicked field was not focusable or
  // already had a value.
  kFieldNotEmptyOrNotFocusable = 1,
  // The sheet was not shown because there were no valid credit cards to
  // suggest.
  kNoValidCards = 2,
  // The sheet was not shown because either the client or the form was not
  // secure.
  kFormOrClientNotSecure = 3,
  // The sheet was not shown because it has already been shown before.
  kShownBefore = 4,
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
  // Form is considered to be already filled if the credit card number or expiry
  // date already have non-empty values.
  kFormAlreadyFilled = 12,
  kMaxValue = kFormAlreadyFilled
};

constexpr const char kUmaTouchToFillCreditCardTriggerOutcome[] =
    "Autofill.TouchToFill.CreditCard.TriggerOutcome";

class BrowserAutofillManager;
class FormStructure;

// Delegate for in-browser Touch To Fill (TTF) surface display and selection.
// Currently TTF surface is eligible only for credit card forms on click on
// an empty focusable field.
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
// TODO(crbug.com/1324900): Consider using more descriptive name.
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
  AutofillManager* GetManager() override;
  bool ShouldShowScanCreditCard() override;
  void ScanCreditCard() override;
  void OnCreditCardScanned(const CreditCard& card) override;
  void ShowCreditCardSettings() override;
  void SuggestionSelected(std::string unique_id, bool is_virtual) override;
  void OnDismissed(bool dismissed_by_user) override;

  void LogMetricsAfterSubmission(const FormStructure& submitted_form) override;

  base::WeakPtr<TouchToFillDelegateAndroidImpl> GetWeakPtr();

 private:
  enum class TouchToFillState {
    kShouldShow,
    kIsShowing,
    kWasShown,
  };

  using TriggerOutcome = TouchToFillCreditCardTriggerOutcome;

  struct DryRunResult {
    DryRunResult(TriggerOutcome outcome,
                 std::vector<CreditCard> cards_to_suggest);
    DryRunResult(DryRunResult&&);
    DryRunResult& operator=(DryRunResult&&);
    ~DryRunResult();

    TriggerOutcome outcome;
    std::vector<CreditCard> cards_to_suggest;
  };

  // Checks all preconditions for showing the TTF, that is, for calling
  // AutofillClient::ShowTouchToFillCreditCard().
  //
  // If the DryRunResult::outcome is TriggerOutcome::kShow, the
  // DryRun::cards_to_suggest contains the cards; otherwise it is empty.
  // TODO(crbug.com/1331312): Remove the optional_received_form. The
  // implementation currently fetches the FormStructure corresponding to
  // form_id. The fields' values of this form structure correspond to the
  // initial and so probably stale values. optional_received_form is the form
  // received from the renderer, so it contains the current values. This is
  // needed for the non-empty checks.
  DryRunResult DryRun(FormGlobalId form_id,
                      FieldGlobalId field_id,
                      const FormData* optional_received_form = nullptr);

  bool HasAnyAutofilledFields(const FormStructure& submitted_form) const;

  // The form is considered perfectly filled if all non-empty fields are
  // autofilled without further edits.
  bool IsFillingPerfect(const FormStructure& submitted_form) const;

  // The form is considered correctly filled if all autofilled fields were not
  // edited by user afterwards.
  bool IsFillingCorrect(const FormStructure& submitted_form) const;

  // Checks if the credit card form is already filled with values. The form is
  // considered to be filled if the credit card number field is non-empty. The
  // expiration date fields are not checked because they might have arbitrary
  // placeholders.
  // TODO(crbug.com/1331312): FormData is used here to ensure that we check the
  // most recent form values. FormStructure knows only about the initial values.
  bool IsFormPrefilled(const FormData& form);

  TouchToFillState ttf_credit_card_state_ = TouchToFillState::kShouldShow;

  const raw_ptr<BrowserAutofillManager> manager_;
  FormData query_form_;
  FormFieldData query_field_;
  bool dismissed_by_user_;

  base::WeakPtrFactory<TouchToFillDelegateAndroidImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_DELEGATE_ANDROID_IMPL_H_

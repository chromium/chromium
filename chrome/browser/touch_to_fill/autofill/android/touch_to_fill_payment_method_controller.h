// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view_controller.h"

namespace autofill {

namespace payments {
struct BnplIssuerContext;
}  // namespace payments

class BnplIssuer;
struct BnplTosModel;
class ContentAutofillClient;
class Iban;
class LoyaltyCard;
struct Suggestion;
class TouchToFillDelegate;
class TouchToFillPaymentMethodView;

// Controller of the bottom sheet surface for filling credit card, IBAN or
// loyalty card data on Android. It is responsible for showing the view and
// handling user interactions.
class TouchToFillPaymentMethodController
    : public TouchToFillPaymentMethodViewController {
 public:
  ~TouchToFillPaymentMethodController() override = default;

  // Shows the Touch To Fill `view`. `delegate` will provide the fillable credit
  // cards and be notified of the user's decision. `suggestions` are generated
  // using the `cards_to_suggest` data and include fields such as `main_text`,
  // `minor_text`, and `apply_deactivated_style`. The `apply_deactivated_style`
  // field determines which card suggestions should be disabled and grayed out
  // for the current merchant. Returns whether the surface was successfully
  // shown.
  virtual bool ShowPaymentMethods(
      std::unique_ptr<TouchToFillPaymentMethodView> view,
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const Suggestion> suggestions) = 0;

  // Shows the Touch To Fill `view`. `delegate` will provide the fillable IBANs
  // and be notified of the user's decision. Returns whether the surface was
  // successfully shown.
  virtual bool ShowIbans(std::unique_ptr<TouchToFillPaymentMethodView> view,
                         base::WeakPtr<TouchToFillDelegate> delegate,
                         base::span<const Iban> ibans_to_suggest) = 0;

  // Shows the Touch To Fill `view`. `delegate` will provide the fillable
  // loyalty cards and be notified of the user's decision. `first_time_usage` is
  // true if the user has never seen the loyalty card IPH or the Touch To Fill
  // view before. Returns whether the surface was successfully shown.
  virtual bool ShowLoyaltyCards(
      std::unique_ptr<TouchToFillPaymentMethodView> view,
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const LoyaltyCard> affiliated_loyalty_cards,
      base::span<const LoyaltyCard> all_loyalty_cards,
      bool first_time_usage) = 0;

  // If the user is on the credit card suggestion screen and amount extraction
  // is completed, this updates the BNPL payment method option on the Touch To
  // Fill suggestion view. If the `extracted_amount` is null, the option is
  // grayed out and its message text is updated to inform users that the
  // purchase is not available. If the amount is present but not supported by
  // any issuer, the UI is updated with a grayed out BNPL option. If the amount
  // is available and supported by at least one issuer, it is set to continue
  // the flow. If the user clicks on the BNPL suggestion before amount
  // extraction is completed, the user is shown the progress screen. If amount
  // extraction then returns with a valid amount, the progress screen is
  // updated with the issuer selection screen. If the amount is not null but
  // not supported by any issuer, the selection screen is shown with grayed out
  // issuers, and when the user returns to the home screen, the BNPL suggestion
  // is grayed out as well. If the amount is null, an error screen is shown.
  virtual bool OnPurchaseAmountExtracted(
      base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
      std::optional<int64_t> extracted_amount,
      bool is_amount_supported_by_any_issuer,
      const std::optional<std::string>& app_locale,
      base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
      base::OnceClosure cancel_callback) = 0;

  // Shows the Touch To Fill progress screen. If the TTF surface is already
  // being shown when this is called, `view` is optional and will override the
  // existing view when present. Otherwise, if the TTF surface is not already
  // being shown, `view` is required. `cancel_callback` will be run if the
  // screen is dismissed by the user. Returns whether the surface was
  // successfully shown.
  virtual bool ShowProgressScreen(
      std::unique_ptr<TouchToFillPaymentMethodView> view,
      base::OnceClosure cancel_callback) = 0;

  // Shows the Touch To Fill BNPL issuer selection screen.
  // `bnpl_issuer_contexts` provides a read-only list of BNPL issuer contexts
  // to be shown. `app_locale` provides the application's current language and
  // region code for localization. `selected_issuer_callback` provides a
  // one-time callback to be invoked when an issuer is selected.
  // `cancel_callback` provides a one-time callback to be invoked to reset the
  // BNPL flow. Returns whether the surface was successfully shown.
  virtual bool ShowBnplIssuers(
      base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
      const std::string& app_locale,
      base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
      base::OnceClosure cancel_callback) = 0;

  // Shows the Touch To Fill error screen. If the TTF surface is already being
  // shown when this is called, `view` is optional and will override the
  // existing view when present. Otherwise, if the TTF surface is not already
  // being shown, `view` is required. `title` and `description` are displayed on
  // the screen. Returns whether the surface was successfully shown.
  virtual bool ShowErrorScreen(
      std::unique_ptr<TouchToFillPaymentMethodView> view,
      const std::u16string& title,
      const std::u16string& description) = 0;

  // Shows the Touch To Fill BNPL issuer Terms of Service screen. Returns
  // whether the surface was successfully shown.
  virtual bool ShowBnplIssuerTos(BnplTosModel bnpl_tos_model,
                                 base::OnceClosure accept_callback,
                                 base::OnceClosure cancel_callback) = 0;

  // Hides the surface if it is currently shown.
  virtual void Hide() = 0;

  // Sets the surface visibility to `visible`.
  virtual void SetVisible(bool visible) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_H_

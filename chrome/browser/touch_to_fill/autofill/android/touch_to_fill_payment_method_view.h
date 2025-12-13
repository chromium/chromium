// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_VIEW_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_VIEW_H_

#include <string>

#include "base/containers/span.h"

namespace autofill {

namespace payments {
struct BnplIssuerContext;
struct BnplIssuerTosDetail;
}  // namespace payments

class Iban;
class LoyaltyCard;
struct Suggestion;
class TouchToFillPaymentMethodViewController;

// The UI interface which prompts the user to select a credit card to fill
// using Touch To Fill surface.
class TouchToFillPaymentMethodView {
 public:
  virtual ~TouchToFillPaymentMethodView() = default;

  virtual bool ShowPaymentMethods(
      TouchToFillPaymentMethodViewController* controller,
      base::span<const Suggestion> suggestions,
      bool should_show_scan_credit_card) = 0;
  virtual bool ShowIbans(TouchToFillPaymentMethodViewController* controller,
                         base::span<const Iban> ibans_to_suggest) = 0;
  virtual bool ShowLoyaltyCards(
      TouchToFillPaymentMethodViewController* controller,
      base::span<const LoyaltyCard> affiliated_loyalty_cards,
      base::span<const LoyaltyCard> all_loyalty_cards,
      bool first_time_usage) = 0;
  virtual bool OnPurchaseAmountExtracted(
      const TouchToFillPaymentMethodViewController& controller,
      base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
      std::optional<int64_t> extracted_amount,
      bool is_amount_supported_by_any_issuer,
      const std::optional<std::string>& app_locale);
  virtual bool ShowProgressScreen(
      TouchToFillPaymentMethodViewController* controller) = 0;
  virtual bool ShowBnplIssuers(
      const TouchToFillPaymentMethodViewController& controller,
      base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
      const std::string& app_locale) = 0;
  virtual bool ShowErrorScreen(
      TouchToFillPaymentMethodViewController* controller,
      const std::u16string& title,
      const std::u16string& description) = 0;
  virtual bool ShowBnplIssuerTos(
      const TouchToFillPaymentMethodViewController& controller,
      const payments::BnplIssuerTosDetail& bnpl_issuer_tos_detail) = 0;
  virtual void Hide() = 0;
  virtual void SetVisible(bool visible) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_VIEW_H_

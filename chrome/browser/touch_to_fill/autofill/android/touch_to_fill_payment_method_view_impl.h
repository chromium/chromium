// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_VIEW_IMPL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_VIEW_IMPL_H_

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view.h"

namespace content {
class WebContents;
}

namespace autofill {

namespace payments {
struct BnplIssuerContext;
struct BnplIssuerTosDetail;
}  // namespace payments

class Iban;
class LoyaltyCard;
struct Suggestion;
class TouchToFillPaymentMethodViewController;

// Android implementation of the surface to select a credit card or IBAN to fill.
// Uses Java TouchToFillPaymentMethodComponent to present a bottom sheet.
class TouchToFillPaymentMethodViewImpl : public TouchToFillPaymentMethodView {
 public:
  explicit TouchToFillPaymentMethodViewImpl(content::WebContents* web_contents);
  TouchToFillPaymentMethodViewImpl(const TouchToFillPaymentMethodViewImpl&) = delete;
  TouchToFillPaymentMethodViewImpl& operator=(
      const TouchToFillPaymentMethodViewImpl&) = delete;
  ~TouchToFillPaymentMethodViewImpl() override;

 private:
  // Returns true if `env`, `java_object_` and `controller` are in a state
  // where TTF view can be rendered.
  bool IsReadyToShow(TouchToFillPaymentMethodViewController* controller,
                     JNIEnv* env);
  // TouchToFillPaymentMethodView:
  bool ShowPaymentMethods(TouchToFillPaymentMethodViewController* controller,
                          base::span<const Suggestion> suggestions,
                          bool should_show_scan_credit_card) override;
  bool ShowIbans(TouchToFillPaymentMethodViewController* controller,
                 base::span<const autofill::Iban> ibans_to_suggest) override;
  bool ShowLoyaltyCards(TouchToFillPaymentMethodViewController* controller,
                        base::span<const LoyaltyCard> affiliated_loyalty_cards,
                        base::span<const LoyaltyCard> all_loyalty_cards,
                        bool first_time_usage) override;
  bool OnPurchaseAmountExtracted(
      const TouchToFillPaymentMethodViewController& controller,
      base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
      std::optional<int64_t> extracted_amount,
      bool is_amount_supported_by_any_issuer,
      const std::optional<std::string>& app_locale) override;
  bool ShowProgressScreen(
      TouchToFillPaymentMethodViewController* controller) override;
  bool ShowBnplIssuers(
      const TouchToFillPaymentMethodViewController& controller,
      base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
      const std::string& app_locale) override;
  bool ShowErrorScreen(TouchToFillPaymentMethodViewController* controller,
                       const std::u16string& title,
                       const std::u16string& description) override;
  bool ShowBnplIssuerTos(
      const TouchToFillPaymentMethodViewController& controller,
      const payments::BnplIssuerTosDetail& bnpl_issuer_tos_detail) override;
  void Hide() override;
  void SetVisible(bool visible) override;

  // The corresponding Java TouchToFillPaymentMethodViewBridge.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_VIEW_IMPL_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_DELEGATE_IMPL_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_DELEGATE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/ui/fast_checkout_client.h"
#include "components/autofill/core/browser/ui/fast_checkout_delegate.h"
#include "content/public/browser/web_contents.h"

namespace autofill {
class BrowserAutofillManager;
}  // namespace autofill

// Delegate for in-browser Fast Checkout (FC) surface display.
// Currently FC surface is eligible only for a particular set of domains on
// click on an empty focusable field during checkout flows.
//
// If the surface was shown once, it won't be triggered again on the same page.
//
// It is supposed to be owned by the given |BrowserAutofillManager|, and
// interact with it and its |FastCheckoutClient|.
//
// Due to asynchronous parsing, showing the FC surface proceeds in two stages:
// IntendsToShowFastCheckout() is called before parsing, and only if this
// returns true, TryToShowFastCheckout() is called after parsing. This is
// necessary for the keyboard suppression mechanism to work; see
// `TouchToFillKeyboardSuppressor` for details.
class FastCheckoutDelegateImpl : public autofill::FastCheckoutDelegate {
 public:
  FastCheckoutDelegateImpl(content::WebContents* web_contents,
                           autofill::FastCheckoutClient* client,
                           autofill::BrowserAutofillManager* manager);
  FastCheckoutDelegateImpl(const FastCheckoutDelegateImpl&) = delete;
  FastCheckoutDelegateImpl& operator=(const FastCheckoutDelegateImpl&) = delete;
  ~FastCheckoutDelegateImpl() override;

  // FastCheckoutDelegate:
  bool TryToShowFastCheckout(
      const autofill::FormData& form,
      const autofill::FormFieldData& field,
      base::WeakPtr<autofill::AutofillManager> autofill_manager) override;
  bool IntendsToShowFastCheckout(
      autofill::AutofillManager& manager,
      autofill::FormGlobalId form_id,
      autofill::FieldGlobalId field_id,
      const autofill::FormData& form_data) const override;
  bool IsShowingFastCheckoutUI() const override;
  void HideFastCheckout(bool allow_further_runs) override;

 private:
  // The WebContents.
  raw_ptr<content::WebContents> web_contents_;

  const raw_ptr<autofill::FastCheckoutClient> client_;
  const raw_ptr<autofill::BrowserAutofillManager> manager_;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_DELEGATE_IMPL_H_

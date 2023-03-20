// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_TRIGGER_VALIDATOR_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_TRIGGER_VALIDATOR_H_

#include "chrome/browser/fast_checkout/fast_checkout_enums.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"

constexpr char kUmaKeyFastCheckoutTriggerOutcome[] =
    "Autofill.FastCheckout.TriggerOutcome";

// Checks whether a Fast Checkout run should be permitted or not.
class FastCheckoutTriggerValidator {
 public:
  virtual ~FastCheckoutTriggerValidator() = default;

  FastCheckoutTriggerValidator(const FastCheckoutTriggerValidator&) = delete;
  FastCheckoutTriggerValidator& operator=(const FastCheckoutTriggerValidator&) =
      delete;

  // Returns `true` if a Fast Checkout run should be permitted, `false`
  // otherwise. Logs outcome to UMA and chrome://autofill-internals.
  virtual bool ShouldRun(
      const autofill::FormData& form,
      const autofill::FormFieldData& field,
      const FastCheckoutUIState ui_state,
      const bool is_running,
      const autofill::AutofillManager& autofill_manager) const = 0;

  // Returns `true` if the current profile has Autofill data enabled and at
  // least one valid Autofill profile and credit card stored, `false` otherwise.
  virtual bool HasValidPersonalData() const = 0;

 protected:
  FastCheckoutTriggerValidator() = default;
};

#endif

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_TRIGGER_VALIDATOR_IMPL_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_TRIGGER_VALIDATOR_IMPL_H_

#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher.h"
#include "chrome/browser/fast_checkout/fast_checkout_personal_data_helper.h"
#include "chrome/browser/fast_checkout/fast_checkout_trigger_validator.h"
#include "components/autofill/core/browser/ui/fast_checkout_enums.h"

class FastCheckoutTriggerValidatorImpl : public FastCheckoutTriggerValidator {
 public:
  FastCheckoutTriggerValidatorImpl(
      autofill::AutofillClient* autofill_client,
      FastCheckoutCapabilitiesFetcher* capabilities_fetcher,
      FastCheckoutPersonalDataHelper* personal_data_helper);
  ~FastCheckoutTriggerValidatorImpl() override = default;

  FastCheckoutTriggerValidatorImpl(const FastCheckoutTriggerValidatorImpl&) =
      delete;
  FastCheckoutTriggerValidatorImpl& operator=(
      const FastCheckoutTriggerValidatorImpl&) = delete;

  // FastCheckoutTriggerValidator:
  autofill::FastCheckoutTriggerOutcome ShouldRun(
      const autofill::FormData& form,
      const autofill::FormFieldData& field,
      const autofill::FastCheckoutUIState ui_state,
      const bool is_running,
      const autofill::AutofillManager& autofill_manager) const override;
  autofill::FastCheckoutTriggerOutcome HasValidPersonalData() const override;

 private:
  bool IsTriggerForm(const autofill::FormData& form,
                     const autofill::FormFieldData& field) const;
  void LogAutofillInternals(std::string message) const;

  raw_ptr<autofill::AutofillClient> autofill_client_;
  raw_ptr<FastCheckoutCapabilitiesFetcher> capabilities_fetcher_;
  raw_ptr<FastCheckoutPersonalDataHelper> personal_data_helper_;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_TRIGGER_VALIDATOR_IMPL_H_

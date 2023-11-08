// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_delegate_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/fast_checkout/fast_checkout_trigger_validator.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/ui/fast_checkout_enums.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/logging/log_macros.h"

FastCheckoutDelegateImpl::FastCheckoutDelegateImpl(
    content::WebContents* web_contents,
    autofill::FastCheckoutClient* client,
    autofill::BrowserAutofillManager* manager)
    : web_contents_(web_contents), client_(client), manager_(manager) {
  DCHECK(client_);
  DCHECK(manager_);
}

FastCheckoutDelegateImpl::~FastCheckoutDelegateImpl() = default;

bool FastCheckoutDelegateImpl::TryToShowFastCheckout(
    const autofill::FormData& form,
    const autofill::FormFieldData& field,
    base::WeakPtr<autofill::AutofillManager> autofill_manager) {
  const GURL& url = web_contents_->GetLastCommittedURL();
  return client_->TryToStart(url, form, field, autofill_manager);
}

bool FastCheckoutDelegateImpl::IntendsToShowFastCheckout(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form_id,
    autofill::FieldGlobalId field_id,
    const autofill::FormData& form_data) const {
  if (const autofill::FormStructure* form =
          manager_->FindCachedFormById(form_id)) {
    if (const autofill::AutofillField* field = form->GetFieldById(field_id)) {
      autofill::FastCheckoutTriggerOutcome trigger_outcome =
          client_->CanRun(form->ToFormData(), *field, manager);
      if (trigger_outcome !=
          autofill::FastCheckoutTriggerOutcome::kUnsupportedFieldType) {
        base::UmaHistogramEnumeration(kUmaKeyFastCheckoutTriggerOutcome,
                                      trigger_outcome);
      }
      return trigger_outcome == autofill::FastCheckoutTriggerOutcome::kSuccess;
    }
  }
  return false;
}

bool FastCheckoutDelegateImpl::IsShowingFastCheckoutUI() const {
  return client_->IsShowing();
}

void FastCheckoutDelegateImpl::HideFastCheckout(bool allow_further_runs) {
  if (IsShowingFastCheckoutUI()) {
    client_->Stop(/*allow_further_runs=*/allow_further_runs);
  }
}

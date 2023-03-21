// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/touch_to_fill/touch_to_fill_keyboard_suppressor.h"

#include "components/autofill/content/browser/content_autofill_client.h"

namespace autofill {

TouchToFillKeyboardSuppressor::TouchToFillKeyboardSuppressor(
    ContentAutofillClient* autofill_client,
    base::RepeatingCallback<bool(AutofillManager&)> is_showing,
    base::RepeatingCallback<bool(AutofillManager&, FormGlobalId, FieldGlobalId)>
        intends_to_show,
    base::TimeDelta timeout)
    : is_showing_(std::move(is_showing)),
      intends_to_show_(std::move(intends_to_show)),
      timeout_(timeout) {
  driver_factory_observation_.Observe(
      autofill_client->GetAutofillDriverFactory());
}

TouchToFillKeyboardSuppressor::~TouchToFillKeyboardSuppressor() {
  Unsuppress();
}

void TouchToFillKeyboardSuppressor::OnContentAutofillDriverFactoryDestroyed(
    ContentAutofillDriverFactory& factory) {
  Unsuppress();
  driver_factory_observation_.Reset();
}

void TouchToFillKeyboardSuppressor::OnContentAutofillDriverCreated(
    ContentAutofillDriverFactory& factory,
    ContentAutofillDriver& driver) {
  autofill_manager_observations_.AddObservation(driver.autofill_manager());
}

void TouchToFillKeyboardSuppressor::OnContentAutofillDriverWillBeDeleted(
    ContentAutofillDriverFactory& factory,
    ContentAutofillDriver& driver) {
  if (suppressed_manager_.get() == driver.autofill_manager()) {
    Unsuppress();
  }
  autofill_manager_observations_.RemoveObservation(driver.autofill_manager());
}

void TouchToFillKeyboardSuppressor::OnAutofillManagerDestroyed(
    AutofillManager& manager) {
  if (suppressed_manager_.get() == &manager) {
    Unsuppress();
  }
  autofill_manager_observations_.RemoveObservation(&manager);
}

void TouchToFillKeyboardSuppressor::OnBeforeAskForValuesToFill(
    AutofillManager& manager,
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  if (is_showing_.Run(manager) ||
      intends_to_show_.Run(manager, form_id, field_id)) {
    Suppress(manager);
  } else {
    Unsuppress();
  }
}

void TouchToFillKeyboardSuppressor::OnAfterAskForValuesToFill(
    AutofillManager& manager,
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  if (is_showing_.Run(manager)) {
    KeepSuppressing();
  } else {
    Unsuppress();
  }
}

void TouchToFillKeyboardSuppressor::Suppress(AutofillManager& manager) {
  if (suppressed_manager_.get() == &manager) {
    return;
  }
  Unsuppress();
  manager.SetShouldSuppressKeyboard(true);
  suppressed_manager_ = &manager;
  unsuppress_timer_.Start(FROM_HERE, timeout_, this,
                          &TouchToFillKeyboardSuppressor::Unsuppress);
}

void TouchToFillKeyboardSuppressor::Unsuppress() {
  if (!suppressed_manager_) {
    return;
  }
  suppressed_manager_->SetShouldSuppressKeyboard(false);
  suppressed_manager_ = nullptr;
  unsuppress_timer_.Stop();
}

}  // namespace autofill

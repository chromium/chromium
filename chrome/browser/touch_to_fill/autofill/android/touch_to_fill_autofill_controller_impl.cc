// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_autofill_controller_impl.h"

#include <cstddef>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_autofill_view.h"
#include "components/autofill/android/touch_to_fill_keyboard_suppressor.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_autofill_delegate.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

namespace {
TouchToFillAutofillDelegate* GetDelegate(AutofillManager& manager) {
  // TODO(crbug.com/527817991): Implement creating the delegate when connecting
  // the trigger point.
  return nullptr;
}
}  // namespace

TouchToFillAutofillControllerImpl::TouchToFillAutofillControllerImpl(
    ContentAutofillClient* autofill_client)
    : keyboard_suppressor_(
          autofill_client,
          base::BindRepeating([](AutofillManager& manager) {
            TouchToFillAutofillDelegate* delegate = GetDelegate(manager);
            return delegate && delegate->IsShowingTouchToFill();
          }),
          base::BindRepeating([](AutofillManager& manager,
                                 FormGlobalId form,
                                 FieldGlobalId field,
                                 const FormData& form_data) {
            TouchToFillAutofillDelegate* delegate = GetDelegate(manager);
            return delegate && delegate->IntendsToShowTouchToFill(form, field);
          }),
          base::Seconds(1)) {
  driver_factory_observation_.Observe(
      &autofill_client->GetAutofillDriverFactory());
}

TouchToFillAutofillControllerImpl::~TouchToFillAutofillControllerImpl() =
    default;

void TouchToFillAutofillControllerImpl::Hide() {
  if (view_) {
    view_->Hide();
  }
}

content::WebContents* TouchToFillAutofillControllerImpl::GetWebContents() {
  return driver_factory_observation_.GetSource()->web_contents();
}

bool TouchToFillAutofillControllerImpl::ShowPersonalContextNotice(
    std::unique_ptr<TouchToFillAutofillView> view,
    base::WeakPtr<TouchToFillAutofillDelegate> delegate) {
  view_ = std::move(view);
  delegate_ = delegate;
  return view_->ShowPersonalContextNotice(this);
}

}  // namespace autofill

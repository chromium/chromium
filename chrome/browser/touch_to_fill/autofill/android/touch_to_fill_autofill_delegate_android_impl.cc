// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_autofill_delegate_android_impl.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

TouchToFillAutofillDelegateAndroidImpl::TouchToFillAutofillDelegateAndroidImpl(
    BrowserAutofillManager* manager)
    : manager_(CHECK_DEREF(manager)) {}

TouchToFillAutofillDelegateAndroidImpl::
    ~TouchToFillAutofillDelegateAndroidImpl() = default;

bool TouchToFillAutofillDelegateAndroidImpl::IntendsToShowTouchToFill(
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  // TODO(crbug.com/521716313): Call FirstRunService to check whether to show
  // the TTF surface.
  return false;
}

bool TouchToFillAutofillDelegateAndroidImpl::TryToShowTouchToFill(
    const FormData& form,
    const FormFieldData& field) {
  return false;
}

bool TouchToFillAutofillDelegateAndroidImpl::IsShowingTouchToFill() {
  return false;
}

void TouchToFillAutofillDelegateAndroidImpl::HideTouchToFill() {}

void TouchToFillAutofillDelegateAndroidImpl::OnShow() {
  // TODO(crbug.com/521716313): Record shown metrics.
}

void TouchToFillAutofillDelegateAndroidImpl::OnNoticeAcknowledged() {
  // TODO(crbug.com/521716313): Call FirstRunService to mark the notice
  // acknowledged event.
}

void TouchToFillAutofillDelegateAndroidImpl::OnDismissed() {}

}  // namespace autofill

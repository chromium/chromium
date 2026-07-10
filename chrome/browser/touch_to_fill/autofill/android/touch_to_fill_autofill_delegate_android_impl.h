// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_DELEGATE_ANDROID_IMPL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_DELEGATE_ANDROID_IMPL_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_autofill_delegate.h"

namespace autofill {

class BrowserAutofillManager;

class TouchToFillAutofillDelegateAndroidImpl
    : public TouchToFillAutofillDelegate {
 public:
  explicit TouchToFillAutofillDelegateAndroidImpl(
      BrowserAutofillManager* manager);
  TouchToFillAutofillDelegateAndroidImpl(
      const TouchToFillAutofillDelegateAndroidImpl&) = delete;
  TouchToFillAutofillDelegateAndroidImpl& operator=(
      const TouchToFillAutofillDelegateAndroidImpl&) = delete;
  ~TouchToFillAutofillDelegateAndroidImpl() override;

  // TouchToFillAutofillDelegate:
  bool IntendsToShowTouchToFill(FormGlobalId form_id,
                                FieldGlobalId field_id) override;
  bool TryToShowTouchToFill(const FormData& form,
                            const FormFieldData& field) override;
  bool IsShowingTouchToFill() override;
  void HideTouchToFill() override;
  void OnShow() override;
  void OnNoticeAcknowledged() override;
  void OnDismissed() override;

 private:
  const raw_ref<BrowserAutofillManager> manager_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_DELEGATE_ANDROID_IMPL_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_DELEGATE_ANDROID_IMPL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_DELEGATE_ANDROID_IMPL_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_autofill_delegate.h"

namespace autofill {

class BrowserAutofillManager;

// Android implementation of TouchToFillAutofillDelegate.
//
// This class manages the state of the TouchToFill bottom sheet (currently
// showing a Personal Context notice). It uses a state machine to track
// whether the sheet is showing, inactive, or transitioning away (e.g., to
// settings).
//
// State transitions on dismissal:
// - User dismissal (swipe down or click Acknowledge): Transitions from
//   `kShowing` to `kInactive`, which triggers standard keyboard suggestions
//   so the user can continue filling the form.
// - Navigation dismissal (click on Settings): Transitions from `kShowing` to
//   `kNavigatingAway`. When the sheet is closed, it transitions to `kInactive`
//   but bypasses triggering suggestions since the user is leaving the page.
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
  void OnNoticeAcknowledged() override;
  void OnSettingsLinkClicked() override;
  void OnDismissed() override;

 private:
  enum class TouchToFillAutofillState {
    // TouchToFill is not active.
    kInactive,
    // The TouchToFill UI is currently showing.
    kShowing,
    // The user clicked a link or button that navigates away from the current
    // page. We are transitioning while the sheet is being dismissed, and we
    // want to bypass default dismissal behavior (e.g. triggering suggestions).
    kNavigatingAway,
    // The bottom sheet was dismissed, and we are temporarily suppressing
    // TouchToFill to allow standard suggestions to show on the re-triggered
    // flow.
    kSuppressing,
  };

  void TriggerAskForValuesToFill();

  const raw_ref<BrowserAutofillManager> manager_;

  TouchToFillAutofillState ttf_autofill_state_ =
      TouchToFillAutofillState::kInactive;

  FieldGlobalId query_field_id_;
  base::WeakPtrFactory<TouchToFillAutofillDelegateAndroidImpl>
      weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_DELEGATE_ANDROID_IMPL_H_

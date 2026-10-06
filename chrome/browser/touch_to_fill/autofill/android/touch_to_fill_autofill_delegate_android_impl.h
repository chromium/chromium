// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_DELEGATE_ANDROID_IMPL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_DELEGATE_ANDROID_IMPL_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_autofill_delegate.h"

namespace autofill {

class AutofillField;
class BrowserAutofillManager;
class FormStructure;

// Android implementation of TouchToFillAutofillDelegate.
//
// This class manages the state of the TouchToFill bottom sheet (currently
// showing either a Personal Context notice or an Autofill AI private inference
// notice). It uses a state machine to track whether the sheet is showing,
// inactive, or transitioning away (e.g., to settings).
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

  // The notices that can be shown in the TouchToFill bottom sheet.
  enum class NoticeType {
    // Informs the user that data from their Google Account can be filled.
    kPersonalContext,
    // Informs the user that page content may be processed in a private space
    // in the cloud.
    kPrivateInference,
  };

  // Returns the notice that should be shown for the given form field, or
  // `std::nullopt` if no notice should be shown. Personal Context takes
  // precedence over the private inference notice, mirroring the priority of
  // the corresponding suggestions.
  std::optional<NoticeType> GetNoticeToShow(FormGlobalId form_id,
                                            FieldGlobalId field_id);

  // Returns whether all conditions for showing the respective notice on
  // `field` of `form` are met.
  bool CanShowPersonalContextNotice(const FormStructure& form,
                                    const AutofillField& field);
  bool CanShowPrivateInferenceNotice(const FormStructure& form,
                                     const AutofillField& field);

  // Shows `notice` via the client. Returns whether it was shown.
  bool ShowNotice(NoticeType notice);

  void OnPersonalContextNoticeAcknowledged();
  void OnPrivateInferenceNoticeAcknowledged();

  void TriggerAskForValuesToFill();

  const raw_ref<BrowserAutofillManager> manager_;

  TouchToFillAutofillState ttf_autofill_state_ =
      TouchToFillAutofillState::kInactive;

  // The notice that is currently shown, or was shown last. It determines which
  // notice-specific logic (e.g. prefs or settings page) runs on user
  // interactions. Unset until a notice has been shown.
  std::optional<NoticeType> shown_notice_;

  FieldGlobalId query_field_id_;
  base::WeakPtrFactory<TouchToFillAutofillDelegateAndroidImpl>
      weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_DELEGATE_ANDROID_IMPL_H_

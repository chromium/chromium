// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_controller.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller.h"
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_bridge.h"
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_controller.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using autofill::password_generation::PasswordGenerationType;
using password_manager::metrics_util::GenerationDialogChoice;
using ShouldShowAction = ManualFillingController::ShouldShowAction;

TouchToFillPasswordGenerationController::
    ~TouchToFillPasswordGenerationController() {
  HideTouchToFill();
  RemoveSuppressShowingImeCallback();
}

TouchToFillPasswordGenerationController::
    TouchToFillPasswordGenerationController(
        base::WeakPtr<password_manager::ContentPasswordManagerDriver>
            frame_driver,
        content::WebContents* web_contents,
        PasswordGenerationElementData generation_element_data,
        std::unique_ptr<TouchToFillPasswordGenerationBridge> bridge,
        OnDismissedCallback on_dismissed_callback,
        base::WeakPtr<ManualFillingController> manual_filling_controller)
    : frame_driver_(frame_driver),
      web_contents_(web_contents),
      generation_element_data_(std::move(generation_element_data)),
      bridge_(std::move(bridge)),
      on_dismissed_callback_(std::move(on_dismissed_callback)),
      manual_filling_controller_(manual_filling_controller) {
  CHECK(bridge_);
  CHECK(on_dismissed_callback_);
  suppress_showing_ime_callback_ = base::BindRepeating([]() {
    // This controller exists only while the TTF is being shown, so
    // always suppress the keyboard.
    return true;
  });
}

bool TouchToFillPasswordGenerationController::ShowTouchToFill(
    std::string account_display_name,
    PasswordGenerationType type,
    PrefService* pref_service) {
  password_generation_type_ = type;

  std::u16string generated_password =
      frame_driver_->GetPasswordGenerationHelper()->GeneratePassword(
          web_contents_->GetLastCommittedURL().DeprecatedGetOriginAsURL(), type,
          generation_element_data_.form_signature,
          generation_element_data_.field_signature,
          generation_element_data_.max_password_length);
  if (!bridge_->Show(web_contents_, pref_service, this,
                     std::move(generated_password),
                     std::move(account_display_name))) {
    return false;
  }

  AddSuppressShowingImeCallback();
  return true;
}

void TouchToFillPasswordGenerationController::HideTouchToFill() {
  bridge_->Hide();
}

void TouchToFillPasswordGenerationController::OnDismissed(
    bool generated_password_accepted) {
  GenerationDialogChoice choice = generated_password_accepted
                                      ? GenerationDialogChoice::kAccepted
                                      : GenerationDialogChoice::kRejected;
  password_manager::metrics_util::LogGenerationDialogChoice(
      choice, password_generation_type_);

  if (on_dismissed_callback_) {
    std::exchange(on_dismissed_callback_, base::NullCallback()).Run();
  }
}

void TouchToFillPasswordGenerationController::OnGeneratedPasswordAccepted(
    const std::u16string& password) {
  frame_driver_->GeneratedPasswordAccepted(
      generation_element_data_.form_data,
      generation_element_data_.generation_element_id, password);
  frame_driver_->FocusNextFieldAfterPasswords();
}

void TouchToFillPasswordGenerationController::OnGeneratedPasswordRejected() {
  // TODO (crbug.com/1495639) Trigger Keyboard Accessory here.
}

void TouchToFillPasswordGenerationController::AddSuppressShowingImeCallback() {
  if (suppress_showing_ime_callback_added_) {
    return;
  }
  frame_driver_->render_frame_host()
      ->GetRenderWidgetHost()
      ->AddSuppressShowingImeCallback(suppress_showing_ime_callback_);
  suppress_showing_ime_callback_added_ = true;
}

void TouchToFillPasswordGenerationController::
    RemoveSuppressShowingImeCallback() {
  if (!suppress_showing_ime_callback_added_) {
    return;
  }
  if (frame_driver_) {
    frame_driver_->render_frame_host()
        ->GetRenderWidgetHost()
        ->RemoveSuppressShowingImeCallback(suppress_showing_ime_callback_,
                                           /*trigger_ime=*/false);
  }
  suppress_showing_ime_callback_added_ = false;
}

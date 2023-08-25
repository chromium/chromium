// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_controller.h"

#include <memory>
#include <string>
#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_bridge.h"
#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_controller.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

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
    std::string account_display_name) {
  std::u16string generated_password =
      frame_driver_->GetPasswordGenerationHelper()->GeneratePassword(
          web_contents_->GetLastCommittedURL().DeprecatedGetOriginAsURL(),
          generation_element_data_.form_signature,
          generation_element_data_.field_signature,
          generation_element_data_.max_password_length);
  if (!bridge_->Show(web_contents_, this, std::move(generated_password),
                     std::move(account_display_name))) {
    return false;
  }

  AddSuppressShowingImeCallback();
  return true;
}

void TouchToFillPasswordGenerationController::HideTouchToFill() {
  bridge_->Hide();
}

void TouchToFillPasswordGenerationController::OnDismissed() {
  if (on_dismissed_callback_) {
    std::exchange(on_dismissed_callback_, base::NullCallback()).Run();
  }
}

void TouchToFillPasswordGenerationController::OnGeneratedPasswordAccepted(
    const std::u16string& password) {
  frame_driver_->GeneratedPasswordAccepted(
      generation_element_data_.form_data,
      generation_element_data_.generation_element_id, password);
}

void TouchToFillPasswordGenerationController::OnGeneratedPasswordRejected() {
  manual_filling_controller_->OnAccessoryActionAvailabilityChanged(
      ShouldShowAction(true),
      autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC);
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
        ->RemoveSuppressShowingImeCallback(suppress_showing_ime_callback_);
  }
  suppress_showing_ime_callback_added_ = false;
}

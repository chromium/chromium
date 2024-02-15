// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_CONTROLLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller.h"
#include "chrome/browser/password_manager/android/password_generation_element_data.h"
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_bridge.h"
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_delegate.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"

namespace password_manager {
class ContentPasswordManagerDriver;
}  // namespace password_manager

// The controller responsible for the password generation bottom sheet UI.
// It should be created before showing the bottom sheet and destroyed right
// after the bottom sheet is dismissed.
class TouchToFillPasswordGenerationController
    : public TouchToFillPasswordGenerationDelegate {
 public:
  using OnDismissedCallback = base::OnceCallback<void()>;

  // If the bottom sheet was shown and dismissed more than
  // `kMaxAllowedNumberOfDismisses` it must not be displayed.
  static constexpr int kMaxAllowedNumberOfDismisses = 4;

  TouchToFillPasswordGenerationController(
      base::WeakPtr<password_manager::ContentPasswordManagerDriver>
          frame_driver,
      content::WebContents* web_contents,
      PasswordGenerationElementData generation_element_data,
      std::unique_ptr<TouchToFillPasswordGenerationBridge> bridge,
      OnDismissedCallback on_dismissed_callback,
      base::WeakPtr<ManualFillingController> manual_filling_controller);
  TouchToFillPasswordGenerationController(
      const TouchToFillPasswordGenerationController&) = delete;
  TouchToFillPasswordGenerationController& operator=(
      const TouchToFillPasswordGenerationController&) = delete;
  ~TouchToFillPasswordGenerationController() override;

  // Shows the password generation bottom sheet.
  bool ShowTouchToFill(
      std::string account_display_name,
      autofill::password_generation::PasswordGenerationType type,
      PrefService* pref_service);

  void OnDismissed(bool generated_password_accepted) override;

  void OnGeneratedPasswordAccepted(const std::u16string& password) override;

  void OnGeneratedPasswordRejected() override;

 private:
  // Suppressing IME input is necessary for Touch-To-Fill.
  void AddSuppressShowingImeCallback();
  void RemoveSuppressShowingImeCallback();

  // Hides the password generation bottom sheet.
  void HideTouchToFill();

  // Password manager driver for the frame on which the Touch-To-Fill was
  // triggered.
  base::WeakPtr<password_manager::ContentPasswordManagerDriver> frame_driver_;
  raw_ptr<content::WebContents> web_contents_;
  PasswordGenerationElementData generation_element_data_;
  std::unique_ptr<TouchToFillPasswordGenerationBridge> bridge_;
  OnDismissedCallback on_dismissed_callback_;
  // The manual filling controller object to forward client requests to.
  base::WeakPtr<ManualFillingController> manual_filling_controller_;

  content::RenderWidgetHost::SuppressShowingImeCallback
      suppress_showing_ime_callback_;
  bool suppress_showing_ime_callback_added_ = false;

  autofill::password_generation::PasswordGenerationType
      password_generation_type_;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_CONTROLLER_H_

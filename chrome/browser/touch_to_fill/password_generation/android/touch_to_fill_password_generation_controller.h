// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_CONTROLLER_H_

#include <string>
#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/password_manager/android/password_generation_element_data.h"
#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_bridge.h"
#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_delegate.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

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

  TouchToFillPasswordGenerationController(
      base::WeakPtr<password_manager::ContentPasswordManagerDriver>
          frame_driver,
      content::WebContents* web_contents,
      PasswordGenerationElementData generation_element_data,
      std::unique_ptr<TouchToFillPasswordGenerationBridge> bridge,
      OnDismissedCallback on_dismissed_callback);
  TouchToFillPasswordGenerationController(
      const TouchToFillPasswordGenerationController&) = delete;
  TouchToFillPasswordGenerationController& operator=(
      const TouchToFillPasswordGenerationController&) = delete;
  ~TouchToFillPasswordGenerationController() override;

  // Shows the password generation bottom sheet.
  bool ShowTouchToFill(std::string account_display_name);

  void OnDismissed() override;

  void OnGeneratedPasswordAccepted(const std::u16string& password) override;

 private:
  // Suppressing IME input is necessary for Touch-To-Fill.
  void AddSuppressShowingImeCallback();
  void RemoveSuppressShowingImeCallback();

  // Hides the password generation bottom sheet.
  void HideTouchToFill();

  // Password manager driver for the frame on which the Touch-To-Fill was
  // triggered.
  base::WeakPtr<password_manager::ContentPasswordManagerDriver> frame_driver_;
  base::raw_ptr<content::WebContents> web_contents_;
  PasswordGenerationElementData generation_element_data_;
  std::unique_ptr<TouchToFillPasswordGenerationBridge> bridge_;
  OnDismissedCallback on_dismissed_callback_;

  content::RenderWidgetHost::SuppressShowingImeCallback
      suppress_showing_ime_callback_;
  bool suppress_showing_ime_callback_added_ = false;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_CONTROLLER_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_CONTROLLER_H_

#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace password_manager {
class ContentPasswordManagerDriver;
}  // namespace password_manager

// The controller responsible for the password generation bottom sheet UI.
// It should be created before showing the bottom sheet and destroyed right
// after the bottom sheet is dismissed.
class TouchToFillPasswordGenerationController {
 public:
  explicit TouchToFillPasswordGenerationController(
      base::WeakPtr<password_manager::ContentPasswordManagerDriver>
          frame_driver);
  TouchToFillPasswordGenerationController(
      const TouchToFillPasswordGenerationController&) = delete;
  TouchToFillPasswordGenerationController& operator=(
      const TouchToFillPasswordGenerationController&) = delete;
  ~TouchToFillPasswordGenerationController();

  // Shows the password generation bottom sheet.
  void ShowTouchToFill();

 private:
  // Suppressing IME input is necessary for Touch-To-Fill.
  void AddSuppressShowingImeCallback();
  void RemoveSuppressShowingImeCallback();

  // Password manager driver for the frame on which the Touch-To-Fill was
  // triggered. Ensure that the bottom sheet should be hidden when the frame is
  // removed.
  base::WeakPtr<password_manager::ContentPasswordManagerDriver> frame_driver_;

  content::RenderWidgetHost::SuppressShowingImeCallback
      suppress_showing_ime_callback_;
  bool suppress_showing_ime_callback_added_ = false;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_CONTROLLER_H_

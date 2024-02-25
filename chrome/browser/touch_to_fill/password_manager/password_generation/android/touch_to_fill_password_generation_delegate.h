// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_DELEGATE_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_DELEGATE_H_

#include <string>

// The delegate, which handles the Password generation bottom sheet
// (Touch-To-Fill) functionality. Implemented by
// `TouchToFillPasswordGenerationController`.
class TouchToFillPasswordGenerationDelegate {
 public:
  virtual ~TouchToFillPasswordGenerationDelegate() = default;

  // Handles the bottom sheet dismissal. It's called in every execution path no
  // matter how the bottom sheet is dismissed.
  virtual void OnDismissed(bool generated_password_accepted) = 0;

  // Called if the user accepts the proposed generated password. Here the
  // password should be saved and filled into the form.
  virtual void OnGeneratedPasswordAccepted(const std::u16string& password) = 0;

  // Called if the user doesn't accept the proposed generated password. Here the
  // keyboard accessory bar with "Suggest strong password" button should be
  // displayed.
  virtual void OnGeneratedPasswordRejected() = 0;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_DELEGATE_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_DELEGATE_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_DELEGATE_H_

#include "base/memory/weak_ptr.h"

// The delegate, which handles the Password generation bottom sheet
// (Touch-To-Fill) functionality. Implemented by
// `TouchToFillPasswordGenerationController`.
class TouchToFillPasswordGenerationDelegate
    : public base::SupportsWeakPtr<TouchToFillPasswordGenerationDelegate> {
 public:
  virtual ~TouchToFillPasswordGenerationDelegate() = default;

  // Handles the bottom sheet dismissal. It's called in every execution path no
  // matter how the bottom sheet is dismissed.
  virtual void OnDismissed() = 0;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_DELEGATE_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_VIEW_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_VIEW_H_

namespace autofill {

class TouchToFillAutofillController;

// The UI interface which prompts the user with notices or suggestions
// using the Touch To Fill Autofill surface.
class TouchToFillAutofillView {
 public:
  virtual ~TouchToFillAutofillView() = default;

  virtual bool ShowPersonalContextNotice(
      TouchToFillAutofillController* controller) = 0;
  virtual void Hide() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_VIEW_H_

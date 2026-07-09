// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_controller_base.h"

namespace autofill {

class TouchToFillAutofillView;
class TouchToFillAutofillDelegate;

// Controller of the bottom sheet surface for filling autofill data on
// Android. It is responsible for showing the view and handling user
// interactions.
class TouchToFillAutofillController : public TouchToFillControllerBase {
 public:
  ~TouchToFillAutofillController() override = default;

  // Shows the Touch To Fill notice screen. Returns whether the surface was
  // successfully shown.
  virtual bool ShowPersonalContextNotice(
      std::unique_ptr<TouchToFillAutofillView> view,
      base::WeakPtr<TouchToFillAutofillDelegate> delegate) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_CONTROLLER_H_

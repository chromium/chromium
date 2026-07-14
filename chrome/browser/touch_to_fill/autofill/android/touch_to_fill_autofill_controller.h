// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_controller_base.h"

namespace autofill {

class ContentAutofillClient;
class TouchToFillAutofillView;
class TouchToFillAutofillDelegate;

// Controller for showing non-payments autofill bottomsheets on Android. It is
// responsible for showing the view and handling user interactions.
class TouchToFillAutofillController : public TouchToFillControllerBase {
 public:
  static std::unique_ptr<TouchToFillAutofillController> Create(
      ContentAutofillClient* autofill_client);

  ~TouchToFillAutofillController() override = default;

  // Shows the Touch To Fill notice screen. Returns whether the surface was
  // successfully shown.
  virtual bool ShowPersonalContextNotice(
      std::unique_ptr<TouchToFillAutofillView> view,
      base::WeakPtr<TouchToFillAutofillDelegate> delegate) = 0;

  // Called by the UI when the user acknowledges the notice.
  virtual void OnNoticeAcknowledged() = 0;

  // Called by the UI when the user clicks the Manage settings link.
  virtual void OnSettingsLinkClicked() = 0;

  // Called by the UI when the notice bottom sheet is dismissed.
  virtual void OnDismissed() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_CONTROLLER_H_

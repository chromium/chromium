// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_CONTROLLER_IMPL_H_

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_autofill_controller.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_autofill_view.h"
#include "components/autofill/android/touch_to_fill_keyboard_suppressor.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_autofill_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

class ContentAutofillClient;
class TouchToFillAutofillView;

// Controller of the bottom sheet surface for filling autofill data on Android.
// It is responsible for showing the view and handling user interactions.
class TouchToFillAutofillControllerImpl
    : public TouchToFillAutofillController,
      public ContentAutofillDriverFactory::Observer {
 public:
  explicit TouchToFillAutofillControllerImpl(
      ContentAutofillClient* autofill_client);
  TouchToFillAutofillControllerImpl(const TouchToFillAutofillControllerImpl&) =
      delete;
  TouchToFillAutofillControllerImpl& operator=(
      const TouchToFillAutofillControllerImpl&) = delete;
  ~TouchToFillAutofillControllerImpl() override;

  // TouchToFillControllerBase:
  void Hide() override;
  content::WebContents* GetWebContents() override;

  // TouchToFillAutofillController:
  bool ShowPersonalContextNotice(
      std::unique_ptr<TouchToFillAutofillView> view,
      base::WeakPtr<TouchToFillAutofillDelegate> delegate) override;

 private:
  base::ScopedObservation<ContentAutofillDriverFactory,
                          ContentAutofillDriverFactory::Observer>
      driver_factory_observation_{this};
  std::unique_ptr<TouchToFillAutofillView> view_;
  base::WeakPtr<TouchToFillAutofillDelegate> delegate_;
  TouchToFillKeyboardSuppressor keyboard_suppressor_;
  base::WeakPtrFactory<TouchToFillAutofillControllerImpl> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_CONTROLLER_IMPL_H_

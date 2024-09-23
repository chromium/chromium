// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_PAYMENT_METHOD_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_PAYMENT_METHOD_ACCESSORY_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/keyboard_accessory/android/accessory_controller.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"

namespace content {
class WebContents;
}

namespace autofill {

// Interface for payment method-specific keyboard accessory controller between the
// ManualFillingController and Autofill backend logic.
class PaymentMethodAccessoryController : public AccessoryController,
                                      public PersonalDataManagerObserver {
 public:
  PaymentMethodAccessoryController() = default;
  ~PaymentMethodAccessoryController() override = default;
  // Disallow copy and assign
  PaymentMethodAccessoryController(const PaymentMethodAccessoryController&) = delete;
  PaymentMethodAccessoryController& operator=(
      const PaymentMethodAccessoryController&) = delete;

  // Returns true if the accessory controller may exist for |web_contents|.
  // Otherwise it returns false.
  static bool AllowedForWebContents(content::WebContents* web_contents);

  // Returns a reference to the unique controller associated with
  // |web_contents|. A new instance is created if the first time this function
  // is called. Should only be called if AllowedForWebContents returns true for
  // |web_contents|.
  static PaymentMethodAccessoryController* GetOrCreate(
      content::WebContents* web_contents);

  // Returns a reference to the unique controller associated with
  // |web_contents|. Returns null if no such instance exists.
  static PaymentMethodAccessoryController* GetIfExisting(
      content::WebContents* web_contents);

  // Fetches suggestions and propagates to the frontend.
  virtual void RefreshSuggestions() = 0;

  // Get a WeakPtr to the instance.
  virtual base::WeakPtr<PaymentMethodAccessoryController> AsWeakPtr() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_PAYMENT_METHOD_ACCESSORY_CONTROLLER_H_

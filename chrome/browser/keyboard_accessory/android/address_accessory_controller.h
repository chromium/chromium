// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_ADDRESS_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_ADDRESS_ACCESSORY_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/keyboard_accessory/android/accessory_controller.h"
#include "content/public/browser/web_contents_user_data.h"

class AffiliatedPlusProfilesProvider;

namespace autofill {

// Interface for address-specific keyboard accessory controller between the
// ManualFillingController and the autofill backend.
//
// There is a single instance per WebContents that can be accessed by calling:
//     AddressAccessoryController::GetOrCreate(web_contents);
// On the first call, an instance is attached to |web_contents|, so it can be
// returned by subsequent calls.
class AddressAccessoryController : public AccessoryController {
 public:
  AddressAccessoryController() = default;

  AddressAccessoryController(const AddressAccessoryController&) = delete;
  AddressAccessoryController& operator=(const AddressAccessoryController&) =
      delete;

  ~AddressAccessoryController() override = default;

  // Returns a reference to the unique AddressAccessoryController associated
  // with |web_contents|. A new instance is created if the first time this
  // function is called. Only valid to be called if
  // |AddressAccessoryController::AllowedForWebContents(web_contents)|.
  static AddressAccessoryController* GetOrCreate(
      content::WebContents* web_contents);

  // Adds a plus profiles provider to this controller that is used to generate
  // the plus profiles section for the frontend.
  virtual void RegisterPlusProfilesProvider(
      base::WeakPtr<AffiliatedPlusProfilesProvider> provider) = 0;

  // Fetches suggestions and propagates them to the frontend.
  virtual void RefreshSuggestions() = 0;

  // Get a WeakPtr to the instance.
  virtual base::WeakPtr<AddressAccessoryController> AsWeakPtr() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_ADDRESS_ACCESSORY_CONTROLLER_H_

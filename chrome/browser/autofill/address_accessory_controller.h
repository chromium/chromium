// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ADDRESS_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_ADDRESS_ACCESSORY_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill/accessory_controller.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

// Interface for address-specific keyboard accessory controller between the
// ManualFillingController and the autofill backend.
//
// There is a single instance per WebContents that can be accessed by calling:
//     AddressAccessoryController::GetOrCreate(web_contents);
// On the first call, an instance is attached to |web_contents|, so it can be
// returned by subsequent calls.
class AddressAccessoryController
    : public base::SupportsWeakPtr<AddressAccessoryController>,
      public AccessoryController {
 public:
  AddressAccessoryController() = default;
  ~AddressAccessoryController() override = default;

  // Returns true if the accessory controller may exist for |web_contents|.
  // Otherwise it returns false.
  static bool AllowedForWebContents(content::WebContents* web_contents);

  // Returns a reference to the unique AddressAccessoryController associated
  // with |web_contents|. A new instance is created if the first time this
  // function is called. Only valid to be called if
  // |AddressAccessoryController::AllowedForWebContents(web_contents)|.
  static AddressAccessoryController* GetOrCreate(
      content::WebContents* web_contents);

  // Returns a reference to the unique AddressAccessoryController associated
  // with |web_contents|. Returns null if no such instance exists.
  static AddressAccessoryController* GetIfExisting(
      content::WebContents* web_contents);

  // Fetches suggestions and propagates them to the frontend.
  virtual void RefreshSuggestions() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AddressAccessoryController);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ADDRESS_ACCESSORY_CONTROLLER_H_

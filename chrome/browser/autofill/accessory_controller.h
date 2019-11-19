// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_ACCESSORY_CONTROLLER_H_

#include <vector>

#include "components/autofill/core/browser/ui/accessory_sheet_data.h"

// Interface for the portions of type-specific manual filling controllers (e.g.,
// password, credit card) which interact with the generic
// ManualFillingController.
class AccessoryController {
 public:
  virtual ~AccessoryController() = default;

  // Triggered when a user selects an item for filling. This handler is
  // responsible for propagating it so that it ultimately ends up in the form
  // in the content area.
  virtual void OnFillingTriggered(
      const autofill::UserInfo::Field& selection) = 0;

  // Triggered when a user selects an option.
  virtual void OnOptionSelected(autofill::AccessoryAction selected_action) = 0;
};

#endif  // CHROME_BROWSER_AUTOFILL_ACCESSORY_CONTROLLER_H_

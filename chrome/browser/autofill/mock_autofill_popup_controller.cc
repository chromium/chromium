// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/mock_autofill_popup_controller.h"

namespace autofill {

MockAutofillPopupController::MockAutofillPopupController() {
  gfx::FontList::SetDefaultFontDescription("Arial, Times New Roman, 15px");
  layout_model_ = std::make_unique<autofill::AutofillPopupLayoutModel>(
      this, false /* is_credit_card_field */);
}

MockAutofillPopupController::~MockAutofillPopupController() = default;

}  // namespace autofill

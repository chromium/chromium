// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/mock_autofill_popup_controller.h"
#include "ui/gfx/font_list.h"

namespace autofill {

MockAutofillPopupController::MockAutofillPopupController()
    : default_font_desc_setter_("Arial, Times New Roman, 15px") {}

MockAutofillPopupController::~MockAutofillPopupController() = default;

}  // namespace autofill

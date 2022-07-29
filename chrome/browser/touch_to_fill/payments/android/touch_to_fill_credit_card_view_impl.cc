// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_view_impl.h"

#include "base/notreached.h"

namespace autofill {

TouchToFillCreditCardViewImpl::TouchToFillCreditCardViewImpl() = default;
TouchToFillCreditCardViewImpl::~TouchToFillCreditCardViewImpl() = default;

bool TouchToFillCreditCardViewImpl::Show() {
  // TODO(crbug.com/1247698): Show Android view.
  NOTIMPLEMENTED();
  return false;
}

void TouchToFillCreditCardViewImpl::Hide() {
  // TODO(crbug.com/1247698): Hide Android view.
  NOTIMPLEMENTED();
}

}  // namespace autofill

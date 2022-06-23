// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_external_action_delegate.h"

FastCheckoutExternalActionDelegate::~FastCheckoutExternalActionDelegate() =
    default;

void FastCheckoutExternalActionDelegate::OnActionRequested(
    const autofill_assistant::external::Action& action_info,
    base::OnceCallback<void(DomUpdateCallback)> start_dom_checks_callback,
    base::OnceCallback<void(const autofill_assistant::external::Result&)>
        end_action_callback) {
  // TODO(crbug.com/1338523): Implement.
}

void FastCheckoutExternalActionDelegate::OnInterruptStarted() {
  // TODO(crbug.com/1338523): Implement.
}

void FastCheckoutExternalActionDelegate::OnInterruptFinished() {
  // TODO(crbug.com/1338523): Implement.
}

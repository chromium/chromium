// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_accessibility_service_impl.h"

#include "chrome/browser/ui/android/autofill/autofill_accessibility_utils.h"

void FastCheckoutAccessibilityServiceImpl::Announce(
    const std::u16string& text) {
  autofill::AnnounceTextForA11y(text);
}

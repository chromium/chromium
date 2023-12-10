// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_ACCESSIBILITY_SERVICE_IMPL_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_ACCESSIBILITY_SERVICE_IMPL_H_

#include "chrome/browser/fast_checkout/fast_checkout_accessibility_service.h"

class FastCheckoutAccessibilityServiceImpl
    : public FastCheckoutAccessibilityService {
 public:
  FastCheckoutAccessibilityServiceImpl() = default;
  ~FastCheckoutAccessibilityServiceImpl() override = default;

  FastCheckoutAccessibilityServiceImpl(
      const FastCheckoutAccessibilityServiceImpl&) = delete;
  FastCheckoutAccessibilityServiceImpl& operator=(
      const FastCheckoutAccessibilityServiceImpl&) = delete;

  void Announce(const std::u16string& text) override;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_ACCESSIBILITY_SERVICE_IMPL_H_

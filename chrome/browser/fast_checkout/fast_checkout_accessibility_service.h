// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_ACCESSIBILITY_SERVICE_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_ACCESSIBILITY_SERVICE_H_

#include <string>

class FastCheckoutAccessibilityService {
 public:
  FastCheckoutAccessibilityService() = default;
  virtual ~FastCheckoutAccessibilityService() = default;
  FastCheckoutAccessibilityService(const FastCheckoutAccessibilityService&) =
      delete;
  FastCheckoutAccessibilityService& operator=(
      const FastCheckoutAccessibilityService&) = delete;

  virtual void Announce(const std::u16string& text);
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_ACCESSIBILITY_SERVICE_H_

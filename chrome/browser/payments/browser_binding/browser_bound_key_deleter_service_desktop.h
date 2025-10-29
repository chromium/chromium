// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_SERVICE_DESKTOP_H_
#define CHROME_BROWSER_PAYMENTS_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_SERVICE_DESKTOP_H_

#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service.h"

namespace payments {

class BrowserBoundKeyDeleterServiceDesktop
    : public BrowserBoundKeyDeleterService {
 public:
  BrowserBoundKeyDeleterServiceDesktop();

  ~BrowserBoundKeyDeleterServiceDesktop() override;

  void RemoveInvalidBBKs() override;
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_SERVICE_DESKTOP_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service_desktop.h"

#include <memory>

#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service.h"
#include "components/payments/content/web_payments_web_data_service.h"

namespace payments {

std::unique_ptr<BrowserBoundKeyDeleterService>
GetBrowserBoundKeyDeleterServiceInstance(
    scoped_refptr<WebPaymentsWebDataService> web_data_service) {
  return std::make_unique<BrowserBoundKeyDeleterServiceDesktop>();
}

BrowserBoundKeyDeleterServiceDesktop::BrowserBoundKeyDeleterServiceDesktop() =
    default;

BrowserBoundKeyDeleterServiceDesktop::~BrowserBoundKeyDeleterServiceDesktop() =
    default;

void BrowserBoundKeyDeleterServiceDesktop::RemoveInvalidBBKs() {
  // TODO(crbug.com/441553248): Implement in a follow-up CL.
}

}  // namespace payments

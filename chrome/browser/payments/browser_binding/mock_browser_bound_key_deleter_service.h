// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_BROWSER_BINDING_MOCK_BROWSER_BOUND_KEY_DELETER_SERVICE_H_
#define CHROME_BROWSER_PAYMENTS_BROWSER_BINDING_MOCK_BROWSER_BOUND_KEY_DELETER_SERVICE_H_

#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace payments {

class MockBrowserBoundKeyDeleterService : public BrowserBoundKeyDeleterService {
 public:
  MockBrowserBoundKeyDeleterService();

  ~MockBrowserBoundKeyDeleterService() override;

  MOCK_METHOD(void, RemoveInvalidBBKs, (), (override));
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_BROWSER_BINDING_MOCK_BROWSER_BOUND_KEY_DELETER_SERVICE_H_

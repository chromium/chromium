// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_SERVICE_H_
#define CHROME_BROWSER_PAYMENTS_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace payments {

class BrowserBoundKeyDeleterService : public KeyedService {
 public:
  BrowserBoundKeyDeleterService() = default;

  // Non-copyable
  BrowserBoundKeyDeleterService(const BrowserBoundKeyDeleterService&) = delete;
  BrowserBoundKeyDeleterService operator=(
      const BrowserBoundKeyDeleterService&) = delete;

  // Non-moveable
  BrowserBoundKeyDeleterService(const BrowserBoundKeyDeleterService&&) = delete;
  BrowserBoundKeyDeleterService operator=(
      const BrowserBoundKeyDeleterService&&) = delete;

  ~BrowserBoundKeyDeleterService() override = default;

  // Starts the asynchronous process to find browser bound keys and delete them.
  virtual void RemoveInvalidBBKs() = 0;
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_SERVICE_H_

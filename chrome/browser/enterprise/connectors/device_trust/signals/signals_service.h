// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_H_

#include <memory>

#include "base/callback.h"
#include "base/values.h"

namespace enterprise_connectors {

// Service in charge of retrieving context-aware signals for its consumers.
class SignalsService {
 public:
  using CollectSignalsCallback =
      base::OnceCallback<void(const base::Value::Dict)>;

  virtual ~SignalsService() = default;

  // Collects the signals based on the current environment and asynchronously
  // returns them via `callback`.
  virtual void CollectSignals(CollectSignalsCallback callback) = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_H_

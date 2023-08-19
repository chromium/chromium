// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_FILTERER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_FILTERER_H_

#include "base/values.h"

namespace enterprise_connectors {

// This class is in charge of removing or modifying signals from the signal
// payload, e.g., for privacy reasons.
class SignalsFilterer {
 public:
  virtual ~SignalsFilterer() = default;

  // Modifies `signals` based on the current device context.
  virtual void Filter(base::Value::Dict& signals);
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_FILTERER_H_

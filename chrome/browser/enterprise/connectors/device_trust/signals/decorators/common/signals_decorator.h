// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_SIGNALS_DECORATOR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_SIGNALS_DECORATOR_H_

#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"

namespace enterprise_connectors {

// Interface representing instances in charge of decorating DeviceTrustSignals
// instances by adding signals data to them.
class SignalsDecorator {
 public:
  virtual ~SignalsDecorator() = default;

  // Sets signals data on the |signals| proto properties.
  virtual void Decorate(DeviceTrustSignals& signals) = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_SIGNALS_DECORATOR_H_

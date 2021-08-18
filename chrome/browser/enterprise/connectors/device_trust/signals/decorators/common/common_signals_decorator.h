// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_COMMON_SIGNALS_DECORATOR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_COMMON_SIGNALS_DECORATOR_H_

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"

namespace enterprise_connectors {

// Definition of the SignalsDecorator common to all platforms.
class CommonSignalsDecorator : public SignalsDecorator {
 public:
  CommonSignalsDecorator();
  ~CommonSignalsDecorator() override;

  // SignalsDecorator:
  void Decorate(DeviceTrustSignals& signals) override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_COMMON_SIGNALS_DECORATOR_H_

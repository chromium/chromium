// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_ASH_ASH_SIGNALS_DECORATOR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_ASH_ASH_SIGNALS_DECORATOR_H_

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"

#include "base/values.h"

namespace policy {
class BrowserPolicyConnectorAsh;
}  // namespace policy

namespace enterprise_connectors {

// Definition of the SignalsDecorator for Chrome OS Ash platform.
class AshSignalsDecorator : public SignalsDecorator {
 public:
  AshSignalsDecorator(
      policy::BrowserPolicyConnectorAsh* browser_policy_connector);
  ~AshSignalsDecorator() override;

  // SignalsDecorator:
  void Decorate(base::Value::Dict& signals,
                base::OnceClosure done_closure) override;

 private:
  policy::BrowserPolicyConnectorAsh* const browser_policy_connector_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_ASH_ASH_SIGNALS_DECORATOR_H_

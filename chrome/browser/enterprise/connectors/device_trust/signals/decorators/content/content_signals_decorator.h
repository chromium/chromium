// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_CONTENT_CONTENT_SIGNALS_DECORATOR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_CONTENT_CONTENT_SIGNALS_DECORATOR_H_

#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"

class PolicyBlocklistService;

namespace enterprise_connectors {

// Definition of the SignalsDecorator common to platforms supporting the
// Content API.
class ContentSignalsDecorator : public SignalsDecorator {
 public:
  explicit ContentSignalsDecorator(
      PolicyBlocklistService* policy_blocklist_service);
  ~ContentSignalsDecorator() override;

  // SignalsDecorator:
  void Decorate(base::Value::Dict& signals,
                base::OnceClosure done_closure) override;

 private:
  PolicyBlocklistService* policy_blocklist_service_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_CONTENT_CONTENT_SIGNALS_DECORATOR_H_

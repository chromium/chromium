// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_COMMON_SIGNALS_DECORATOR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_COMMON_SIGNALS_DECORATOR_H_

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"

class PolicyBlocklistService;
class PrefService;

namespace enterprise_connectors {

// Definition of the SignalsDecorator common to all platforms.
class CommonSignalsDecorator : public SignalsDecorator {
 public:
  CommonSignalsDecorator(PrefService* local_state,
                         PrefService* profile_prefs,
                         PolicyBlocklistService* policy_blocklist_service);
  ~CommonSignalsDecorator() override;

  // SignalsDecorator:
  void Decorate(SignalsType& signals, base::OnceClosure done_closure) override;

 private:
  PrefService* local_state_;
  PrefService* profile_prefs_;
  PolicyBlocklistService* policy_blocklist_service_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_COMMON_SIGNALS_DECORATOR_H_

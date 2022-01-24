// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_BROWSER_BROWSER_SIGNALS_DECORATOR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_BROWSER_BROWSER_SIGNALS_DECORATOR_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"

namespace policy {
class BrowserDMTokenStorage;
class CloudPolicyStore;
}  // namespace policy

namespace enterprise_signals {
class DeviceInfoFetcher;
}  // namespace enterprise_signals

namespace enterprise_connectors {

// Definition of the SignalsDecorator common to all Chrome browser platforms.
class BrowserSignalsDecorator : public SignalsDecorator {
 public:
  BrowserSignalsDecorator(policy::BrowserDMTokenStorage* dm_token_storage,
                          policy::CloudPolicyStore* cloud_policy_store,
                          std::unique_ptr<enterprise_signals::DeviceInfoFetcher>
                              device_info_fetcher);
  ~BrowserSignalsDecorator() override;

  // SignalsDecorator:
  void Decorate(DeviceTrustSignals& signals,
                base::OnceClosure done_closure) override;

 private:
  void DecorateOnBackgroundThread(DeviceTrustSignals& signals,
                                  base::OnceClosure done_closure);

  policy::BrowserDMTokenStorage* const dm_token_storage_;
  policy::CloudPolicyStore* const cloud_policy_store_;
  std::unique_ptr<enterprise_signals::DeviceInfoFetcher> device_info_fetcher_;

  base::WeakPtrFactory<BrowserSignalsDecorator> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_BROWSER_BROWSER_SIGNALS_DECORATOR_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_BROWSER_BROWSER_SIGNALS_DECORATOR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_BROWSER_BROWSER_SIGNALS_DECORATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {
class CloudPolicyStore;
}  // namespace policy

namespace enterprise_signals {
struct DeviceInfo;
}  // namespace enterprise_signals

namespace enterprise_connectors {

// Definition of the SignalsDecorator common to all Chrome browser platforms.
class BrowserSignalsDecorator : public SignalsDecorator {
 public:
  explicit BrowserSignalsDecorator(
      policy::CloudPolicyStore* cloud_policy_store);
  ~BrowserSignalsDecorator() override;

  // SignalsDecorator:
  void Decorate(base::Value::Dict& signals,
                base::OnceClosure done_closure) override;

 private:
  void OnDeviceInfoFetched(base::Value::Dict& signals,
                           base::TimeTicks start_time,
                           base::OnceClosure done_closure,
                           const enterprise_signals::DeviceInfo& device_info);

  const raw_ptr<policy::CloudPolicyStore> cloud_policy_store_;

  base::WeakPtrFactory<BrowserSignalsDecorator> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_BROWSER_BROWSER_SIGNALS_DECORATOR_H_

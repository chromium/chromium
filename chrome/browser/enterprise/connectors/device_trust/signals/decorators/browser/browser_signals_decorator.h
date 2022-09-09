// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_BROWSER_BROWSER_SIGNALS_DECORATOR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_BROWSER_BROWSER_SIGNALS_DECORATOR_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {
class BrowserDMTokenStorage;
class CloudPolicyStore;
}  // namespace policy

namespace enterprise_signals {
struct DeviceInfo;
}  // namespace enterprise_signals

namespace enterprise_connectors {

// Definition of the SignalsDecorator common to all Chrome browser platforms.
class BrowserSignalsDecorator : public SignalsDecorator {
 public:
  BrowserSignalsDecorator(policy::BrowserDMTokenStorage* dm_token_storage,
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

  void UpdateFromCache(base::Value::Dict& signals);

  policy::BrowserDMTokenStorage* const dm_token_storage_;
  policy::CloudPolicyStore* const cloud_policy_store_;

  // Use this variable to control whether or not the cache has been set since
  // some platforms may not have those values at all.
  bool cache_initialized_{false};

  // These values are expensive to fetch and are not expected to change
  // throughout the browser's lifetime, so the decorator will be caching them
  // for performance reasons.
  absl::optional<std::string> cached_serial_number_;
  absl::optional<bool> cached_is_disk_encrypted_;

  base::WeakPtrFactory<BrowserSignalsDecorator> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_BROWSER_BROWSER_SIGNALS_DECORATOR_H_

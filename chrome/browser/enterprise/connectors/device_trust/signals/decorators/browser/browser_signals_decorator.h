// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_BROWSER_BROWSER_SIGNALS_DECORATOR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_BROWSER_BROWSER_SIGNALS_DECORATOR_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"

class Profile;

namespace policy {
class CloudPolicyManager;
}  // namespace policy

namespace enterprise_core {
class DependencyFactory;
}  // namespace enterprise_core

namespace enterprise_signals {
struct DeviceInfo;
}  // namespace enterprise_signals

namespace device_signals {
class SignalsAggregator;
struct SignalsAggregationResponse;
}  // namespace device_signals

namespace enterprise_connectors {

// Definition of the SignalsDecorator common to all Chrome browser platforms.
class BrowserSignalsDecorator : public SignalsDecorator {
 public:
  BrowserSignalsDecorator(
      policy::CloudPolicyManager* browser_cloud_policy_manager,
      std::unique_ptr<enterprise_core::DependencyFactory> dependency_factory,
      device_signals::SignalsAggregator* signals_aggregator);
  ~BrowserSignalsDecorator() override;

  // SignalsDecorator:
  void Decorate(base::Value::Dict& signals,
                base::OnceClosure done_closure) override;

 private:
  // Called when done retrieving signals from DeviceInfoFetcher. The retrieved
  // signals are added to `signals` for the caller to use. `done_closure` can be
  // invoked to indicate that this part of the signals collection has concluded.
  // `device_info` represents the signals collected by the DeviceInfoFetcher.
  void OnDeviceInfoFetched(base::Value::Dict& signals,
                           base::OnceClosure done_closure,
                           const enterprise_signals::DeviceInfo& device_info);

  // Called when done retrieving signals from SignalsAggregator. The retrieved
  // signals are added to `signals` for the caller to use. `done_closure` can be
  // invoked to indicate that this part of the signals collection has concluded.
  // `response` represents the signals collected by the aggregator.
  void OnAggregatedSignalsReceived(
      base::Value::Dict& signals,
      base::OnceClosure done_closure,
      device_signals::SignalsAggregationResponse response);

  // Ultimately called when all async signals are retrieved. `start_time`
  // indicates the timestamp at which signal collection started for this
  // decorator. `done_closure` will be run to let the caller know that the
  // decorator is done collecting signals.
  void OnAllSignalsReceived(base::TimeTicks start_time,
                            base::OnceClosure done_closure);

  const raw_ptr<policy::CloudPolicyManager> browser_cloud_policy_manager_;
  std::unique_ptr<enterprise_core::DependencyFactory> dependency_factory_;

  // Signals aggregator, which is a profile-keyed service. Can be nullptr in
  // the case where the Profile is an incognito profile.
  const raw_ptr<device_signals::SignalsAggregator> signals_aggregator_;

  base::WeakPtrFactory<BrowserSignalsDecorator> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_BROWSER_BROWSER_SIGNALS_DECORATOR_H_

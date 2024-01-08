// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_ASH_ASH_SIGNALS_DECORATOR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_ASH_ASH_SIGNALS_DECORATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/core/device_attributes.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"
#include "chromeos/crosapi/mojom/networking_attributes.mojom.h"

#include "base/values.h"

namespace policy {
class BrowserPolicyConnectorAsh;
}  // namespace policy

class Profile;

namespace enterprise_connectors {

// Definition of the SignalsDecorator for ChromeOS Ash platform.
class AshSignalsDecorator : public SignalsDecorator {
 public:
  AshSignalsDecorator(
      policy::BrowserPolicyConnectorAsh* browser_policy_connector,
      Profile* profile);
  ~AshSignalsDecorator() override;

  // SignalsDecorator:
  void Decorate(base::Value::Dict& signals,
                base::OnceClosure done_closure) override;

 private:
  void OnNetworkInfoRetrieved(
      base::Value::Dict& signals,
      base::TimeTicks start_time,
      base::OnceClosure done_closure,
      crosapi::mojom::GetNetworkDetailsResultPtr result);

  const raw_ptr<policy::BrowserPolicyConnectorAsh> browser_policy_connector_;
  raw_ptr<Profile> profile_;

  std::unique_ptr<policy::DeviceAttributes> attributes_;

  base::WeakPtrFactory<AshSignalsDecorator> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_ASH_ASH_SIGNALS_DECORATOR_H_

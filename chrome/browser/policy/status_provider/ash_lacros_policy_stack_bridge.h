// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_STATUS_PROVIDER_ASH_LACROS_POLICY_STACK_BRIDGE_H_
#define CHROME_BROWSER_POLICY_STATUS_PROVIDER_ASH_LACROS_POLICY_STACK_BRIDGE_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/value_provider/policy_value_provider.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"

// AshLacrosPolicyStackBridge makes policy status from Ash-Chrome available in
// Lacros through crosapi mojom calls. Provides device policy values and
// status for Lacros through Ash and supports triggering reload of all policies
// on Ash through mojom calls. Fetches the device policy values and status on
// construction with an async call. Returns empty if GetStatus() or GetValues()
// is called before the fetch is complete.
class AshLacrosPolicyStackBridge : public policy::PolicyStatusProvider,
                                   public policy::PolicyValueProvider {
 public:
  AshLacrosPolicyStackBridge();
  ~AshLacrosPolicyStackBridge() override;

  // PolicyStatusProvider implementation.
  base::Value::Dict GetStatus() override;

  // PolicyValueProvider implementation.
  // Chrome policies may have common policies between Lacros policies and the
  // values may be overwritten in case of a merge. Make sure you call
  // `ChromePolicesValueProvider`'s GetValues() function before this if you want
  // to merge the policies with Chrome policies.
  base::Value::Dict GetValues() override;

  // Returns empty dictionary because Lacros doesn't have unique policy names.
  // All related policy names are share between Chrome policies and can be
  // obtained by ChromePoliciesValueProvider.
  base::Value::Dict GetNames() override;

  // Refreshes all policies in Ash with a crosapi mojom call.
  void Refresh() override;

 private:
  void LoadDevicePolicy();

  void OnDevicePolicyLoaded(base::Value::Dict device_policy,
                            base::Value::Dict legend_data);

  void OnDevicePolicyLoadedDeprecated(base::Value device_policy,
                                      base::Value legend_data);

  base::Value::Dict device_policy_status_;
  base::Value::Dict device_policy_;
  base::WeakPtrFactory<AshLacrosPolicyStackBridge> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_POLICY_STATUS_PROVIDER_ASH_LACROS_POLICY_STACK_BRIDGE_H_

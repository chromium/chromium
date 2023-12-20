// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_VALUE_AND_STATUS_AGGREGATOR_H_
#define CHROME_BROWSER_POLICY_POLICY_VALUE_AND_STATUS_AGGREGATOR_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/value_provider/policy_value_provider.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "extensions/buildflags/buildflags.h"

class Profile;

namespace policy {

extern const char kUserStatusKey[];
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
extern const char kDeviceStatusKey[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

// PolicyValueAndStatusAggregator is a wrapper class that will contain all the
// platform-specific PolicyStatusProviders and PolicyValueProviders. It will
// call GetStatus(), GetValues(), GetNames() on the available providers, merge
// them and return the dictionary that contains all the available information.
class PolicyValueAndStatusAggregator : public PolicyValueProvider::Observer,
                                       public PolicyStatusProvider::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPolicyValueAndStatusChanged() = 0;
  };

  // Returns the PolicyValueAndStatusAggregator instance for the running
  // platform.
  static std::unique_ptr<PolicyValueAndStatusAggregator>
  CreateDefaultPolicyValueAndStatusAggregator(Profile* profile);

  PolicyValueAndStatusAggregator(const PolicyValueAndStatusAggregator&) =
      delete;
  PolicyValueAndStatusAggregator& operator=(
      const PolicyValueAndStatusAggregator&) = delete;
  ~PolicyValueAndStatusAggregator() override;

  // Returns the dictionary containing the policy metadata available for the
  // platform.
  base::Value::Dict GetAggregatedPolicyStatus();

  // Returns the dictionary containing policy names.
  base::Value::Dict GetAggregatedPolicyNames();

  // Returns the available policy values.
  base::Value::Dict GetAggregatedPolicyValues();

  // Refreshes the policy values by calling Refresh() on all
  // PolicyValueProviders and notifies the observers about policy value and
  // status change when the refresh is done.
  void Refresh();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // PolicyValueProvider::Observer implementation.
  void OnPolicyValueChanged() override;

  // PolicyStatusProvider::Observer implementation.
  void OnPolicyStatusChanged() override;

  void AddPolicyValueProvider(
      std::unique_ptr<PolicyValueProvider> value_provider);

  void AddPolicyStatusProvider(
      std::string status_provider_description,
      std::unique_ptr<PolicyStatusProvider> status_provider);

  template <typename ProviderType>
  void AddPolicyStatusAndValueProvider(
      std::string status_provider_description,
      std::unique_ptr<ProviderType> status_and_value_provider) {
    AddPolicyValueProviderUnowned(status_and_value_provider.get());
    AddPolicyStatusProvider(status_provider_description,
                            std::move(status_and_value_provider));
  }

 private:
  PolicyValueAndStatusAggregator();

  void NotifyValueAndStatusChange();

  // Adds `value_provider` to `value_providers_unowned_`.
  void AddPolicyValueProviderUnowned(PolicyValueProvider* value_provider);

  // Contains the available PolicyStatusProviders for the platform with the
  // description strings.
  base::flat_map<std::string, std::unique_ptr<PolicyStatusProvider>>
      status_providers_;
  // Contains the PolicyValueProviders available for the platform.
  std::vector<std::unique_ptr<PolicyValueProvider>> value_providers_;
  // Contains the pointers to PolicyValueProvider which also implement
  // PolicyStatusProvider. The ownership of the instances will be in
  // `status_providers_`.
  std::vector<raw_ptr<PolicyValueProvider, VectorExperimental>>
      value_providers_unowned_;
  base::ObserverList<Observer> observers_;
  base::ScopedMultiSourceObservation<PolicyValueProvider,
                                     PolicyValueProvider::Observer>
      policy_value_provider_observations_{this};
  base::ScopedMultiSourceObservation<PolicyStatusProvider,
                                     PolicyStatusProvider::Observer>
      policy_status_provider_observations_{this};
};
}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_VALUE_AND_STATUS_AGGREGATOR_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_VALUE_PROVIDER_POLICY_VALUE_PROVIDER_H_
#define CHROME_BROWSER_POLICY_VALUE_PROVIDER_POLICY_VALUE_PROVIDER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "components/policy/resources/webui/mojom/policy.mojom-forward.h"

namespace policy {

// An interface for querying a policy provider about policy names and values and
// refreshing them.
class PolicyValueProvider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPolicyValueChanged() = 0;
  };

  PolicyValueProvider();
  PolicyValueProvider(const PolicyValueProvider&) = delete;
  PolicyValueProvider& operator=(const PolicyValueProvider&) = delete;
  virtual ~PolicyValueProvider();

  // Returns the dictionary containing policy values.
  virtual base::DictValue GetValues() = 0;
  // TODO(crbug.com/40897784): Remove the non-mojo version once the migration is
  // complete and make the mojo definition pure.
  virtual base::flat_map<std::string, policy::mojom::PolicyGroupPtr>
  GetValuesMojo() const;

  // Returns the dictionary containing the policy names.
  virtual base::DictValue GetNames() = 0;
  // TODO(crbug.com/40897784): Remove the non-mojo version once the migration is
  // complete and make the mojo definition pure.
  virtual base::flat_map<std::string, policy::mojom::PolicyGroupNamesPtr>
  GetNamesMojo() const;

  // Refreshes the policy values and notifies the observers.
  virtual void Refresh();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyValueChange();

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_VALUE_PROVIDER_POLICY_VALUE_PROVIDER_H_

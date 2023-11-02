// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_VALUE_PROVIDER_POLICY_VALUE_PROVIDER_H_
#define CHROME_BROWSER_POLICY_VALUE_PROVIDER_POLICY_VALUE_PROVIDER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/values.h"

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
  virtual base::Value::Dict GetValues() = 0;

  // Returns the dictionary containing the policy names.
  virtual base::Value::Dict GetNames() = 0;

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

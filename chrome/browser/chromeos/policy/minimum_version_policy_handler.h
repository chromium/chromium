// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_MINIMUM_VERSION_POLICY_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_MINIMUM_VERSION_POLICY_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"

namespace policy {

// This class observes the device setting |kMinimumRequiredChromeVersion|, and
// checks if respective requirement is met.
class MinimumVersionPolicyHandler {
 public:
  class Observer {
   public:
    virtual void OnMinimumVersionStateChanged() = 0;
    virtual ~Observer() = default;
  };

  explicit MinimumVersionPolicyHandler(chromeos::CrosSettings* cros_settings);
  ~MinimumVersionPolicyHandler();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool RequirementsAreSatisfied() const { return requirements_met_; }

  // Returns |true| if the requirements represented by the given
  // |kMinimumRequiredChromeVersion| setting string are satisfied.
  // Defaults to |true| if there are no requirements, or no valid requirements.
  static bool AreRequirementsSatisfied(
      const std::string& min_chrome_version_string);

 private:
  void OnPolicyChanged();

  void NotifyMinimumVersionStateChanged();

  bool requirements_met_ = true;

  // Non-owning reference to CrosSettings. This class have shorter lifetime than
  // CrosSettings.
  chromeos::CrosSettings* cros_settings_;

  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      policy_subscription_;

  // List of registered observers.
  base::ObserverList<Observer>::Unchecked observers_;

  base::WeakPtrFactory<MinimumVersionPolicyHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MinimumVersionPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_MINIMUM_VERSION_POLICY_HANDLER_H_

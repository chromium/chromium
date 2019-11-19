// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CALCULATORS_POLICIES_BINDER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CALCULATORS_POLICIES_BINDER_H_

#include <memory>

#include "base/macros.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {

class CrosSettings;

// Observes device settings & user profile modifications and propagates them to
// BulkPrintersCalculator objects associated with given device context and user
// profile. All methods must be called from the same sequence (UI).
class CalculatorsPoliciesBinder {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // |settings| is the source of device policies. |profile| is a user profile.
  static std::unique_ptr<CalculatorsPoliciesBinder> Create(
      CrosSettings* settings,
      Profile* profile);
  virtual ~CalculatorsPoliciesBinder() = default;

 protected:
  CalculatorsPoliciesBinder() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(CalculatorsPoliciesBinder);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CALCULATORS_POLICIES_BINDER_H_

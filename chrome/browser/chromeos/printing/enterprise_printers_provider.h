// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_ENTERPRISE_PRINTERS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_ENTERPRISE_PRINTERS_PROVIDER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {

class CrosSettings;
class Printer;

// Uses classes BulkPrintersCalculator and CalculatorsPoliciesBinder to track
// device settings & user profile modifications and to calculates resultant
// list of available enterprise printers. The final list of available enterprise
// printers is propagated to Observers.
// All methods must be called from the same sequence (UI) and all observers'
// notifications will be called from this sequence.
class EnterprisePrintersProvider {
 public:
  class Observer {
   public:
    // |complete| is true if all policies have been parsed and applied (even
    // when parsing errors occurred), false means that a new list of available
    // printers is being calculated. |printers| contains the current list of
    // available printers: the map is indexed by printers ids. This
    // notification is called when value of any of these two parameters changes.
    virtual void OnPrintersChanged(
        bool complete,
        const std::unordered_map<std::string, Printer>& printers) = 0;
  };

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // |settings| is the source of device policies. |profile| is a user profile.
  static std::unique_ptr<EnterprisePrintersProvider> Create(
      CrosSettings* settings,
      Profile* profile);
  virtual ~EnterprisePrintersProvider() = default;

  // This method also calls directly OnPrintersChanged(...) from |observer|.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  EnterprisePrintersProvider() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(EnterprisePrintersProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_ENTERPRISE_PRINTERS_PROVIDER_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_CALCULATORS_POLICIES_BINDER_H_
#define CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_CALCULATORS_POLICIES_BINDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/printing/enterprise/bulk_printers_calculator.h"

class PrefService;
class PrefRegistrySimple;

namespace ash {

class CrosSettings;

// Observes device settings & user profile modifications and propagates them to
// BulkPrintersCalculator objects associated with given device context and user
// profile. All methods must be called from the same sequence (UI).
class CalculatorsPoliciesBinder {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Binds events from |settings| to the appropriate fields in |calculator|.
  static std::unique_ptr<CalculatorsPoliciesBinder> DeviceBinder(
      CrosSettings* settings,
      base::WeakPtr<BulkPrintersCalculator> calculator);

  // Binds events from |profile| to the appropriate fields in |calculator|.
  static std::unique_ptr<CalculatorsPoliciesBinder> UserBinder(
      PrefService* prefs,
      base::WeakPtr<BulkPrintersCalculator> calculator);

  CalculatorsPoliciesBinder(const CalculatorsPoliciesBinder&) = delete;
  CalculatorsPoliciesBinder& operator=(const CalculatorsPoliciesBinder&) =
      delete;

  virtual ~CalculatorsPoliciesBinder();

 protected:
  // |access_mode_name| is the name of the access mode policy.  |blocklist_name|
  // is the name of the blocklist policy.  |allowlist_name| is the name of the
  // allowlist policy.  |calculator| will receive updates from the bound
  // policies.
  CalculatorsPoliciesBinder(const char* access_mode_name,
                            const char* blocklist_name,
                            const char* allowlist_name,
                            base::WeakPtr<BulkPrintersCalculator> calculator);

  // Returns a WeakPtr to the Derived class.
  base::WeakPtr<CalculatorsPoliciesBinder> GetWeakPtr();

  // Binds |policy_name| to |callback| for each policy name, using the
  // appropriate preference system.
  virtual void Bind(const char* policy_name,
                    base::RepeatingClosure callback) = 0;

  // Returns the access mode integer preference for |name|.
  virtual int GetAccessMode(const char* name) const = 0;

  // Returns a string list for the prefrence |name|.
  virtual std::vector<std::string> GetStringList(const char* name) const = 0;

 private:
  // Attaches bindings since they cannot be safely bound in the constructor.
  void Init();

  void UpdateAccessMode();
  void UpdateAllowlist();
  void UpdateBlocklist();

  const char* access_mode_name_;
  const char* blocklist_name_;
  const char* allowlist_name_;
  base::WeakPtr<BulkPrintersCalculator> calculator_;
  base::WeakPtrFactory<CalculatorsPoliciesBinder> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_CALCULATORS_POLICIES_BINDER_H_

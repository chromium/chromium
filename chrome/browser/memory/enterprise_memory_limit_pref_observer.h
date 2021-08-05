// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_ENTERPRISE_MEMORY_LIMIT_PREF_OBSERVER_H_
#define CHROME_BROWSER_MEMORY_ENTERPRISE_MEMORY_LIMIT_PREF_OBSERVER_H_

#include "base/sequence_checker.h"
#include "chrome/browser/memory/enterprise_memory_limit_evaluator.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace memory {

// Tracks changes to the TotalMemoryLimitMb policy and starts/stops/updates
// the limit of its EnterpriseMemoryLimitEvaluator accordingly.
class EnterpriseMemoryLimitPrefObserver {
 public:
  explicit EnterpriseMemoryLimitPrefObserver(PrefService* pref_service);
  ~EnterpriseMemoryLimitPrefObserver();

  // Returns true if the current platform is supported, false otherwise.
  static bool PlatformIsSupported();

  // Registers the TotalMemoryLimitMb pref with the provided PrefRegistry.
  // Should only be called by RegisterLocalState() in
  // chrome/browser/prefs/browser_prefs.cc.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // Gets the value of the TotalMemoryLimitMb preference and updates the
  // evaluator accordingly.
  void GetPref();

  // PrefService from which we get the preference upon notification that the
  // preference has changed.
  PrefService* pref_service_;

  // Registrar which notifies us of changes to the preference.
  PrefChangeRegistrar pref_change_registrar_;

  // Evaluator which sends its votes to the MultiSourceMemoryPressureMonitor.
  // Should only be running when the preference is enabled.
  std::unique_ptr<EnterpriseMemoryLimitEvaluator> evaluator_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_ENTERPRISE_MEMORY_LIMIT_PREF_OBSERVER_H_

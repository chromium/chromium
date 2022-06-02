// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HIGH_EFFICIENCY_MODE_POLICY_HELPER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HIGH_EFFICIENCY_MODE_POLICY_HELPER_H_

#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace performance_manager::policies {

class HighEfficiencyModePolicyHelper {
 public:
  explicit HighEfficiencyModePolicyHelper(PrefService* local_state);

 private:
  void OnHighEfficiencyModeChanged();
  void OnBatterySaverModeChanged();

  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HIGH_EFFICIENCY_MODE_POLICY_HELPER_H_

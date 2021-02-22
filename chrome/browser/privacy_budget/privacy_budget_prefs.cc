// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace prefs {

const char kPrivacyBudgetActiveSurfaces[] = "privacy_budget.active_surfaces";
const char kPrivacyBudgetRetiredSurfaces[] = "privacy_budget.retired_surfaces";
const char kPrivacyBudgetSeed[] = "privacy_budget.randomizer_seed";
const char kPrivacyBudgetGeneration[] = "privacy_budget.generation";

void RegisterPrivacyBudgetPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kPrivacyBudgetActiveSurfaces, std::string());
  registry->RegisterStringPref(kPrivacyBudgetRetiredSurfaces, std::string());
  registry->RegisterUint64Pref(kPrivacyBudgetSeed, 0u);
  registry->RegisterIntegerPref(kPrivacyBudgetGeneration, 0);
}

}  // namespace prefs

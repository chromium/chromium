// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_PREFS_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_PREFS_H_

#include "components/prefs/pref_registry_simple.h"

namespace prefs {

// See documentation for IdentifiabilityStudyState for details on how these
// values are used.

// Pref used for persisting |IdentifiabilityStudyState::active_surfaces_|.
//
// Value is a list of IdentifiableSurface encoded via
// EncodeIdentifiabilityFieldTrialParam<> into a StringPref. Use
// DecodeIdentifiabilityFieldTrialParam<IdentifiableSurface> to read.
extern const char kPrivacyBudgetActiveSurfaces[];

// Pref used for persisting |IdentifiabilityStudyState::retired_surfaces_|.
//
// Value is a list of IdentifiableSurface encoded via
// EncodeIdentifiabilityFieldTrialParam<> into a StringPref. Use
// DecodeIdentifiabilityFieldTrialParam<IdentifiableSurface> to read.
extern const char kPrivacyBudgetRetiredSurfaces[];

// Pref used for persisting |IdentifiabilityStudyState::prng_seed_|.
//
// Value is an uint64_t stored as Uint64Pref.
extern const char kPrivacyBudgetSeed[];

// Pref used for persisting |IdentifiabilityStudyState::generation_|.
//
// Value is an int stored as IntegerPref.
extern const char kPrivacyBudgetGeneration[];

void RegisterPrivacyBudgetPrefs(PrefRegistrySimple* registry);

}  // namespace prefs

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_PREFS_H_

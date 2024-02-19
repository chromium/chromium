// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_PREFS_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_PREFS_H_

#include "components/prefs/pref_registry_simple.h"

namespace prefs {

// See documentation for IdentifiabilityStudyState for details on how these
// values are used.

// Pref used for persisting |IdentifiabilityStudyState::generation_|.
//
// Value is an int stored as IntegerPref.
extern const char kPrivacyBudgetGeneration[];

// Pref used for persisting |IdentifiabilityStudyState::seen_surface_sequence_|.
//
// Value is a list of IdentifiableSurface encoded via
// EncodeIdentifiabilityFieldTrialParam<> into a StringPref. Use
// DecodeIdentifiabilityFieldTrialParam<> to read.
//
// The ordering is a significant and represents the order in which the surfaces
// were encountered with the exception of blocked surfaces.
extern const char kPrivacyBudgetSeenSurfaces[];

// Pref used for persisting |IdentifiabilityStudyState::selected_offsets_|.
//
// Value is a list of offsets (>= 0) encoded into a ListPref where each element
// in the list is an integer value.
extern const char kPrivacyBudgetSelectedOffsets[];

// Pref used for persisting the selected block during assigned block sampling.
//
// Value is an integer >= 0.
extern const char kPrivacyBudgetSelectedBlock[];

// Pref used for persisting the random salt which is used to decide whether the
// meta experiment should be active. The meta experiment is active if the salt
// is smaller than the meta experiment selection probability.
//
// Value is a double in the interval [0,1)
extern const char kPrivacyBudgetMetaExperimentActivationSalt[];

void RegisterPrivacyBudgetPrefs(PrefRegistrySimple* registry);

}  // namespace prefs

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_PREFS_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_MANAGEMENT_DISCLOSURE_FIELD_TRIAL_H_
#define ASH_LOGIN_UI_MANAGEMENT_DISCLOSURE_FIELD_TRIAL_H_

#include "ash/ash_export.h"
#include "components/variations/entropy_provider.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class FeatureList;
}  // namespace base

namespace ash::management_disclosure_field_trial {

// Registers preferences.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Creates a field trial to control the Improved Management Disclosure feature.
// The trial is client controlled because the disclosure needs to be available
// during the OOBE flow before which shows up before variations seed is
// available.
//
// The trial group chosen on first run is persisted to local state prefs and
// reused on subsequent runs. This keeps the management disclosure UI stable
// between runs. Local state prefs can be reset via powerwash, which will result
// in re-randomization.
ASH_EXPORT void Create(base::FeatureList* feature_list,
                       PrefService* local_state,
                       const variations::EntropyProviders& entropy_providers);

}  // namespace ash::management_disclosure_field_trial

#endif  // ASH_LOGIN_UI_MANAGEMENT_DISCLOSURE_FIELD_TRIAL_H_

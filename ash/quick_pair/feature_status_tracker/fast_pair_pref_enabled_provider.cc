// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/fast_pair_pref_enabled_provider.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/scoped_observation.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace quick_pair {

FastPairPrefEnabledProvider::FastPairPrefEnabledProvider() {
  session_observation_.Observe(Shell::Get()->session_controller());
}

FastPairPrefEnabledProvider::~FastPairPrefEnabledProvider() = default;

// static
void FastPairPrefEnabledProvider::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kFastPairEnabled,
                                /*default_value=*/true);
}

// Only called when there exists a last active user prefs. Caller ensures
// that prefs is never null.
void FastPairPrefEnabledProvider::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  pref_change_registrar_.reset();
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);
  pref_change_registrar_->Add(
      prefs::kFastPairEnabled,
      base::BindRepeating(&FastPairPrefEnabledProvider::OnFastPairPrefChanged,
                          base::Unretained(this)));

  OnFastPairPrefChanged();
}

void FastPairPrefEnabledProvider::OnFastPairPrefChanged() {
  SetEnabledAndInvokeCallback(
      pref_change_registrar_->prefs()->GetBoolean(prefs::kFastPairEnabled));
}

}  // namespace quick_pair
}  // namespace ash
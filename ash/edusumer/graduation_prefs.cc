// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/edusumer/graduation_prefs.h"

#include "ash/constants/ash_pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash::graduation_prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kGraduationEnablementStatus);
  registry->RegisterBooleanPref(prefs::kGraduationNudgeShown, false);
}

}  // namespace ash::graduation_prefs

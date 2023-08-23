// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_prefs.h"

#include "ash/constants/ash_pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

AcceleratorPrefs::AcceleratorPrefs() = default;
AcceleratorPrefs::~AcceleratorPrefs() = default;

// static:
void AcceleratorPrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kShortcutCustomizationAllowed, true);
}

}  // namespace ash

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_util.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

PrefService* GetPrimaryUserPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

}  // namespace

namespace desks_templates_util {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDeskTemplatesEnabled, false);
}

bool AreDesksTemplatesEnabled() {
  // Always allow the feature to be used if it is explicitly enabled. Otherwise
  // check the policy.
  if (features::AreDesksTemplatesEnabled())
    return true;

  PrefService* pref_service = GetPrimaryUserPrefService();
  return pref_service && pref_service->GetBoolean(prefs::kDeskTemplatesEnabled);
}

}  // namespace desks_templates_util
}  // namespace ash

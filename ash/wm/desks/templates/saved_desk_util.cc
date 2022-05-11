// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_util.h"

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

namespace saved_desk_util {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDeskTemplatesEnabled, false);
}

bool AreDesksTemplatesEnabled() {
  PrefService* pref_service = GetPrimaryUserPrefService();
  DCHECK(pref_service);

  const PrefService::Preference* desk_templates_pref =
      pref_service->FindPreference(prefs::kDeskTemplatesEnabled);

  DCHECK(desk_templates_pref);

  if (desk_templates_pref->IsManaged()) {
    // Let policy settings override flags configuration.
    return pref_service->GetBoolean(prefs::kDeskTemplatesEnabled);
  }

  // Allow the feature to be enabled by user when there is not explicit
  // policy.
  return features::AreDesksTemplatesEnabled();
}

bool IsDeskSaveAndRecallEnabled() {
  return features::IsSavedDesksEnabled();
}

bool IsSavedDesksEnabled() {
  return AreDesksTemplatesEnabled() || IsDeskSaveAndRecallEnabled();
}

}  // namespace saved_desk_util
}  // namespace ash

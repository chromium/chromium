// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/system_shortcut_behavior_policy.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/prefs/pref_service.h"

namespace ash {

void RegisterSystemShortcutBehaviorProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kSystemShortcutBehavior,
      static_cast<int>(SystemShortcutBehaviorType::kNormalShortcutBehavior));
}

SystemShortcutBehaviorType GetSystemShortcutBehavior() {
  PrefService* pref_service =
      ash::Shell::Get()->session_controller()->GetActivePrefService();
  if (!pref_service) {
    return SystemShortcutBehaviorType::kNormalShortcutBehavior;
  }

  const auto* pref =
      pref_service->FindPreference(ash::prefs::kSystemShortcutBehavior);
  if (!pref || !pref->IsManaged()) {
    return SystemShortcutBehaviorType::kNormalShortcutBehavior;
  }

  auto* value = pref->GetValue();
  CHECK(value && value->is_int());
  const int shortcut_behavior_type = value->GetInt();
  if (shortcut_behavior_type <
          static_cast<int>(SystemShortcutBehaviorType::kMinValue) ||
      shortcut_behavior_type >
          static_cast<int>(SystemShortcutBehaviorType::kMaxValue)) {
    return SystemShortcutBehaviorType::kNormalShortcutBehavior;
  }

  return static_cast<SystemShortcutBehaviorType>(shortcut_behavior_type);
}

}  // namespace ash

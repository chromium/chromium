// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/system_shortcut_behavior_policy.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

SystemShortcutBehaviorType GetSystemShortcutBehaviorFromFlags() {
  if (!base::FeatureList::IsEnabled(features::kSystemShortcutBehavior)) {
    return SystemShortcutBehaviorType::kNormalShortcutBehavior;
  }

  auto system_shortcut_behavior_param =
      features::kSystemShortcutBehaviorParam.Get();
  switch (system_shortcut_behavior_param) {
    case features::SystemShortcutBehaviorParam::kNormalShortcutBehavior:
      return SystemShortcutBehaviorType::kNormalShortcutBehavior;
    case features::SystemShortcutBehaviorParam::kIgnoreCommonVdiShortcutList:
      return SystemShortcutBehaviorType::kIgnoreCommonVdiShortcuts;
    case features::SystemShortcutBehaviorParam::
        kIgnoreCommonVdiShortcutListFullscreenOnly:
      return SystemShortcutBehaviorType::
          kIgnoreCommonVdiShortcutsFullscreenOnly;
    case features::SystemShortcutBehaviorParam::kAllowSearchBasedPassthrough:
      return SystemShortcutBehaviorType::kAllowSearchBasedPassthrough;
    case features::SystemShortcutBehaviorParam::
        kAllowSearchBasedPassthroughFullscreenOnly:
      return SystemShortcutBehaviorType::
          kAllowSearchBasedPassthroughFullscreenOnly;
  }
}

SystemShortcutBehaviorType GetSystemShortcutBehaviorFromPolicy() {
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

}  // namespace

void RegisterSystemShortcutBehaviorProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kSystemShortcutBehavior,
      static_cast<int>(SystemShortcutBehaviorType::kNormalShortcutBehavior));
}

SystemShortcutBehaviorType GetSystemShortcutBehavior() {
  if (auto behavior_from_policy = GetSystemShortcutBehaviorFromPolicy();
      behavior_from_policy !=
      SystemShortcutBehaviorType::kNormalShortcutBehavior) {
    return behavior_from_policy;
  }

  return GetSystemShortcutBehaviorFromFlags();
}

}  // namespace ash

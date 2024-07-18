// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_SYSTEM_SHORTCUT_BEHAVIOR_POLICY_H_
#define ASH_ACCELERATORS_SYSTEM_SHORTCUT_BEHAVIOR_POLICY_H_

#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

enum class SystemShortcutBehaviorType {
  kNormalShortcutBehavior = 0,
  kMinValue = kNormalShortcutBehavior,
  kIgnoreCommonVdiShortcuts = 1,
  kIgnoreCommonVdiShortcutsFullscreenOnly = 2,
  kAllowSearchBasedPassthrough = 3,
  kAllowSearchBasedPassthroughFullscreenOnly = 4,
  kMaxValue = kAllowSearchBasedPassthroughFullscreenOnly,
};

// Registers profile prefs for system shortcuts behavior. This is set by a
// system policy.
void RegisterSystemShortcutBehaviorProfilePrefs(PrefRegistrySimple* registry);

// Get the current value of the `SystemShortcutBehavior` policy.
// Returns `kNormalShortcutBehavior` if the policy is not set.
SystemShortcutBehaviorType GetSystemShortcutBehavior();

}  // namespace ash

#endif  // ASH_ACCELERATORS_SYSTEM_SHORTCUT_BEHAVIOR_POLICY_H_

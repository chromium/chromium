// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_PREFS_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_PREFS_H_

class PrefRegistrySimple;

namespace actor::ui {

inline constexpr char kToastShown[] = "actor.ui.toast_shown";

// Registers actor UI state manager related profile prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_PREFS_H_

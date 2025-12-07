// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_state_manager_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace actor::ui {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // Set number of times toast has been shown to 0.
  registry->RegisterIntegerPref(kToastShown, /*default_value=*/0);
}

}  // namespace actor::ui

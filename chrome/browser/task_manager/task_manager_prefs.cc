// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_prefs.h"

#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace task_manager {

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kTaskManagerWindowPlacement);
  registry->RegisterIntegerPref(prefs::kTaskManagerCategory, 1);
  registry->RegisterDictionaryPref(prefs::kTaskManagerColumnVisibility);
  registry->RegisterBooleanPref(prefs::kTaskManagerEndProcessEnabled, true);
}

bool IsEndProcessEnabled() {
  PrefService* state = g_browser_process->local_state();
  return !state || state->GetBoolean(prefs::kTaskManagerEndProcessEnabled);
}

}  // namespace task_manager

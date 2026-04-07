// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_for_testing/prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace chrome_for_testing {

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kEnableUserEducationUI, false);
  registry->RegisterBooleanPref(prefs::kEnableSearchEngineChoiceDialog, false);
  registry->RegisterBooleanPref(prefs::kEnableVirtualClipboard, false);
}

}  // namespace chrome_for_testing

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_prefs.h"

#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"

namespace contextual_cueing::prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kZeroStateSuggestionsSupportedTools);
}

}  // namespace contextual_cueing::prefs

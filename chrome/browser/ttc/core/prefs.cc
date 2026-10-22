// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace ttc {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kTtcFreCompleted, false);
}

}  // namespace ttc

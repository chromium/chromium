// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace indigo::prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kIndigoPolicy, kAllowed);
  registry->RegisterBooleanPref(kIndigoHasOnboarded, false);
}

}  // namespace indigo::prefs

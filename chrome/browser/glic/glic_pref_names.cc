// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_pref_names.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace glic::prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kGlicPinnedToTabstrip, true);
  registry->RegisterBooleanPref(kGlicMicrophoneEnabled, false);
  registry->RegisterBooleanPref(kGlicGeolocationEnabled, false);
  registry->RegisterBooleanPref(kGlicTabContextEnabled, false);
  registry->RegisterBooleanPref(kGlicCompletedFre, false);
}

}  // namespace glic::prefs

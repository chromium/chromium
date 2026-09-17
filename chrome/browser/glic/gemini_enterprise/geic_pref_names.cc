// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_pref_names.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace geic::prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // These prefs are intentionally NOT synced: they represent per-device UI
  // state, so they must not follow the user across profiles/devices. Omitting
  // the flags argument registers them with NO_REGISTRATION_FLAGS; do not add
  // SYNCABLE_PREF or SYNCABLE_PRIORITY_PREF here.
  registry->RegisterBooleanPref(kGeicPinnedToTabstrip, true);
}

}  // namespace geic::prefs

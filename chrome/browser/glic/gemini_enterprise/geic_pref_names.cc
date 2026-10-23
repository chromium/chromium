// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_pref_names.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace geic::prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kGeicCompletedFre);
}

}  // namespace geic::prefs

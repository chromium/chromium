// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_syncable_util.h"

#include <utility>

#include "chrome/browser/prefs/pref_service_incognito_allowlist.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync_preferences/pref_service_syncable.h"


sync_preferences::PrefServiceSyncable* PrefServiceSyncableFromProfile(
    Profile* profile) {
  return static_cast<sync_preferences::PrefServiceSyncable*>(
      profile->GetPrefs());
}

std::unique_ptr<sync_preferences::PrefServiceSyncable>
CreateIncognitoPrefServiceSyncable(
    sync_preferences::PrefServiceSyncable* pref_service,
    PrefStore* incognito_extension_pref_store) {
  return pref_service->CreateIncognitoPrefService(
      incognito_extension_pref_store,
      prefs::GetIncognitoPersistentPrefsAllowlist());
}

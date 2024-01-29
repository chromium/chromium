// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_settings.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"

const char DevToolsSettings::kSyncDevToolsPreferencesFrontendName[] =
    "sync-preferences";
const bool DevToolsSettings::kSyncDevToolsPreferencesDefault = false;

DevToolsSettings::DevToolsSettings(Profile* profile) : profile_(profile) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kDevToolsSyncPreferences,
      base::BindRepeating(&DevToolsSettings::DevToolsSyncPreferencesChanged,
                          base::Unretained(this)));
}

DevToolsSettings::~DevToolsSettings() = default;

void DevToolsSettings::Register(const std::string& name,
                                const RegisterOptions& options) {
  // kSyncDevToolsPreferenceFrontendName is not stored in any of the relevant
  // dictionaries. Skip registration.
  if (name == kSyncDevToolsPreferencesFrontendName)
    return;

  if (options.sync_mode == RegisterOptions::SyncMode::kSync) {
    synced_setting_names_.insert(name);
  }

  // Setting might have had a different sync status in the past. Move the
  // setting to the correct dictionary.
  PrefService* prefs = profile_->GetPrefs();
  const char* dictionary_to_remove_from =
      options.sync_mode == RegisterOptions::SyncMode::kSync
          ? prefs::kDevToolsPreferences
          : GetDictionaryNameForSyncedPrefs();
  const std::string* settings_value =
      prefs->GetDict(dictionary_to_remove_from).FindString(name);
  if (!settings_value) {
    return;
  }

  const char* dictionary_to_insert_into =
      GetDictionaryNameForSettingsName(name);
  // Settings already moved to the synced dictionary on a different device have
  // precedence.
  const std::string* already_synced_value =
      prefs->GetDict(dictionary_to_insert_into).FindString(name);
  if (dictionary_to_insert_into == prefs::kDevToolsPreferences ||
      !already_synced_value) {
    ScopedDictPrefUpdate insert_update(profile_->GetPrefs(),
                                       dictionary_to_insert_into);
    insert_update->Set(name, *settings_value);
  }

  ScopedDictPrefUpdate remove_update(profile_->GetPrefs(),
                                     dictionary_to_remove_from);
  remove_update->Remove(name);
}

base::Value::Dict DevToolsSettings::Get() {
  base::Value::Dict settings;

  PrefService* prefs = profile_->GetPrefs();
  // DevTools expects any kind of preference to be a string. Parsing is
  // happening on the frontend.
  settings.Set(
      kSyncDevToolsPreferencesFrontendName,
      prefs->GetBoolean(prefs::kDevToolsSyncPreferences) ? "true" : "false");
  settings.Merge(prefs->GetDict(prefs::kDevToolsPreferences).Clone());
  settings.Merge(prefs->GetDict(GetDictionaryNameForSyncedPrefs()).Clone());

  return settings;
}

std::optional<base::Value> DevToolsSettings::Get(const std::string& name) {
  PrefService* prefs = profile_->GetPrefs();
  if (name == kSyncDevToolsPreferencesFrontendName) {
    // DevTools expects any kind of preference to be a string. Parsing is
    // happening on the frontend.
    bool result = prefs->GetBoolean(prefs::kDevToolsSyncPreferences);
    return base::Value(result ? "true" : "false");
  }
  const char* dict_name = GetDictionaryNameForSettingsName(name);
  const base::Value::Dict& dict = prefs->GetDict(dict_name);
  const base::Value* value = dict.Find(name);
  return value ? std::optional<base::Value>(value->Clone()) : std::nullopt;
}

void DevToolsSettings::Set(const std::string& name, const std::string& value) {
  if (name == kSyncDevToolsPreferencesFrontendName) {
    profile_->GetPrefs()->SetBoolean(prefs::kDevToolsSyncPreferences,
                                     value == "true");
    return;
  }

  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              GetDictionaryNameForSettingsName(name));
  update->Set(name, value);
}

void DevToolsSettings::Remove(const std::string& name) {
  if (name == kSyncDevToolsPreferencesFrontendName) {
    profile_->GetPrefs()->SetBoolean(prefs::kDevToolsSyncPreferences,
                                     kSyncDevToolsPreferencesDefault);
    return;
  }

  PrefService* prefs = profile_->GetPrefs();
  for (auto* dict_name :
       {GetDictionaryNameForSyncedPrefs(), prefs::kDevToolsPreferences}) {
    const base::Value::Dict& dict = prefs->GetDict(dict_name);
    if (dict.Find(name)) {
      ScopedDictPrefUpdate update(profile_->GetPrefs(), dict_name);
      update->Remove(name);
    }
  }
}

void DevToolsSettings::Clear() {
  profile_->GetPrefs()->SetBoolean(prefs::kDevToolsSyncPreferences,
                                   kSyncDevToolsPreferencesDefault);
  profile_->GetPrefs()->SetDict(prefs::kDevToolsPreferences,
                                base::Value::Dict());
  profile_->GetPrefs()->SetDict(prefs::kDevToolsSyncedPreferencesSyncEnabled,
                                base::Value::Dict());
  profile_->GetPrefs()->SetDict(prefs::kDevToolsSyncedPreferencesSyncDisabled,
                                base::Value::Dict());
}

const char* DevToolsSettings::GetDictionaryNameForSettingsName(
    const std::string& name) const {
  return synced_setting_names_.contains(name)
             ? GetDictionaryNameForSyncedPrefs()
             : prefs::kDevToolsPreferences;
}

const char* DevToolsSettings::GetDictionaryNameForSyncedPrefs() const {
  const bool isDevToolsSyncEnabled =
      profile_->GetPrefs()->GetBoolean(prefs::kDevToolsSyncPreferences);
  return isDevToolsSyncEnabled ? prefs::kDevToolsSyncedPreferencesSyncEnabled
                               : prefs::kDevToolsSyncedPreferencesSyncDisabled;
}

void DevToolsSettings::DevToolsSyncPreferencesChanged() {
  // There are two cases to handle:
  //
  // Sync was enabled: We assume this was triggered by the user in the local
  // DevTools session as opposed to synced from a different device. As such, the
  // local settings have precedence when merging. The unsynced dictonary is
  // cleared.
  //
  // Sync was disabled: As kDevToolsSyncPreferences is synced itself we can
  // clear the synced dictionary after copying to the unsynced one. The unsynced
  // dictionary is empty.
  //
  // Considering the points above, the implementation between both cases can be
  // shared modulo the source/target dictionary.
  const bool sync_enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kDevToolsSyncPreferences);
  const char* target_dictionary =
      sync_enabled ? prefs::kDevToolsSyncedPreferencesSyncEnabled
                   : prefs::kDevToolsSyncedPreferencesSyncDisabled;
  const char* source_dictionary =
      sync_enabled ? prefs::kDevToolsSyncedPreferencesSyncDisabled
                   : prefs::kDevToolsSyncedPreferencesSyncEnabled;

  ScopedDictPrefUpdate source_update(profile_->GetPrefs(), source_dictionary);
  ScopedDictPrefUpdate target_update(profile_->GetPrefs(), target_dictionary);
  base::Value::Dict source_dict;
  std::swap(source_dict, *source_update);
  target_update->Merge(std::move(source_dict));
}

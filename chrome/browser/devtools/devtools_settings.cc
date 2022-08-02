// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_settings.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"

const char DevToolsSettings::kSyncDevToolsPreferencesFrontendName[] =
    "sync_preferences";
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
      prefs->GetValueDict(dictionary_to_remove_from).FindString(name);
  if (!settings_value) {
    return;
  }

  const char* dictionary_to_insert_into =
      GetDictionaryNameForSettingsName(name);
  // Settings already moved to the synced dictionary on a different device have
  // precedence.
  const std::string* already_synced_value =
      prefs->GetValueDict(dictionary_to_insert_into).FindString(name);
  if (dictionary_to_insert_into == prefs::kDevToolsPreferences ||
      !already_synced_value) {
    DictionaryPrefUpdate insert_update(profile_->GetPrefs(),
                                       dictionary_to_insert_into);
    insert_update.Get()->SetStringKey(name, *settings_value);
  }

  DictionaryPrefUpdate remove_update(profile_->GetPrefs(),
                                     dictionary_to_remove_from);
  remove_update.Get()->RemoveKey(name);
}

base::Value DevToolsSettings::Get() {
  base::Value::Dict settings;

  PrefService* prefs = profile_->GetPrefs();
  // DevTools expects any kind of preference to be a string. Parsing is
  // happening on the frontend.
  settings.Set(
      kSyncDevToolsPreferencesFrontendName,
      prefs->GetBoolean(prefs::kDevToolsSyncPreferences) ? "true" : "false");
  settings.Merge(prefs->GetValueDict(prefs::kDevToolsPreferences).Clone());
  settings.Merge(
      prefs->GetValueDict(GetDictionaryNameForSyncedPrefs()).Clone());

  return base::Value(std::move(settings));
}

absl::optional<base::Value> DevToolsSettings::Get(const std::string& name) {
  PrefService* prefs = profile_->GetPrefs();
  if (name == kSyncDevToolsPreferencesFrontendName) {
    // DevTools expects any kind of preference to be a string. Parsing is
    // happening on the frontend.
    bool result = prefs->GetBoolean(prefs::kDevToolsSyncPreferences);
    return base::Value(result ? "true" : "false");
  }
  const char* dict_name = GetDictionaryNameForSettingsName(name);
  const base::Value::Dict& dict = prefs->GetValueDict(dict_name);
  const base::Value* value = dict.Find(name);
  return value ? absl::optional<base::Value>(value->Clone()) : absl::nullopt;
}

void DevToolsSettings::Set(const std::string& name, const std::string& value) {
  if (name == kSyncDevToolsPreferencesFrontendName) {
    profile_->GetPrefs()->SetBoolean(prefs::kDevToolsSyncPreferences,
                                     value == "true");
    return;
  }

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              GetDictionaryNameForSettingsName(name));
  update.Get()->SetStringKey(name, value);
}

void DevToolsSettings::Remove(const std::string& name) {
  if (name == kSyncDevToolsPreferencesFrontendName) {
    profile_->GetPrefs()->SetBoolean(prefs::kDevToolsSyncPreferences,
                                     kSyncDevToolsPreferencesDefault);
    return;
  }

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              GetDictionaryNameForSettingsName(name));
  update.Get()->RemoveKey(name);
}

void DevToolsSettings::Clear() {
  profile_->GetPrefs()->SetBoolean(prefs::kDevToolsSyncPreferences,
                                   kSyncDevToolsPreferencesDefault);
  DictionaryPrefUpdate unsynced_update(profile_->GetPrefs(),
                                       prefs::kDevToolsPreferences);
  unsynced_update.Get()->DictClear();
  DictionaryPrefUpdate sync_enabled_update(
      profile_->GetPrefs(), prefs::kDevToolsSyncedPreferencesSyncEnabled);
  sync_enabled_update.Get()->DictClear();
  DictionaryPrefUpdate sync_disabled_update(
      profile_->GetPrefs(), prefs::kDevToolsSyncedPreferencesSyncDisabled);
  sync_disabled_update.Get()->DictClear();
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

  DictionaryPrefUpdate target_update(profile_->GetPrefs(), target_dictionary);
  target_update.Get()->MergeDictionary(
      // This should be `PrefService::GetValueDict`, but
      // `DictionaryPrefUpdate::Get()` currently yields a `base::Value*`.
      &profile_->GetPrefs()->GetValue(source_dictionary));
  DictionaryPrefUpdate source_update(profile_->GetPrefs(), source_dictionary);
  source_update.Get()->DictClear();
}

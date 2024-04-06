// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_SETTINGS_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_SETTINGS_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace base {
class Value;
}

struct RegisterOptions {
  enum class SyncMode {
    kSync,
    kDontSync,
  };
  SyncMode sync_mode;
};

class DevToolsSettings {
 public:
  // The frontend setting name that mirrors prefs::kDevToolsSyncPreferences.
  static const char kSyncDevToolsPreferencesFrontendName[];
  static const bool kSyncDevToolsPreferencesDefault;

  explicit DevToolsSettings(Profile* profile);
  ~DevToolsSettings();

  void Register(const std::string& name, const RegisterOptions& options);
  base::Value::Dict Get();
  std::optional<base::Value> Get(const std::string& name);
  void Set(const std::string& name, const std::string& value);
  void Remove(const std::string& name);
  void Clear();

 private:
  const char* GetDictionaryNameForSettingsName(const std::string& name) const;
  const char* GetDictionaryNameForSyncedPrefs() const;
  void DevToolsSyncPreferencesChanged();

  const raw_ptr<Profile> profile_;

  // Contains the set of synced settings.
  // The DevTools frontend *must* call `Register` for each setting prior to
  // use, which makes persisting this set unnecessary.
  base::flat_set<std::string> synced_setting_names_;

  // Settings pref observer that moves synced settings between their two
  // dictionaries depending on the state of kDevToolsSyncPreferences.
  // kDevToolsSyncPreferences is only changed via the frontend, so it is
  // sufficient to only listen for changes during a DevTools session.
  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_SETTINGS_H_

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_settings.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"

DevToolsSettings::DevToolsSettings(Profile* profile) : profile_(profile) {}

const base::Value* DevToolsSettings::Get() {
  return profile_->GetPrefs()->GetDictionary(prefs::kDevToolsPreferences);
}

void DevToolsSettings::Set(const std::string& name, const std::string& value) {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsPreferences);
  update.Get()->SetKey(name, base::Value(value));
}

void DevToolsSettings::Remove(const std::string& name) {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsPreferences);
  update.Get()->RemoveKey(name);
}

void DevToolsSettings::Clear() {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsPreferences);
  update.Get()->Clear();
}

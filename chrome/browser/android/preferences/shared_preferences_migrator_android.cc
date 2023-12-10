// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/shared_preferences_migrator_android.h"

#include <optional>
#include <string>

#include "base/android/shared_preferences/shared_preferences_manager.h"
#include "chrome/browser/preferences/android/chrome_shared_preferences.h"

using base::android::SharedPreferencesManager;

namespace android::shared_preferences {

void ClearKey(const std::string& shared_preference_key) {
  SharedPreferencesManager shared_prefs = GetChromeSharedPreferences();
  shared_prefs.RemoveKey(shared_preference_key);
}

std::optional<bool> GetAndClearBoolean(
    const std::string& shared_preference_key) {
  SharedPreferencesManager shared_prefs = GetChromeSharedPreferences();

  if (!shared_prefs.ContainsKey(shared_preference_key)) {
    return std::nullopt;
  }

  bool result =
      shared_prefs.ReadBoolean(shared_preference_key, /*default_value=*/false);
  shared_prefs.RemoveKey(shared_preference_key);
  return result;
}

std::optional<int> GetAndClearInt(const std::string& shared_preference_key) {
  SharedPreferencesManager shared_prefs = GetChromeSharedPreferences();

  if (!shared_prefs.ContainsKey(shared_preference_key)) {
    return std::nullopt;
  }

  int result = shared_prefs.ReadInt(shared_preference_key, /*default_value=*/0);
  shared_prefs.RemoveKey(shared_preference_key);
  return result;
}

std::string GetString(const std::string& shared_preference_key,
                      const std::string& default_value) {
  SharedPreferencesManager shared_prefs = GetChromeSharedPreferences();
  return shared_prefs.ReadString(shared_preference_key, default_value);
}

void SetString(const std::string& shared_preference_key,
               const std::string& value) {
  SharedPreferencesManager shared_prefs = GetChromeSharedPreferences();
  shared_prefs.WriteString(shared_preference_key, value);
}

}  // namespace android::shared_preferences

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_prefs_manager.h"

#include <algorithm>
#include <initializer_list>

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace safe_browsing {

SettingsResetPromptPrefsManager::SettingsResetPromptPrefsManager(
    Profile* profile,
    int prompt_wave)
    : profile_(profile), prefs_(profile->GetPrefs()) {
  DCHECK(profile_);
  DCHECK(prefs_);

  // If we are in a new prompt_wave, clear previous prefs.
  int prefs_prompt_wave =
      prefs_->GetInteger(prefs::kSettingsResetPromptPromptWave);
  if (prompt_wave != prefs_prompt_wave) {
    prefs_->SetInteger(prefs::kSettingsResetPromptPromptWave, prompt_wave);
    prefs_->ClearPref(prefs::kSettingsResetPromptLastTriggeredForDefaultSearch);
    prefs_->ClearPref(prefs::kSettingsResetPromptLastTriggeredForStartupUrls);
    prefs_->ClearPref(prefs::kSettingsResetPromptLastTriggeredForHomepage);
  }
}

SettingsResetPromptPrefsManager::~SettingsResetPromptPrefsManager() {}

// static
void SettingsResetPromptPrefsManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  DCHECK(registry);
  registry->RegisterIntegerPref(prefs::kSettingsResetPromptPromptWave, 0);
  registry->RegisterInt64Pref(
      prefs::kSettingsResetPromptLastTriggeredForDefaultSearch, 0);
  registry->RegisterInt64Pref(
      prefs::kSettingsResetPromptLastTriggeredForStartupUrls, 0);
  registry->RegisterInt64Pref(
      prefs::kSettingsResetPromptLastTriggeredForHomepage, 0);
}

base::Time SettingsResetPromptPrefsManager::LastTriggeredPrompt() const {
  return std::max({LastTriggeredPromptForDefaultSearch(),
                   LastTriggeredPromptForStartupUrls(),
                   LastTriggeredPromptForHomepage()});
}

base::Time
SettingsResetPromptPrefsManager::LastTriggeredPromptForDefaultSearch() const {
  return base::Time::FromInternalValue(prefs_->GetInt64(
      prefs::kSettingsResetPromptLastTriggeredForDefaultSearch));
}

base::Time SettingsResetPromptPrefsManager::LastTriggeredPromptForStartupUrls()
    const {
  return base::Time::FromInternalValue(
      prefs_->GetInt64(prefs::kSettingsResetPromptLastTriggeredForStartupUrls));
}

base::Time SettingsResetPromptPrefsManager::LastTriggeredPromptForHomepage()
    const {
  return base::Time::FromInternalValue(
      prefs_->GetInt64(prefs::kSettingsResetPromptLastTriggeredForHomepage));
}

void SettingsResetPromptPrefsManager::RecordPromptShownForDefaultSearch(
    const base::Time& prompt_time) {
  prefs_->SetInt64(prefs::kSettingsResetPromptLastTriggeredForDefaultSearch,
                   prompt_time.ToInternalValue());
}

void SettingsResetPromptPrefsManager::RecordPromptShownForStartupUrls(
    const base::Time& prompt_time) {
  prefs_->SetInt64(prefs::kSettingsResetPromptLastTriggeredForStartupUrls,
                   prompt_time.ToInternalValue());
}

void SettingsResetPromptPrefsManager::RecordPromptShownForHomepage(
    const base::Time& prompt_time) {
  prefs_->SetInt64(prefs::kSettingsResetPromptLastTriggeredForHomepage,
                   prompt_time.ToInternalValue());
}

}  // namespace safe_browsing

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_PREFS_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_PREFS_MANAGER_H_

#include "base/time/time.h"

class Profile;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace safe_browsing {

// Class responsible for reading and updating the preferences related to the
// settings reset prompt.
class SettingsResetPromptPrefsManager {
 public:
  // |prompt_wave| should be set to the prompt wave parameter obtained from the
  // |SettingsResetPromptConfig| class. If a new prompt wave has been started
  // (i.e., if the |prompt_wave| passed in to the constructor is greater than
  // the one stored in preferences), other related settings in preferences will
  // be reset.
  SettingsResetPromptPrefsManager(Profile* profile, int prompt_wave);
  ~SettingsResetPromptPrefsManager();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  base::Time LastTriggeredPrompt() const;
  base::Time LastTriggeredPromptForDefaultSearch() const;
  base::Time LastTriggeredPromptForStartupUrls() const;
  base::Time LastTriggeredPromptForHomepage() const;

  void RecordPromptShownForDefaultSearch(const base::Time& prompt_time);
  void RecordPromptShownForStartupUrls(const base::Time& prompt_time);
  void RecordPromptShownForHomepage(const base::Time& prompt_time);

 private:
  Profile* const profile_;
  PrefService* const prefs_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_PREFS_MANAGER_H_

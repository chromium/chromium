// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_UTIL_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_UTIL_WIN_H_

namespace safe_browsing {

// Function to be called after startup in order to display the settings reset
// prompt. The function will figure out if a prompt is needed, and if so, show
// the dialog after a delay as determined by the |kSettingsResetPrompt|
// feature parameters.
void MaybeShowSettingsResetPromptWithDelay();

// Delegate for MaybeShowSettingsResetPromptWithDelay() that can be overriden
// by tests that only want to check if the flow for the settings reset prompt
// will be initiated.
class SettingsResetPromptDelegate {
 public:
  SettingsResetPromptDelegate();

  SettingsResetPromptDelegate(const SettingsResetPromptDelegate&) = delete;
  SettingsResetPromptDelegate& operator=(const SettingsResetPromptDelegate&) =
      delete;

  virtual ~SettingsResetPromptDelegate();

  virtual void ShowSettingsResetPromptWithDelay() const = 0;
};

// Sets the global SettingsResetPromptDelegate, usually for testing.
void SetSettingsResetPromptDelegate(SettingsResetPromptDelegate* delegate);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_UTIL_WIN_H_

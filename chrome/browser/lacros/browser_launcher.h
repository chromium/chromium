// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_BROWSER_LAUNCHER_H_
#define CHROME_BROWSER_LACROS_BROWSER_LAUNCHER_H_

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"

class Profile;

// Responsible for creating and launching new instances of lacros-chrome.
class BrowserLauncher : public base::SupportsUserData::Data {
 public:
  explicit BrowserLauncher(Profile* profile);
  BrowserLauncher(const BrowserLauncher&) = delete;
  BrowserLauncher& operator=(const BrowserLauncher&) = delete;
  ~BrowserLauncher() override = default;

  // Creates the BrowserLauncher instance if it does not already exist.
  static BrowserLauncher* GetForProfile(Profile* profile);

  // Launches lacros-chrome for the last opened profiles. Returns true if
  // BrowserLauncher was able to perform the launch action successfully.
  // `restore_tabbed_browser` should only be flipped false by Ash full
  // restore code path, suppressing restoring a normal browser when there were
  // only PWAs open in previous session. See crbug.com/1463906.
  bool LaunchForLastOpenedProfiles(bool skip_crash_restore,
                                   bool restore_tabbed_browser);

  bool is_launching_for_last_opened_profiles() const {
    return is_launching_for_last_opened_profiles_;
  }

 private:
  // Tracks whether the BrowserLauncher is in the process of launching
  // lacros-chrome for the last opened profiles.
  bool is_launching_for_last_opened_profiles_ = false;

  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_LACROS_BROWSER_LAUNCHER_H_

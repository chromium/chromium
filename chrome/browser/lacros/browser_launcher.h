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

  // Launches lacros-chrome for full restore. This method should be called
  // before any lacros browser windows have been created while the process is in
  // the background / windowless state.
  void LaunchForFullRestore(bool skip_crash_restore);

  bool is_launching_for_full_restore() const {
    return is_launching_for_full_restore_;
  }

 private:
  // Tracks whether the BrowserLauncher is in the process of performing a Full
  // Restore launch of lacros-chrome.
  bool is_launching_for_full_restore_ = false;

  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_LACROS_BROWSER_LAUNCHER_H_

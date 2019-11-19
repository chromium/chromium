// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_H_
#define CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {
namespace first_run {

// Registers preferences related to ChromeOS first-run tutorial.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Probably launches first-run dialog after session start depending on synced
// user prefs. This method should be called after user already logged in but
// session didn't started yet.
void MaybeLaunchDialogAfterSessionStart();

// Launches overlay tutorial for current user.
void LaunchTutorial();

}  // namespace first_run
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_H_

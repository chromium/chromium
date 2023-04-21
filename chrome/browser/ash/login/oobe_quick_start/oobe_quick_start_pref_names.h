// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_OOBE_QUICK_START_PREF_NAMES_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_OOBE_QUICK_START_PREF_NAMES_H_

// The Quick Start pref names are defined here separate from the rest of OOBE
// prefs in order to avoid a dependency cycle with //chrome/browser/ash:ash.
namespace ash::quick_start::prefs {

extern const char kShouldResumeQuickStartAfterReboot[];
extern const char kResumeQuickStartAfterRebootInfo[];

}  // namespace ash::quick_start::prefs

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_OOBE_QUICK_START_PREF_NAMES_H_

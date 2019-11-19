// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GUEST_OS_GUEST_OS_PREF_NAMES_H_
#define CHROME_BROWSER_CHROMEOS_GUEST_OS_GUEST_OS_PREF_NAMES_H_

class PrefRegistrySimple;

namespace guest_os {
namespace prefs {

extern const char kCrostiniSharedPaths[];
extern const char kGuestOSPathsSharedToVms[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace guest_os

#endif  // CHROME_BROWSER_CHROMEOS_GUEST_OS_GUEST_OS_PREF_NAMES_H_

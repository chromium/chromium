// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_PREFS_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_PREFS_H_

class PrefRegistrySimple;

namespace borealis {
namespace prefs {

// A boolean pref which records whether borealis has been successfully installed
// on the device.
extern const char kBorealisInstalledOnDevice[];

// A boolean preference for managing whether borealis is allowed for the user
// (mainly used by enterprises).
extern const char kBorealisAllowedForUser[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_PREFS_H_

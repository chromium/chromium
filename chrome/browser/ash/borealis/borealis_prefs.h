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

// A string pref which records the user's current "insert_coin" guess. We record
// the hash of that guess here, and the hash of this will be used to check the
// token (i.e. we double-hash whatever the user typed in).
extern const char kBorealisVmTokenHash[];

// A boolean preference for managing whether borealis is allowed for the user
// (mainly used by enterprises).
extern const char kBorealisAllowedForUser[];

extern const char kEngagementPrefsPrefix[];

extern const char kBorealisMicAllowed[];

// A string preference for storing additional options to be used during
// Borealis' launch. String should be a list of switches in the form of
// "-<switch>=<value>;..."". Switches and what they do are documented in
// chrome/browser/ash/borealis/borealis_launch_options.h
extern const char kExtraLaunchOptions[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_PREFS_H_

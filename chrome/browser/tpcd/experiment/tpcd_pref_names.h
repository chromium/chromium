// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_EXPERIMENT_TPCD_PREF_NAMES_H_
#define CHROME_BROWSER_TPCD_EXPERIMENT_TPCD_PREF_NAMES_H_

class PrefRegistrySimple;

namespace tpcd::experiment {

namespace prefs {

// Integer that indicates the experiment state the client (i.e. local state
// pref) is in. Refer to |ExperimentState|
extern const char kTPCDExperimentClientState[];

// Integer that indicates the experiment state version the client (i.e. local
// state pref) is in.
extern const char kTPCDExperimentClientStateVersion[];

// Boolean that indicates the experiment eligibility for the profile (i.e.
// profile pref).
extern const char kTPCDExperimentProfileState[];

}  // namespace prefs

// Call once by the browser process to register 3PCD experiment local state
// (per-client) preferences.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Call once by the browser process to register 3PCD experiment profile
// preferences.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace tpcd::experiment

#endif  // CHROME_BROWSER_TPCD_EXPERIMENT_TPCD_PREF_NAMES_H_

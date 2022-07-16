// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_SYNC_CONSENT_OPTIONAL_FIELD_TRIAL_H_
#define CHROME_BROWSER_ASH_SYNC_SYNC_CONSENT_OPTIONAL_FIELD_TRIAL_H_

class PrefRegistrySimple;
class PrefService;

namespace base {
class FeatureList;
}  // namespace base

namespace sync_consent_optional_field_trial {

// Registers preferences.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Creates a field trial to control the SyncConsentOptional feature. The trial
// is client controlled because SyncConsentOptional controls the out-of-box
// experience (OOBE) sync consent dialog, which shows up before a variations
// seed is available.
//
// The trial group chosen on first run is persisted to local state prefs and
// reused on subsequent runs. This keeps the in-session sync settings UI stable
// between runs. Local state prefs can be reset via powerwash, which will result
// in re-randomization, but this also sends the user through the first-run flow
// again and they will see the appropriate consent flow.
//
// Persisting the group also avoids a subtle corner case: A user could be
// randomized to SyncConsentOptional, opt-in to sync during OOBE, then turn off
// OS sync in OS settings but leave "Sync everything" enabled in browser
// settings. If they were re-randomized to non-SyncConsentOptional on a future
// login, then the OS sync data types would go back to being controlled by
// browser sync settings, and those OS types would be re-enabled even though the
// user had them disabled.
//
// Launch bug for the SyncConsentOptional feature: https://crbug.com/1020731
void Create(base::FeatureList* feature_list, PrefService* local_state);

}  // namespace sync_consent_optional_field_trial

#endif  // CHROME_BROWSER_ASH_SYNC_SYNC_CONSENT_OPTIONAL_FIELD_TRIAL_H_

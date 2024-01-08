// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_PLAY_STORE_ENABLED_PREFERENCE_HANDLER_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_PLAY_STORE_ENABLED_PREFERENCE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace arc {

class ArcSessionManager;

// Observes Google Play Store enabled preference (whose key is "arc.enabled"
// for historical reason), and controls ARC via ArcSessionManager.
class ArcPlayStoreEnabledPreferenceHandler {
 public:
  ArcPlayStoreEnabledPreferenceHandler(Profile* profile,
                                       ArcSessionManager* arc_session_manager);

  ArcPlayStoreEnabledPreferenceHandler(
      const ArcPlayStoreEnabledPreferenceHandler&) = delete;
  ArcPlayStoreEnabledPreferenceHandler& operator=(
      const ArcPlayStoreEnabledPreferenceHandler&) = delete;

  ~ArcPlayStoreEnabledPreferenceHandler();

  // Starts observing Google Play Store enabled preference change.
  // Also, based on its initial value, this may start ArcSession, or may start
  // removing the data, as initial state.
  void Start();

 private:
  // Called when Google Play Store enabled preference is changed.
  void OnPreferenceChanged();

  // Updates enabling/disabling of ArcSessionManager. Also, sets
  // ArcSupportHost's managed state.
  void UpdateArcSessionManager();

  const raw_ptr<Profile> profile_;

  // Owned by ArcServiceLauncher.
  const raw_ptr<ArcSessionManager> arc_session_manager_;

  // Registrar used to monitor ARC enabled state.
  PrefChangeRegistrar pref_change_registrar_;

  // Must be the last member.
  base::WeakPtrFactory<ArcPlayStoreEnabledPreferenceHandler> weak_ptr_factory_{
      this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_PLAY_STORE_ENABLED_PREFERENCE_HANDLER_H_

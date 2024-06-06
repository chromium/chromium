// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAGIC_BOOST_MAGIC_BOOST_STATE_ASH_H_
#define CHROME_BROWSER_ASH_MAGIC_BOOST_MAGIC_BOOST_STATE_ASH_H_

#include "ash/public/cpp/session/session_observer.h"
#include "ash/shell_observer.h"
#include "base/scoped_observation.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "components/prefs/pref_change_registrar.h"

namespace ash {

class SessionController;
class Shell;

// A class that holds MagicBoost related prefs and states.
class MagicBoostStateAsh : public chromeos::MagicBoostState,
                           public ash::SessionObserver,
                           public ash::ShellObserver {
 public:
  MagicBoostStateAsh();

  MagicBoostStateAsh(const MagicBoostStateAsh&) = delete;
  MagicBoostStateAsh& operator=(const MagicBoostStateAsh&) = delete;

  ~MagicBoostStateAsh() override;

  // MagicBoostState:
  int32_t AsyncIncrementHMRConsentWindowDismissCount() override;
  void AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus consent_status) override;

 private:
  friend class MagicBoostStateAshTest;

  // ash::SessionObserver:
  void OnFirstSessionStarted() override;

  // ash::ShellObserver:
  void OnShellDestroying() override;

  // Sets up callbacks for updates to relevant prefs for magic_boost.
  void RegisterPrefChanges(PrefService* pref_service);

  // Called when the related preferences are updated from the pref service.
  void OnHMRConsentStatusUpdated();
  void OnHMRConsentWindowDismissCountUpdated();

  // Observes user profile prefs for magic_boost.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::ScopedObservation<ash::SessionController, ash::SessionObserver>
      session_observation_{this};

  base::ScopedObservation<ash::Shell, ash::ShellObserver> shell_observation_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAGIC_BOOST_MAGIC_BOOST_STATE_ASH_H_

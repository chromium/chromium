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

namespace input_method {
class EditorPanelManager;
}  // namespace input_method

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
  bool IsMagicBoostAvailable() override;
  bool CanShowNoticeBannerForHMR() override;
  int32_t AsyncIncrementHMRConsentWindowDismissCount() override;
  void AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus consent_status) override;
  void AsyncWriteHMREnabled(bool enabled) override;
  void ShouldIncludeOrcaInOptIn(
      base::OnceCallback<void(bool)> callback) override;
  void DisableOrcaFeature() override;

  // Virtual for testing.
  virtual void EnableOrcaFeature();

  input_method::EditorPanelManager* GetEditorPanelManager();

  void set_editor_panel_manager_for_test(
      input_method::EditorPanelManager* editor_manager) {
    editor_manager_for_test_ = editor_manager;
  }

 private:
  friend class MagicBoostStateAshTest;

  // ash::SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // ash::ShellObserver:
  void OnShellDestroying() override;

  // Sets up callbacks for updates to relevant prefs for magic_boost.
  void RegisterPrefChanges(PrefService* pref_service);

  // Called when the related preferences are updated from the pref service.
  void OnMagicBoostEnabledUpdated();
  void OnHMREnabledUpdated();
  void OnHMRConsentStatusUpdated();
  void OnHMRConsentWindowDismissCountUpdated();

  // Observes user profile prefs for magic_boost.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::ScopedObservation<ash::SessionController, ash::SessionObserver>
      session_observation_{this};

  raw_ptr<input_method::EditorPanelManager> editor_manager_for_test_ = nullptr;

  base::ScopedObservation<ash::Shell, ash::ShellObserver> shell_observation_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAGIC_BOOST_MAGIC_BOOST_STATE_ASH_H_

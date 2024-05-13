// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_PRIVACY_SCREEN_CONTROLLER_H_
#define ASH_DISPLAY_PRIVACY_SCREEN_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/privacy_screen_dlp_helper.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/display/manager/display_configurator.h"

class PrefRegistrySimple;
class PrefService;
class PrefChangeRegistrar;

namespace ash {

// Controls the privacy screen feature.
class ASH_EXPORT PrivacyScreenController
    : public PrivacyScreenDlpHelper,
      public SessionObserver,
      display::DisplayConfigurator::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the privacy screen setting is changed.
    virtual void OnPrivacyScreenSettingChanged(bool enabled, bool notify_ui) {}

   protected:
    ~Observer() override = default;
  };

  PrivacyScreenController();
  ~PrivacyScreenController() override;

  PrivacyScreenController(const PrivacyScreenController&) = delete;
  PrivacyScreenController& operator=(const PrivacyScreenController&) = delete;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Whether or not the privacy screen feature is enforced by policy.
  bool IsManaged() const;
  // Get the PrivacyScreen settings stored in the current active user prefs.
  bool GetEnabled() const;
  // Set the desired PrivacyScreen settings in the current active user prefs.
  void SetEnabled(bool enabled);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // PrivacyScreenDlpHelper:
  bool IsSupported() const override;
  void SetEnforced(bool enforced) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;
  void OnSigninScreenPrefServiceInitialized(PrefService* pref_service) override;

  // DisplayConfigurator::Observer:
  void OnDisplayConfigurationChanged(
      const std::vector<raw_ptr<display::DisplaySnapshot, VectorExperimental>>&
          displays) override;

 private:
  // Calculates PrivacyScreen's logical status.
  bool CalculateCurrentStatus() const;

  // Called when the user pref or DLP enforcement for the state of PrivacyScreen
  // is changed.
  void OnStateChanged(bool notify_observers);

  // Called when GPU/DRM is done setting the privacy screen panel to
  // |requested_config|.
  void OnSetPrivacyScreenComplete(bool from_user_pref_init,
                                  bool requested_config,
                                  bool success);

  // Called when a change to |active_user_pref_service_| is detected (i.e. when
  // OnActiveUserPrefServiceChanged() is called.
  void InitFromUserPrefs();

  // Retrieves the current user's PrivacyScreen preference.
  bool GetStateFromActiveUserPreference() const;

  // Whether or not to alert observers about PrivacyScreen state change.
  bool ShouldNotifyObservers(bool from_user_pref_init) const;

  // The pref service of the currently active user. Can be null in
  // ash_unittests.
  raw_ptr<PrefService> active_user_pref_service_ = nullptr;

  // Set to true when entering the login screen. This should happen once per
  // Chrome restart.
  bool applying_login_screen_prefs_ = false;

  // Indicates if the PrivacyScreen is currently on or off.
  bool current_status_ = false;

  // Indicates whether PrivacyScreen is enforced by Data Leak Protection
  // feature.
  bool dlp_enforced_ = false;

  base::ObserverList<Observer> observers_;

  // The registrar used to watch privacy screen prefs changes in the above
  // |active_user_pref_service_| from outside ash.
  // NOTE: Prefs are how Chrome communicates changes to the
  // PrivacyScreenController settings controlled by this class from the WebUI
  // settings.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // This must be the last variable.
  base::WeakPtrFactory<PrivacyScreenController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_DISPLAY_PRIVACY_SCREEN_CONTROLLER_H_

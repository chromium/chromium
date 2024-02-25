// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_DARK_LIGHT_MODE_CONTROLLER_IMPL_H_
#define ASH_STYLE_DARK_LIGHT_MODE_CONTROLLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"

class AccountId;
class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace ash {

class ColorModeObserver;

// Controls the behavior of dark/light mode. Turns on the dark mode at sunset
// and off at sunrise if auto schedule is set (custom start and end for
// scheduling is not supported).
class ASH_EXPORT DarkLightModeControllerImpl
    : public DarkLightModeController,
      public LoginDataDispatcher::Observer,
      public ScheduledFeature {
 public:
  DarkLightModeControllerImpl();
  DarkLightModeControllerImpl(const DarkLightModeControllerImpl&) = delete;
  DarkLightModeControllerImpl& operator=(const DarkLightModeControllerImpl&) =
      delete;
  ~DarkLightModeControllerImpl() override;

  static DarkLightModeControllerImpl* Get();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Enables or disables auto scheduling on dark mode feature. When enabled,
  // the dark mode will automatically turn on during sunset to sunrise and off
  // outside that period.
  void SetAutoScheduleEnabled(bool enabled);

  // True if dark mode is automatically scheduled to turn on at sunset and off
  // at sunrise.
  bool GetAutoScheduleEnabled() const;

  // Toggles pref |kDarkModeEnabled|.
  void ToggleColorMode();

  // DarkLightModeController:
  void AddObserver(ColorModeObserver* observer) override;
  void RemoveObserver(ColorModeObserver* observer) override;
  bool IsDarkModeEnabled() const override;
  void SetDarkModeEnabledForTest(bool enabled) override;

  // LoginDataDispatcher::Observer:
  void OnOobeDialogStateChanged(OobeDialogState state) override;
  void OnFocusPod(const AccountId& account_id) override;

  // ScheduledFeature:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

 private:
  // ScheduledFeature:
  const char* GetFeatureName() const override;

  // Notifies all the observers on color mode changes and refreshes the system's
  // colors on this change.
  void NotifyColorModeChanges();

  // Returns a closure which calls `NotifyIfDarkModeChanged` if the dark mode
  // changed between creation and getting out of scope.
  base::ScopedClosureRunner GetNotifyOnDarkModeChangeClosure();
  void NotifyIfDarkModeChanged(bool old_is_dark_mode_enabled);

  // Temporary field for testing purposes while OOBE WebUI is being migrated.
  std::optional<bool> is_dark_mode_enabled_in_oobe_for_testing_;

  OobeDialogState oobe_state_ = OobeDialogState::HIDDEN;

  // Keep track of the last value that was sent to avoid multiple notifications.
  std::optional<bool> last_value_;

  // std::nullopt in case no user pod is focused.
  std::optional<bool> is_dark_mode_enabled_for_focused_pod_;

  base::ObserverList<ColorModeObserver> observers_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  raw_ptr<PrefService> active_user_pref_service_ = nullptr;  // Not owned.
};

}  // namespace ash

#endif  // ASH_STYLE_DARK_LIGHT_MODE_CONTROLLER_IMPL_H_

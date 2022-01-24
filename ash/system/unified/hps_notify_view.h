// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_HPS_NOTIFY_VIEW_H_
#define ASH_SYSTEM_UNIFIED_HPS_NOTIFY_VIEW_H_

#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/scoped_observation.h"
#include "chromeos/dbus/hps/hps_dbus_client.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;

namespace ash {

// The icon in the system tray notifying a user that a second person has been
// detected looking over their shoulder.
class ASH_EXPORT HpsNotifyView : public TrayItemView,
                                 public SessionObserver,
                                 public chromeos::HpsDBusClient::Observer {
 public:
  explicit HpsNotifyView(Shelf* shelf);
  HpsNotifyView(const HpsNotifyView&) = delete;
  HpsNotifyView& operator=(const HpsNotifyView&) = delete;
  ~HpsNotifyView() override;

  // TODO(crbug.com/1241704): refactor this class into a controller that
  // provides access to a TrayItemView* for the snooping icon.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // views::TrayItemView:
  const char* GetClassName() const override;
  void HandleLocaleChange() override;
  void OnThemeChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // chromeos::HpsDBusClient::Observer:
  void OnHpsNotifyChanged(bool state) override;

 private:
  // Updates the system tray icon to use the color corresponding to the current
  // session state (e.g. darker during OOBE).
  void UpdateIconColor(session_manager::SessionState session_state);

  // Shows or hides the system tray icon as appropriate given the signal,
  // session and preference state. Also updates the current cached states.
  void UpdateIconVisibility(bool is_oobe, bool hps_state, bool is_enabled);

  // A callback to set our initial state by polling the presence daemon.
  void OnHpsPollResult(absl::optional<bool> result);

  // A callback to update visibility when the user enables or disables the
  // feature.
  void OnPrefChanged();

  // Used to track whether a signal should actually trigger a visibility:
  bool hps_state_ = false;   // The state last reported by the daemon.
  bool is_oobe_ = false;     // Whether or not we're in the OOBE.
  bool is_enabled_ = false;  // Whether or not the user has enabled the feature.

  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};
  base::ScopedObservation<chromeos::HpsDBusClient,
                          chromeos::HpsDBusClient::Observer>
      hps_dbus_observation_{this};

  // Used to notify ourselves of changes to the pref that enables / disables
  // this feature.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Must be last.
  base::WeakPtrFactory<HpsNotifyView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_HPS_NOTIFY_VIEW_H_

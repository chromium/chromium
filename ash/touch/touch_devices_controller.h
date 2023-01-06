// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_DEVICES_CONTROLLER_H_
#define ASH_TOUCH_TOUCH_DEVICES_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/functional/callback.h"

class AccountId;
class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace ash {

// Different sources for requested touchscreen/touchpad enabled/disabled state.
enum class TouchDeviceEnabledSource {
  // User-requested state set via a debug accelerator and stored in a pref.
  USER_PREF,
  // Transient global state used to disable touchscreen via power button.
  GLOBAL,
};

// Controls the enabled state of touchpad and touchscreen. Must be initialized
// after Shell and SessionController.
class ASH_EXPORT TouchDevicesController : public SessionObserver {
 public:
  TouchDevicesController();

  TouchDevicesController(const TouchDevicesController&) = delete;
  TouchDevicesController& operator=(const TouchDevicesController&) = delete;

  ~TouchDevicesController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test);

  // Toggles the status of touchpad between enabled and disabled.
  void ToggleTouchpad();

  // Returns the current touchpad enabled status as specified by |source|.
  bool GetTouchpadEnabled(TouchDeviceEnabledSource source) const;

  // Updates the current touchpad status for |source| to |enabled|.
  void SetTouchpadEnabled(bool enabled, TouchDeviceEnabledSource source);

  // Returns the current touchscreen enabled status as specified by |source|.
  // Note that the actual state of the touchscreen device is automatically
  // determined based on the requests of multiple sources.
  bool GetTouchscreenEnabled(TouchDeviceEnabledSource source) const;

  // Sets |source|'s requested touchscreen enabled status to |enabled|. Note
  // that the actual state of the touchscreen device is automatically determined
  // based on the requests of multiple sources.
  void SetTouchscreenEnabled(bool enabled, TouchDeviceEnabledSource source);

  bool tap_dragging_enabled_for_test() { return tap_dragging_enabled_; }

 private:
  // Overridden from SessionObserver:
  void OnUserSessionAdded(const AccountId& account_id) override;
  void OnSigninScreenPrefServiceInitialized(PrefService* prefs) override;
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  // Observes either the signin screen prefs or active user prefs and loads
  // initial state.
  void ObservePrefs(PrefService* prefs);

  // Updates tap dragging enabled state from prefs.
  void UpdateTapDraggingEnabled();

  // Updates the actual enabled/disabled status of the touchpad.
  void UpdateTouchpadEnabled();

  // Updates the actual enabled/disabled status of the touchscreen. Touchscreen
  // is enabled if all the touchscreen enabled sources are enabled.
  void UpdateTouchscreenEnabled();

  // The internal touchpad state which is associated with the global touch
  // device enabled source.
  bool global_touchpad_enabled_ = true;

  // Saves the tap dragging enabled state from prefs.
  bool tap_dragging_enabled_ = false;

  // The touchscreen state which is associated with the global touch device
  // enabled source.
  bool global_touchscreen_enabled_ = true;

  // Observes user profile prefs for touch devices.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Used to record pref started UMA, bound on user session added and run on
  // active user pref service changed. The goal is to record the initial state
  // of the feature.
  base::OnceCallback<void(PrefService* prefs)> uma_record_callback_;
};

}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_DEVICES_CONTROLLER_H_

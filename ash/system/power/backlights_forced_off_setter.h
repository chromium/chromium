// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_BACKLIGHTS_FORCED_OFF_SETTER_H_
#define ASH_SYSTEM_POWER_BACKLIGHTS_FORCED_OFF_SETTER_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/public/cpp/screen_backlight.h"
#include "ash/public/cpp/screen_backlight_observer.h"
#include "ash/public/cpp/screen_backlight_type.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {

class ScopedBacklightsForcedOff;

// BacklightsForcedOffSetter manages multiple requests to force the backlights
// off and coalesces them into SetBacklightsForcedOff D-Bus calls to powerd.
class ASH_EXPORT BacklightsForcedOffSetter
    : public chromeos::PowerManagerClient::Observer,
      public ScreenBacklight {
 public:
  BacklightsForcedOffSetter();

  BacklightsForcedOffSetter(const BacklightsForcedOffSetter&) = delete;
  BacklightsForcedOffSetter& operator=(const BacklightsForcedOffSetter&) =
      delete;

  ~BacklightsForcedOffSetter() override;

  bool backlights_forced_off() const {
    return backlights_forced_off_.value_or(false);
  }

  // ScreenBacklight:
  void AddObserver(ScreenBacklightObserver* observer) override;
  void RemoveObserver(ScreenBacklightObserver* observer) override;
  ScreenBacklightState GetScreenBacklightState() const override;

  // Forces the backlights off. The backlights will be kept in the forced-off
  // state until all requests have been destroyed.
  std::unique_ptr<ScopedBacklightsForcedOff> ForceBacklightsOff();

  // Overridden from chromeos::PowerManagerClient::Observer:
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;
  void PowerManagerRestarted() override;

  // Resets internal state for tests.
  // Note: This will silently cancel all active backlights forced off requests.
  void ResetForTest();

 private:
  // Sets |disable_touchscreen_while_screen_off_| depending on the state of the
  // current command line,
  void InitDisableTouchscreenWhileScreenOff();

  // Sends a request to powerd to get the backlights forced off state so that
  // |backlights_forced_off_| can be initialized.
  void GetInitialBacklightsForcedOff();

  // Callback for |GetInitialBacklightsForcedOff()|.
  void OnGotInitialBacklightsForcedOff(std::optional<bool> is_forced_off);

  // Removes a force backlights off request from the list of active ones, which
  // effectively cancels the request. This is passed to every
  // ScopedBacklightsForcedOff created by |ForceBacklightsOff| as its
  // cancellation callback.
  void OnScopedBacklightsForcedOffDestroyed();

  // Updates the power manager's backlights-forced-off state.
  void SetBacklightsForcedOff(bool forced_off);

  // Enables or disables the touchscreen by updating the global touchscreen
  // enabled status. The touchscreen is disabled when backlights are forced off
  // or |screen_backlight_state_| is OFF_AUTO.
  void UpdateTouchscreenStatus();

  // Controls whether the touchscreen is disabled when the screen is turned off
  // due to user inactivity.
  bool disable_touchscreen_while_screen_off_ = true;

  // Current forced-off state of backlights.
  std::optional<bool> backlights_forced_off_;

  // Current screen state.
  ScreenBacklightState screen_backlight_state_ = ScreenBacklightState::ON;

  // Number of active backlights forced off requests.
  int active_backlights_forced_off_count_ = 0;

  base::ObserverList<ScreenBacklightObserver>::Unchecked observers_;

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_observation_{this};

  base::WeakPtrFactory<BacklightsForcedOffSetter> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_BACKLIGHTS_FORCED_OFF_SETTER_H_

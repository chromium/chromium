// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_PERFORMANCE_MODE_CONTROLLER_H_
#define ASH_DISPLAY_DISPLAY_PERFORMANCE_MODE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/power/power_status.h"
#include "base/scoped_observation.h"

namespace ash {

// DisplayPerformanceModeController listens to the power status change and the
// Display Performance Mode change in the UI to dictate what state the display
// features should be at. Display features that want to depend on the power and
// user preference would listen to this controller and update their state
// accordingly.
class ASH_EXPORT DisplayPerformanceModeController
    : public PowerStatus::Observer {
 public:
  // DisplayPerformanceModeController exposes 3 different modes that a client
  // observing a mode change gets:

  // kHighPerformance: This mode is enabled by the user that wants the best and
  // smoothest display experience. This mode does not necessarily indicate that
  // a power source is connected. However, in this mode, the display features
  // will prioritize performance over power efficiency.

  // kIntelligent: This mode represents an intelligent state for the display
  // features. Users can expect that the display features will dynamically
  // adjust their behavior based on the power status and user preferences. The
  // display features will strive to balance performance and power efficiency,
  // optimizing the user experience. This mode is the default mode.

  // kPowerSaver: This mode represents a power-saving state for the display
  // features. Users can expect that the display features will prioritize
  // power efficiency over performance. This mode is triggered by the system
  // Power Saver mode and is currently not user-configurable.
  enum class ModeState {
    kHighPerformance,
    kIntelligent,
    kPowerSaver,
    kDefault = kIntelligent
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDisplayPerformanceModeChanged(ModeState new_state) = 0;

   protected:
    ~Observer() override = default;
  };

  explicit DisplayPerformanceModeController();
  DisplayPerformanceModeController(const DisplayPerformanceModeController&) =
      delete;
  DisplayPerformanceModeController& operator=(
      const DisplayPerformanceModeController&) = delete;
  ~DisplayPerformanceModeController() override;

  // When an observer is added, the current state should be sent to be up to
  // date.
  ModeState AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetHighPerformanceModeByUser(bool is_high_performance_enabled);

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

  // For testing. Clients should observe the mode change instead of calling
  // this.
  ModeState GetCurrentStateForTesting() const { return current_state_; }

 private:
  void UpdateCurrentStateAndNotifyIfChanged();
  void NotifyObservers();

  ModeState current_state_ = ModeState::kIntelligent;
  bool is_high_performance_enabled_ = false;

  base::ObserverList<Observer> observers_;

  // Not owned.
  // TODO(b/327054689): This pointer is needed because some power tests delete
  // PowerStatus without the observers knowing about it, so we have to check for
  // its validity before using it.
  base::WeakPtr<PowerStatus> power_status_;

  base::WeakPtrFactory<DisplayPerformanceModeController> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_PERFORMANCE_MODE_CONTROLLER_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_EVENT_OBSERVER_TEST_API_H_
#define ASH_SYSTEM_POWER_POWER_EVENT_OBSERVER_TEST_API_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ui {
class Compositor;
}

namespace ash {

class PowerEventObserver;

class PowerEventObserverTestApi {
 public:
  explicit PowerEventObserverTestApi(PowerEventObserver* power_event_observer);

  PowerEventObserverTestApi(const PowerEventObserverTestApi&) = delete;
  PowerEventObserverTestApi& operator=(const PowerEventObserverTestApi&) =
      delete;

  ~PowerEventObserverTestApi();

  void SendLidEvent(chromeos::PowerManagerClient::LidState state);

  void CompositingDidCommit(ui::Compositor* compositor);
  void CompositingStarted(ui::Compositor* compositor);
  void CompositingAckDeprecated(ui::Compositor* compositor);

  // Same as calling CompositingDidCommit, CompositingStarted and
  // CompositingAckDeprecated in sequence.
  void CompositeFrame(ui::Compositor* compositor);

  bool SimulateCompositorsReadyForSuspend();

  bool TrackingLockOnSuspendUsage() const;

 private:
  raw_ptr<PowerEventObserver> power_event_observer_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_EVENT_OBSERVER_TEST_API_H_

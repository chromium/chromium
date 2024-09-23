// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_event_observer_test_api.h"

#include "ash/system/power/power_event_observer.h"
#include "base/time/time.h"
#include "ui/compositor/compositor_observer.h"

namespace ash {

PowerEventObserverTestApi::PowerEventObserverTestApi(
    PowerEventObserver* power_event_observer)
    : power_event_observer_(power_event_observer) {}

PowerEventObserverTestApi::~PowerEventObserverTestApi() = default;

void PowerEventObserverTestApi::SendLidEvent(
    chromeos::PowerManagerClient::LidState state) {
  power_event_observer_->LidEventReceived(state, base::TimeTicks::Now());
}

void PowerEventObserverTestApi::CompositingDidCommit(
    ui::Compositor* compositor) {
  if (!power_event_observer_->compositor_watcher_.get())
    return;
  power_event_observer_->compositor_watcher_->OnCompositingDidCommit(
      compositor);
}

void PowerEventObserverTestApi::CompositingStarted(ui::Compositor* compositor) {
  if (!power_event_observer_->compositor_watcher_.get())
    return;
  power_event_observer_->compositor_watcher_->OnCompositingStarted(
      compositor, base::TimeTicks());
}

void PowerEventObserverTestApi::CompositingAckDeprecated(
    ui::Compositor* compositor) {
  if (!power_event_observer_->compositor_watcher_.get())
    return;
  power_event_observer_->compositor_watcher_->OnCompositingAckDeprecated(
      compositor);
}

void PowerEventObserverTestApi::CompositeFrame(ui::Compositor* compositor) {
  if (!power_event_observer_->compositor_watcher_.get())
    return;
  power_event_observer_->compositor_watcher_->OnCompositingDidCommit(
      compositor);
  power_event_observer_->compositor_watcher_->OnCompositingStarted(
      compositor, base::TimeTicks());
  power_event_observer_->compositor_watcher_->OnCompositingAckDeprecated(
      compositor);
}

bool PowerEventObserverTestApi::SimulateCompositorsReadyForSuspend() {
  if (!power_event_observer_->compositor_watcher_.get())
    return false;
  power_event_observer_->OnCompositorsReadyForSuspend();
  return true;
}

bool PowerEventObserverTestApi::TrackingLockOnSuspendUsage() const {
  return power_event_observer_->lock_on_suspend_usage_.get();
}

}  // namespace ash

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/idle_service_ash.h"

#include <stdint.h>

#include "base/logging.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/event.h"

namespace crosapi {

namespace {

// Minimal intervals between successive UserActivity events to trigger dispatch
// in IdleServiceAsh::Dispatcher.
constexpr int64_t kUserActivityDispatchIntervalMs = 1000;

}  // namespace

/******** IdleServiceAsh::Dispatcher ********/

IdleServiceAsh::Dispatcher::Dispatcher() {
  if (!IdleServiceAsh::Dispatcher::is_disabled_for_testing_) {
    CHECK(ui::UserActivityDetector::Get());
    CHECK(ash::SessionManagerClient::Get());
    ui::UserActivityDetector::Get()->AddObserver(this);
    ash::SessionManagerClient::Get()->AddObserver(this);
  }
}

IdleServiceAsh::Dispatcher::~Dispatcher() {
  if (!IdleServiceAsh::Dispatcher::is_disabled_for_testing_) {
    ash::SessionManagerClient::Get()->RemoveObserver(this);
    ui::UserActivityDetector::Get()->RemoveObserver(this);
  }
}

void IdleServiceAsh::Dispatcher::OnUserActivity(const ui::Event* event) {
  base::TimeTicks event_time =
      event ? event->time_stamp() : base::TimeTicks::Now();
  // UserActivityDetector already limits event frequency, but apply even more
  // limit to prevent incessant calls to DispatchChange().
  if (last_user_activity_time_stamp_.is_null() ||
      (event_time - last_user_activity_time_stamp_).InMilliseconds() >=
          kUserActivityDispatchIntervalMs) {
    last_user_activity_time_stamp_ = event_time;
    DispatchChange();
  }
}

void IdleServiceAsh::Dispatcher::ScreenLockedStateUpdated() {
  DispatchChange();
}

void IdleServiceAsh::Dispatcher::DispatchChange() {
  mojom::IdleInfoPtr idle_info = IdleServiceAsh::ReadIdleInfoFromSystem();
  for (auto& observer : observers_) {
    mojom::IdleInfoPtr idle_info_copy = idle_info->Clone();
    observer->OnIdleInfoChanged(std::move(idle_info_copy));
  }
}

// static
bool IdleServiceAsh::Dispatcher::is_disabled_for_testing_ = false;

/******** IdleServiceAsh ********/

// static
mojom::IdleInfoPtr IdleServiceAsh::ReadIdleInfoFromSystem() {
  mojom::IdleInfoPtr idle_info = mojom::IdleInfo::New();

  if (!IdleServiceAsh::Dispatcher::is_disabled_for_testing_) {
    // Taken from extensions::IdleManager::GetAutoLockDelay().
    idle_info->auto_lock_delay = chromeos::PowerPolicyController::Get()
                                     ->GetMaxPolicyAutoScreenLockDelay();

    // Taken from ui::CalculateIdleTime() for ChromeOS (partial).
    idle_info->last_activity_time =
        ui::UserActivityDetector::Get()->last_activity_time();

    // Taken from ui::CheckIdleStateIsLocked() for ChromeOS.
    idle_info->is_locked = ash::SessionManagerClient::Get()->IsScreenLocked();
  }

  return idle_info;
}

// static
void IdleServiceAsh::DisableForTesting() {
  IdleServiceAsh::Dispatcher::is_disabled_for_testing_ = true;
}

IdleServiceAsh::IdleServiceAsh() = default;

IdleServiceAsh::~IdleServiceAsh() = default;

void IdleServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::IdleService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void IdleServiceAsh::AddIdleInfoObserver(
    mojo::PendingRemote<mojom::IdleInfoObserver> observer) {
  // Fire the observer with the initial value.
  mojo::Remote<mojom::IdleInfoObserver> remote(std::move(observer));
  remote->OnIdleInfoChanged(ReadIdleInfoFromSystem());

  dispatcher_.observers_.Add(std::move(remote));
}

}  // namespace crosapi

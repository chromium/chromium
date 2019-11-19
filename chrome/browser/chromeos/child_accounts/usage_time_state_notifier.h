// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_USAGE_TIME_STATE_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_USAGE_TIME_STATE_NOTIFIER_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace chromeos {

// Notify observers when the state of use of the device changes, that is, when
// the child is using the device or not. We define that the child is not using
// the device when one of the following events happens:
//    * Session state change to some state different than ACTIVE;
//    * When device is in suspend mode;
//    * When the screen is powered off due user inactivity.
class UsageTimeStateNotifier : public session_manager::SessionManagerObserver,
                               public chromeos::PowerManagerClient::Observer {
 public:
  // Possible states for usage time.
  enum class UsageTimeState {
    // Active state means that the child is using the device.
    ACTIVE,
    // Inactive state means that the child is not using the device.
    INACTIVE
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when usage time state changes to |state|.
    virtual void OnUsageTimeStateChange(
        UsageTimeStateNotifier::UsageTimeState state) = 0;
  };

  // Always returns the singleton instance of UsageTimeStateNotifier. It will
  // initialize the instance in the first time it's called.
  static UsageTimeStateNotifier* GetInstance();

  // Adds and removes observers.
  void AddObserver(UsageTimeStateNotifier::Observer* observer);
  void RemoveObserver(UsageTimeStateNotifier::Observer* observer);

  UsageTimeState GetState() const;

 private:
  UsageTimeStateNotifier();
  ~UsageTimeStateNotifier() override;
  friend class base::NoDestructor<UsageTimeStateNotifier>;

  // Sends a notification to all observers that usage time state is now |state|.
  void ChangeUsageTimeState(UsageTimeStateNotifier::UsageTimeState state);

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // power_manager::PowerManagerClient::Observer:
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& state) override;
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  base::ObserverList<Observer> observers_;

  UsageTimeState last_state_;

  DISALLOW_COPY_AND_ASSIGN(UsageTimeStateNotifier);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_USAGE_TIME_STATE_NOTIFIER_H_

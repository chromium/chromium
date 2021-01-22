// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_POWER_CLIENT_H_
#define CHROME_BROWSER_NEARBY_SHARING_POWER_CLIENT_H_

#include <memory>

#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"

class PowerClient : public chromeos::PowerManagerClient::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void SuspendImminent() {}
    virtual void SuspendDone() {}
    virtual void ScreenStateChanged(bool is_screen_on) {}
  };

  PowerClient();
  ~PowerClient() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool IsSuspended();
  bool IsScreenOn();

 protected:
  void SetSuspended(bool is_suspended);
  void SetScreenOn(bool is_screen_on);

 private:
  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& state) override;

  base::ObserverList<Observer> observers_;
  base::OneShotTimer screen_state_notify_timer_;
  bool is_suspended_ = false;
  bool is_screen_on_ = true;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_POWER_CLIENT_H_

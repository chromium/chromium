// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_COLLECTOR_BASE_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_COLLECTOR_BASE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace policy {

// This class contains logic shared between event log updaters, including
// observing network and suspend events.
// Events are then forwarded to derived classes when uploads should be
// attempted.
class InstallEventLogCollectorBase
    : public chromeos::PowerManagerClient::Observer,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  explicit InstallEventLogCollectorBase(Profile* profile);
  ~InstallEventLogCollectorBase() override;

  // Event handlers for the login and logout events.
  // Common conditions are checked before forwarding to derived classes.
  void OnLogin();
  void OnLogout();

 protected:
  // Whether the device is currently online.
  bool online_ = false;
  const raw_ptr<Profile> profile_;

  bool GetOnlineState();

  // Called in case of login and logout and pending apps.
  virtual void OnLoginInternal() = 0;
  virtual void OnLogoutInternal() = 0;

  // Called to forward to derived classes changes to the state of connection.
  virtual void OnConnectionStateChanged(
      network::mojom::ConnectionType type) = 0;

 private:
  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override =
      0;
  void SuspendDone(base::TimeDelta sleep_duration) override = 0;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;
};
}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_COLLECTOR_BASE_H_

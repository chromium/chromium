// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_LORGNETTE_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_SCANNING_LORGNETTE_NOTIFICATION_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"

class Profile;

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

class LorgnetteNotificationController : public DlcserviceClient::Observer {
 public:
  // Current State of the DLC installation
  enum class DlcState {
    kInstalledSuccessfully,
    kInstalling,
    kInstallError,
    kIdle
  };

  explicit LorgnetteNotificationController(Profile* profile);
  ~LorgnetteNotificationController() override;

  LorgnetteNotificationController(const LorgnetteNotificationController&) =
      delete;
  LorgnetteNotificationController& operator=(
      const LorgnetteNotificationController&) = delete;

  // DlcserviceClient::Observer
  void OnDlcStateChanged(const dlcservice::DlcState& dlc_state) override;

  DlcState current_state_for_testing();

 private:
  std::unique_ptr<message_center::Notification> CreateNotification();
  void DisplayNotification(
      std::unique_ptr<message_center::Notification> notification);

  base::ScopedObservation<DlcserviceClient, DlcserviceClient::Observer>
      dlc_observer_{this};
  DlcState current_state_;

  raw_ptr<Profile> profile_;  // Not owned.
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_LORGNETTE_NOTIFICATION_CONTROLLER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_LORGNETTE_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_SCANNING_LORGNETTE_NOTIFICATION_CONTROLLER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>

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
  // Current state of a DLC installation
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

  std::optional<DlcState> current_state_for_testing(const std::string& dlc_id);

 private:
  // Assumes `dlc_id` is supported.
  std::unique_ptr<message_center::Notification> CreateNotification(
      const std::string& dlc_id);
  // Assumes `dlc_id` is supported.
  void DisplayNotification(
      std::unique_ptr<message_center::Notification> notification,
      const std::string& dlc_id);

  base::ScopedObservation<DlcserviceClient, DlcserviceClient::Observer>
      dlc_observer_{this};
  // Contains the current state of each DLC.
  std::map<std::string, DlcState> current_state_per_dlc_;
  // Set of all DLC IDs supported by LorgnetteNotificationController.
  std::set<std::string> supported_dlc_ids_;

  raw_ptr<Profile> profile_;  // Not owned.
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_LORGNETTE_NOTIFICATION_CONTROLLER_H_

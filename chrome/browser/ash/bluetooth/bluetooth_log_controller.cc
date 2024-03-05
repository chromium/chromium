// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bluetooth/bluetooth_log_controller.h"

#include "base/functional/callback_helpers.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {
namespace {
constexpr char kBluetoothLoggingUpstartJob[] = "bluetoothlog";
}  // namespace

BluetoothLogController::BluetoothLogController(
    user_manager::UserManager* user_manager) {
  observation_.Observe(user_manager);
}

BluetoothLogController::~BluetoothLogController() = default;

void BluetoothLogController::OnUserLoggedIn(const user_manager::User& user) {
  // Starts bluetooth logging service for internal accounts and certain devices.
  if (user.GetType() != user_manager::UserType::kRegular ||
      !gaia::IsGoogleInternalAccountEmail(user.GetAccountId().GetUserEmail())) {
    return;
  }

  UpstartClient::Get()->StartJob(kBluetoothLoggingUpstartJob, {},
                                 base::DoNothing());
}

}  // namespace ash

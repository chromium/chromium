// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BLUETOOTH_BLUETOOTH_LOG_CONTROLLER_H_
#define CHROME_BROWSER_ASH_BLUETOOTH_BLUETOOTH_LOG_CONTROLLER_H_

#include "base/scoped_observation.h"
#include "components/user_manager/user_manager.h"

namespace ash {

// Controls `bluetoothlog` upstart job.
class BluetoothLogController : public user_manager::UserManager::Observer {
 public:
  explicit BluetoothLogController(user_manager::UserManager* user_manager);
  BluetoothLogController(const BluetoothLogController&) = delete;
  BluetoothLogController& operator=(const BluetoothLogController&) = delete;
  ~BluetoothLogController() override;

  // user_manager::UserManager::Observer:
  void OnUserLoggedIn(const user_manager::User& user) override;

 private:
  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BLUETOOTH_BLUETOOTH_LOG_CONTROLLER_H_

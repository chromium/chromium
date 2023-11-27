// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TETHER_FAKE_TETHER_SERVICE_H_
#define CHROME_BROWSER_ASH_TETHER_FAKE_TETHER_SERVICE_H_

#include "chrome/browser/ash/tether/tether_service.h"

namespace ash {
namespace tether {

// A stub of TetherService that provides an easy way to develop for Tether on
// non-Chromebooks or without a Tether host. To use, see
// `switches::kTetherStub` for more details.
class FakeTetherService : public TetherService {
 public:
  FakeTetherService(
      Profile* profile,
      chromeos::PowerManagerClient* power_manager_client,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      session_manager::SessionManager* session_manager);
  FakeTetherService(const FakeTetherService&) = delete;
  FakeTetherService& operator=(const FakeTetherService&) = delete;

  // TetherService:
  void StartTetherIfPossible() override;
  void StopTetherIfNecessary() override;

  void set_num_tether_networks(int num_tether_networks) {
    num_tether_networks_ = num_tether_networks;
  }

 protected:
  // TetherService:
  bool HasSyncedTetherHosts() const override;

 private:
  int num_tether_networks_ = 1;
};

}  // namespace tether
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TETHER_FAKE_TETHER_SERVICE_H_

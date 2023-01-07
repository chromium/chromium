// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CHROME_APP_KIOSK_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_CHROME_APP_KIOSK_SERVICE_ASH_H_

#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// The ash-chrome implementation of the ChromeAppKioskService crosapi interface.
// This is used to forward the APIs provided by ChromeKioskLaunchController to
// Lacros.
// This class must only be used from the main thread.
class ChromeAppKioskServiceAsh : public mojom::ChromeAppKioskService {
 public:
  ChromeAppKioskServiceAsh();
  ChromeAppKioskServiceAsh(const ChromeAppKioskServiceAsh&) = delete;
  ChromeAppKioskServiceAsh& operator=(const ChromeAppKioskServiceAsh&) = delete;
  ~ChromeAppKioskServiceAsh() override;

  // Bind this receiver for `mojom::ChromeAppKioskService`. This is used by
  // crosapi.
  void BindReceiver(
      mojo::PendingReceiver<mojom::ChromeAppKioskService> receiver);

  // mojom::ChromeAppKioskService:
  void BindLaunchController(
      mojo::PendingRemote<mojom::ChromeKioskLaunchController> launch_controller)
      override;

  void InstallKioskApp(
      const mojom::AppInstallParams& params,
      mojom::ChromeKioskLaunchController::InstallKioskAppCallback callback);

  void LaunchKioskApp(
      std::string app_id,
      bool is_network_ready,
      mojom::ChromeKioskLaunchController::LaunchKioskAppCallback callback);

 private:
  mojom::ChromeKioskLaunchController* GetController();

  mojo::ReceiverSet<mojom::ChromeAppKioskService> receivers_;
  mojo::RemoteSet<mojom::ChromeKioskLaunchController> launch_controllers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CHROME_APP_KIOSK_SERVICE_ASH_H_

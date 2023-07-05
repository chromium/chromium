// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_WEB_KIOSK_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_WEB_KIOSK_SERVICE_ASH_H_

#include "chromeos/crosapi/mojom/web_kiosk_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "url/gurl.h"

namespace crosapi {

// The ash-chrome implementation of the WebKioskService crosapi interface.
// This is used to forward the APIs provided by WebKioskLaunchController to
// Lacros.
// This class must only be used from the main thread.
class WebKioskServiceAsh : public mojom::WebKioskService {
 public:
  WebKioskServiceAsh();
  WebKioskServiceAsh(const WebKioskServiceAsh&) = delete;
  WebKioskServiceAsh& operator=(const WebKioskServiceAsh&) = delete;
  ~WebKioskServiceAsh() override;

  // Binds this receiver for `mojom::WebKioskService`. This is used by crosapi.
  void BindReceiver(mojo::PendingReceiver<mojom::WebKioskService> receiver);

  // mojom::WebKioskService:
  void BindInstaller(
      mojo::PendingRemote<mojom::WebKioskInstaller> installer) override;

  void GetWebKioskInstallState(
      const GURL& url,
      mojom::WebKioskInstaller::GetWebKioskInstallStateCallback callback);

  void InstallWebKiosk(
      const GURL& url,
      mojom::WebKioskInstaller::InstallWebKioskCallback callback);

 private:
  mojom::WebKioskInstaller* GetInstaller();

  mojo::ReceiverSet<mojom::WebKioskService> receivers_;
  mojo::RemoteSet<mojom::WebKioskInstaller> installers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_WEB_KIOSK_SERVICE_ASH_H_

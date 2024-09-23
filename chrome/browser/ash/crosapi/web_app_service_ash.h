// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_WEB_APP_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_WEB_APP_SERVICE_ASH_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// Created in ash-chrome. Allows lacros-chrome:
// 1) to query web-app-related information in ash-chrome,
// 2) to register its own |crosapi::mojom::WebAppProviderBridge| to let
//    ash-chrome to modify or query WebAppProvider in lacros-chrome.
class WebAppServiceAsh : public crosapi::mojom::WebAppService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnWebAppProviderBridgeConnected() {}
    virtual void OnWebAppProviderBridgeDisconnected() {}
    virtual void OnWebAppServiceAshDestroyed() = 0;
  };

  WebAppServiceAsh();
  ~WebAppServiceAsh() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void BindReceiver(mojo::PendingReceiver<mojom::WebAppService> receiver);

  // crosapi::mojom::WebAppService:
  void RegisterWebAppProviderBridge(
      mojo::PendingRemote<mojom::WebAppProviderBridge> web_app_provider_bridge)
      override;
  void GetAssociatedAndroidPackage(
      const std::string& app_id,
      GetAssociatedAndroidPackageCallback callback) override;
  void MigrateLauncherState(const std::string& from_app_id,
                            const std::string& to_app_id,
                            MigrateLauncherStateCallback callback) override;

  // Returns the web app provider bridge of the currently connected
  // lacros-chrome, or nullptr if there is no connection.
  mojom::WebAppProviderBridge* GetWebAppProviderBridge();

 private:
  // Called when |web_app_provider_bridge_| is disconnected.
  void OnBridgeDisconnected();

  // Crosapi clients connected to this object.
  mojo::ReceiverSet<mojom::WebAppService> receivers_;

  // Remote lacros-chrome web app bridge to send commands to.
  // At the moment only a single connection is supported.
  // TODO(crbug.com/40167449): Support SxS lacros.
  mojo::Remote<mojom::WebAppProviderBridge> web_app_provider_bridge_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<WebAppServiceAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_WEB_APP_SERVICE_ASH_H_

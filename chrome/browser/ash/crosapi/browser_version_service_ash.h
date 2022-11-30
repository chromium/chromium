// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_VERSION_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_VERSION_SERVICE_ASH_H_

#include "chromeos/crosapi/mojom/browser_version.mojom.h"
#include "components/component_updater/component_updater_service.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// The ash-chrome implementation of the BrowserVersion crosapi interface.
class BrowserVersionServiceAsh
    : public mojom::BrowserVersionService,
      public component_updater::ComponentUpdateService::Observer {
 public:
  explicit BrowserVersionServiceAsh(
      component_updater::ComponentUpdateService* component_updater_service);

  BrowserVersionServiceAsh(const BrowserVersionServiceAsh&) = delete;
  BrowserVersionServiceAsh& operator=(const BrowserVersionServiceAsh&) = delete;
  ~BrowserVersionServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::BrowserVersionService> receiver);

  // crosapi::mojom::BrowserVersionService:
  void AddBrowserVersionObserver(
      mojo::PendingRemote<mojom::BrowserVersionObserver> observer) override;
  void GetInstalledBrowserVersion(
      GetInstalledBrowserVersionCallback callback) override;

 private:
  // component_updater::ComponentUpdateService::Observer:
  void OnEvent(Events event, const std::string& id) override;

  // Returns the browser version if there is one installed.
  absl::optional<base::Version> GetBrowserVersion();

  component_updater::ComponentUpdateService* const component_update_service_;

  // Support any number of connections.
  mojo::ReceiverSet<mojom::BrowserVersionService> receivers_;

  // Support any number of observers.
  mojo::RemoteSet<mojom::BrowserVersionObserver> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_VERSION_SERVICE_ASH_H_

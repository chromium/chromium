// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_VERSION_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_VERSION_SERVICE_ASH_H_

#include "base/memory/raw_ptr.h"
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
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the latest available lacros-chrome version that can be launched.
    virtual base::Version GetLatestLaunchableBrowserVersion() const = 0;

    // Returns true if there is a more recent lacros-chrome binary available
    // than what has currently been launched.
    virtual bool IsNewerBrowserAvailable() const = 0;
  };

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

  const Delegate* GetDelegate() const;

  void set_delegate_for_testing(const Delegate* delegate) {
    delegate_for_testing_ = delegate;
  }

 private:
  // component_updater::ComponentUpdateService::Observer:
  void OnEvent(const update_client::CrxUpdateItem& id) override;

  // Returns the stringified version of the latest available lacros-chrome that
  // can be launched.
  std::string GetLatestLaunchableBrowserVersion() const;

  const raw_ptr<component_updater::ComponentUpdateService>
      component_update_service_;

  // Optional delegate member for testing.
  raw_ptr<const Delegate> delegate_for_testing_ = nullptr;

  // Support any number of connections.
  mojo::ReceiverSet<mojom::BrowserVersionService> receivers_;

  // Support any number of observers.
  mojo::RemoteSet<mojom::BrowserVersionObserver> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_VERSION_SERVICE_ASH_H_

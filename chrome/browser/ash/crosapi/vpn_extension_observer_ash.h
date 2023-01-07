// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_VPN_EXTENSION_OBSERVER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_VPN_EXTENSION_OBSERVER_ASH_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/crosapi/mojom/vpn_extension_observer.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi::mojom::VpnExtensionObserver interface for observing
// Lacros Vpn extensions.
class VpnExtensionObserverAsh : public crosapi::mojom::VpnExtensionObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnLacrosVpnExtensionLoaded(
        const std::string& extension_id,
        const std::string& extension_name) = 0;

    virtual void OnLacrosVpnExtensionUnloaded(
        const std::string& extension_id) = 0;

    virtual void OnLacrosVpnExtensionObserverDisconnected() = 0;
  };

  VpnExtensionObserverAsh();
  ~VpnExtensionObserverAsh() override;

  VpnExtensionObserverAsh(const VpnExtensionObserverAsh&) = delete;
  VpnExtensionObserverAsh& operator=(const VpnExtensionObserverAsh&) = delete;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::VpnExtensionObserver> receiver);

  void SetDelegate(Delegate* delegate);

  // crosapi::mojom::VpnExtensionObserver:
  void OnLacrosVpnExtensionLoaded(const std::string& extension_id,
                                  const std::string& extension_name) override;
  void OnLacrosVpnExtensionUnloaded(const std::string& extension_id) override;

 private:
  void OnLacrosVpnExtensionObserverDisconnected();

  mojo::ReceiverSet<crosapi::mojom::VpnExtensionObserver> receivers_;
  raw_ptr<Delegate> delegate_ = nullptr;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_VPN_EXTENSION_OBSERVER_ASH_H_

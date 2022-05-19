// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_VPN_EXTENSION_OBSERVER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_VPN_EXTENSION_OBSERVER_ASH_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/crosapi/mojom/vpn_extension_observer.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi::mojom::VpnExtensionObserver interface for observing
// Lacros Vpn extensions.
class VpnExtensionObserverAsh : public crosapi::mojom::VpnExtensionObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnLacrosVpnExtensionLoaded(const std::string& extension_id,
                                            const std::string& extension_name) {
    }

    virtual void OnLacrosVpnExtensionUnloaded(const std::string& extension_id) {
    }

    virtual void OnLacrosVpnExtensionObserverDisconnected() {}

   protected:
    ~Observer() override = default;
  };

  VpnExtensionObserverAsh();
  ~VpnExtensionObserverAsh() override;

  VpnExtensionObserverAsh(const VpnExtensionObserverAsh&) = delete;
  VpnExtensionObserverAsh& operator=(const VpnExtensionObserverAsh&) = delete;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::VpnExtensionObserver> receiver);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // crosapi::mojom::VpnExtensionObserver:
  void OnLacrosVpnExtensionLoaded(const std::string& extension_id,
                                  const std::string& extension_name) override;
  void OnLacrosVpnExtensionUnloaded(const std::string& extension_id) override;

 private:
  void OnLacrosVpnExtensionObserverDisconnected();

  mojo::ReceiverSet<crosapi::mojom::VpnExtensionObserver> receivers_;
  base::ObserverList<Observer> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_VPN_EXTENSION_OBSERVER_ASH_H_

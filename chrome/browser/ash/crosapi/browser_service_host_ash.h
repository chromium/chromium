// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_SERVICE_HOST_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_SERVICE_HOST_ASH_H_

#include <set>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/crosapi/crosapi_id.h"
#include "chromeos/crosapi/mojom/browser_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {
namespace mojom {
class BrowserService;
}  // namespace mojom

class BrowserServiceHostObserver;

// Maintains the connection to the registered BrowserService. Currently
// this is for supporting multiple Crosapi clients.
class BrowserServiceHostAsh : public mojom::BrowserServiceHost {
 public:
  BrowserServiceHostAsh();
  BrowserServiceHostAsh(const BrowserServiceHostAsh&) = delete;
  BrowserServiceHostAsh& operator=(const BrowserServiceHostAsh&) = delete;
  ~BrowserServiceHostAsh() override;

  void AddObserver(BrowserServiceHostObserver* observer);
  void RemoveObserver(BrowserServiceHostObserver* observer);

  void BindReceiver(CrosapiId id,
                    mojo::PendingReceiver<mojom::BrowserServiceHost> receiver);

  // crosapi::mojom::BrowserServiceHost
  void AddBrowserService(
      mojo::PendingRemote<mojom::BrowserService> remote) override;
  void RequestRelaunch() override;

 private:
  void AddRemote(CrosapiId id, mojo::Remote<mojom::BrowserService> remote);
  void OnVersionReady(
      CrosapiId id,
      std::unique_ptr<mojo::Remote<mojom::BrowserService>> remote,
      uint32_t version);
  void OnDisconnected(mojo::RemoteSetElementId mojo_id);

  mojo::ReceiverSet<mojom::BrowserServiceHost, CrosapiId> receiver_set_;

  base::ObserverList<BrowserServiceHostObserver> observer_list_;
  mojo::RemoteSet<mojom::BrowserService> remote_set_;

  // Map from RemoteSetElementId to its corresponding CrosapiId.
  std::map<mojo::RemoteSetElementId, CrosapiId> crosapi_map_;
  base::WeakPtrFactory<BrowserServiceHostAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_SERVICE_HOST_ASH_H_

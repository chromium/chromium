// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_service_host_ash.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/crosapi/browser_service_host_observer.h"
#include "chromeos/crosapi/mojom/browser_service.mojom.h"

namespace crosapi {

BrowserServiceHostAsh::BrowserServiceHostAsh() {
  remote_set_.set_disconnect_handler(base::BindRepeating(
      &BrowserServiceHostAsh::OnDisconnected, weak_factory_.GetWeakPtr()));
}

BrowserServiceHostAsh::~BrowserServiceHostAsh() = default;

void BrowserServiceHostAsh::AddObserver(BrowserServiceHostObserver* observer) {
  observer_list_.AddObserver(observer);
}

void BrowserServiceHostAsh::RemoveObserver(
    BrowserServiceHostObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void BrowserServiceHostAsh::AddRemote(
    CrosapiId id,
    mojo::Remote<mojom::BrowserService> remote) {
  // We cannot register |remote| to |remote_set_| at this moment, because then
  // we have no way to obtain Remote<BrowserService> so cannot call
  // QueryVersion. Thus, first we call QueryVersion with keeping the instance on
  // the heap, and register when the version gets ready.
  // TODO(crbug.com/40166270): Nice to clean up this trick. Under the
  // discussion.
  auto new_remote =
      std::make_unique<mojo::Remote<mojom::BrowserService>>(std::move(remote));
  // Preserve the pointer, because new_remote will be bound to the callback.
  auto* remote_ptr = new_remote.get();
  // Remote will be reset if it is disconnected during querying the version.
  new_remote->reset_on_disconnect();
  remote_ptr->QueryVersion(
      base::BindOnce(&BrowserServiceHostAsh::OnVersionReady,
                     weak_factory_.GetWeakPtr(), id, std::move(new_remote)));
}

void BrowserServiceHostAsh::BindReceiver(
    CrosapiId id,
    mojo::PendingReceiver<mojom::BrowserServiceHost> receiver) {
  receiver_set_.Add(this, std::move(receiver), id);
}

void BrowserServiceHostAsh::AddBrowserService(
    mojo::PendingRemote<mojom::BrowserService> remote) {
  AddRemote(receiver_set_.current_context(),
            mojo::Remote<mojom::BrowserService>(std::move(remote)));
}

void BrowserServiceHostAsh::OnVersionReady(
    CrosapiId id,
    std::unique_ptr<mojo::Remote<mojom::BrowserService>> remote,
    uint32_t version) {
  // Preserve the pointer to the proxy object, before registering into
  // |remote_set_|. See the comments in AddRemote, too.
  auto* service = remote->get();
  mojo::RemoteSetElementId mojo_id = remote_set_.Add(std::move(*remote));
  crosapi_map_.emplace(mojo_id, id);

  // Version gets ready and registered to |remote_set_|. Notify connected to
  // observers now.
  for (auto& observer : observer_list_)
    observer.OnBrowserServiceConnected(id, mojo_id, service, version);
}

void BrowserServiceHostAsh::RequestRelaunch() {
  CrosapiId crosapi_id = receiver_set_.current_context();
  for (auto& observer : observer_list_)
    observer.OnBrowserRelaunchRequested(crosapi_id);
}

void BrowserServiceHostAsh::OnDisconnected(mojo::RemoteSetElementId mojo_id) {
  auto it = crosapi_map_.find(mojo_id);
  if (it == crosapi_map_.end())
    return;

  CrosapiId id = it->second;
  crosapi_map_.erase(it);

  // Notify disconnected.
  for (auto& observer : observer_list_)
    observer.OnBrowserServiceDisconnected(id, mojo_id);
}

}  // namespace crosapi

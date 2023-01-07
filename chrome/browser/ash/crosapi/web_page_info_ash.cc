// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/web_page_info_ash.h"

namespace crosapi {

WebPageInfoFactoryAsh::WebPageInfoFactoryAsh() {
  web_page_info_providers_.set_disconnect_handler(base::BindRepeating(
      &WebPageInfoFactoryAsh::OnDisconnected, weak_factory_.GetWeakPtr()));
}

WebPageInfoFactoryAsh::~WebPageInfoFactoryAsh() = default;

void WebPageInfoFactoryAsh::BindReceiver(
    mojo::PendingReceiver<mojom::WebPageInfoFactory> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void WebPageInfoFactoryAsh::RegisterWebPageInfoProvider(
    mojo::PendingRemote<mojom::WebPageInfoProvider> web_page_info_provider) {
  mojo::Remote<mojom::WebPageInfoProvider> remote(
      std::move(web_page_info_provider));
  const auto remote_id = web_page_info_providers_.Add(std::move(remote));

  for (auto& observer : observers_) {
    observer.OnLacrosInstanceRegistered(remote_id);
  }
}

void WebPageInfoFactoryAsh::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WebPageInfoFactoryAsh::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WebPageInfoFactoryAsh::RequestCurrentWebPageInfo(
    const mojo::RemoteSetElementId& remote_id,
    RequestCurrentWebPageInfoCallback callback) {
  DCHECK(web_page_info_providers_.Contains(remote_id));
  web_page_info_providers_.Get(remote_id)->RequestCurrentWebPageInfo(
      std::move(callback));
}

void WebPageInfoFactoryAsh::OnDisconnected(mojo::RemoteSetElementId mojo_id) {
  for (auto& observer : observers_) {
    observer.OnLacrosInstanceDisconnected(mojo_id);
  }
}

}  // namespace crosapi

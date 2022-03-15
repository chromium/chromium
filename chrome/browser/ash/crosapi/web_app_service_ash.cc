// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/web_app_service_ash.h"

namespace crosapi {

WebAppServiceAsh::WebAppServiceAsh() = default;
WebAppServiceAsh::~WebAppServiceAsh() = default;

void WebAppServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::WebAppService> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void WebAppServiceAsh::RegisterWebAppProviderBridge(
    mojo::PendingRemote<mojom::WebAppProviderBridge> web_app_provider_bridge) {
  if (web_app_provider_bridge_.is_bound()) {
    // At the moment only a single registration (from a single client) is
    // supported. The rest will be ignored.
    // TODO(crbug.com/1174246): Support SxS lacros.
    LOG(WARNING) << "WebAppProviderBridge already connected";
    return;
  }
  web_app_provider_bridge_.Bind(std::move(web_app_provider_bridge));
  web_app_provider_bridge_.set_disconnect_handler(base::BindOnce(
      &WebAppServiceAsh::OnBridgeDisconnected, weak_factory_.GetWeakPtr()));
}

mojom::WebAppProviderBridge* WebAppServiceAsh::GetWebAppProviderBridge() {
  // At the moment only a single connection is supported.
  // TODO(crbug.com/1174246): Support SxS lacros.
  if (!web_app_provider_bridge_.is_bound()) {
    return nullptr;
  }
  return web_app_provider_bridge_.get();
}

void WebAppServiceAsh::OnBridgeDisconnected() {
  web_app_provider_bridge_.reset();
}

}  // namespace crosapi

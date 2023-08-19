// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_network_access/chrome_private_network_device_delegate.h"

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/private_network_access/chrome_private_network_device_chooser_desktop.h"
#include "chrome/browser/private_network_access/private_network_device_chooser_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"

ChromePrivateNetworkDeviceDelegate::ChromePrivateNetworkDeviceDelegate() =
    default;
ChromePrivateNetworkDeviceDelegate::~ChromePrivateNetworkDeviceDelegate() =
    default;

void ChromePrivateNetworkDeviceDelegate::RequestPermission(
    content::RenderFrameHost& frame,
    blink::mojom::PrivateNetworkDevicePtr device,
    network::mojom::URLLoaderNetworkServiceObserver::
        OnPrivateNetworkAccessPermissionRequiredCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(false);
#else
  auto controller = std::make_unique<PrivateNetworkDeviceChooserController>(
      &frame, std::move(device),
      base::BindOnce(&ChromePrivateNetworkDeviceDelegate::
                         HandlePrivateNetworkDeviceChooserResult,
                     base::Unretained(this), std::move(callback)));
  chooser_ = ChromePrivateNetworkDeviceChooserDesktop::Create(
      &frame, std::move(controller));
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromePrivateNetworkDeviceDelegate::
    HandlePrivateNetworkDeviceChooserResult(
        network::mojom::URLLoaderNetworkServiceObserver::
            OnPrivateNetworkAccessPermissionRequiredCallback callback,
        bool permission_granted) {
  chooser_ = nullptr;
  std::move(callback).Run(permission_granted);
}

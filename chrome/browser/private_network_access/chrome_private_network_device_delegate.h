// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_CHROME_PRIVATE_NETWORK_DEVICE_DELEGATE_H_
#define CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_CHROME_PRIVATE_NETWORK_DEVICE_DELEGATE_H_

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/private_network_access/chrome_private_network_device_chooser.h"
#include "content/public/browser/private_network_device_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "third_party/blink/public/mojom/private_network_device/private_network_device.mojom.h"

namespace content {
class RenderFrameHost;
}  // namespace content

// Interface to support Private Network permission APIs.
class ChromePrivateNetworkDeviceDelegate
    : public content::PrivateNetworkDeviceDelegate {
 public:
  ChromePrivateNetworkDeviceDelegate();
  ChromePrivateNetworkDeviceDelegate(ChromePrivateNetworkDeviceDelegate&) =
      delete;
  ChromePrivateNetworkDeviceDelegate(ChromePrivateNetworkDeviceDelegate&&) =
      delete;
  ChromePrivateNetworkDeviceDelegate& operator=(
      ChromePrivateNetworkDeviceDelegate&) = delete;
  ChromePrivateNetworkDeviceDelegate& operator=(
      ChromePrivateNetworkDeviceDelegate&&) = delete;
  ~ChromePrivateNetworkDeviceDelegate() override;

  // Request permission for Private Network Device.
  // |callback| will be run when the prompt is closed.
  void RequestPermission(
      content::RenderFrameHost& frame,
      blink::mojom::PrivateNetworkDevicePtr device,
      network::mojom::URLLoaderNetworkServiceObserver::
          OnPrivateNetworkAccessPermissionRequiredCallback callback) override;

 private:
  void HandlePrivateNetworkDeviceChooserResult(
      network::mojom::URLLoaderNetworkServiceObserver::
          OnPrivateNetworkAccessPermissionRequiredCallback callback,
      bool permission_granted);

  // The currently-displayed private network device chooser prompt, if any.
  //
  // If a new permission request comes while a chooser was already being
  // displayed, the old one is canceled when we reassign this field.
  // TODO(https://crbug.com/1455117): Handle multiple permission checks
  // better, perhaps by serializing them.
  std::unique_ptr<ChromePrivateNetworkDeviceChooser> chooser_;
};

#endif  // CHROME_BROWSER_PRIVATE_NETWORK_DEVICE_DELEGATE_H_

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
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/private_network_device/private_network_device.mojom.h"

namespace url {
class Origin;
}

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

// Interface to support Private Network permission APIs.
class ChromePrivateNetworkDeviceDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDeviceManagerConnectionError() = 0;
    virtual void OnPermissionRevoked(const url::Origin& origin) = 0;
  };

  ChromePrivateNetworkDeviceDelegate();
  ChromePrivateNetworkDeviceDelegate(ChromePrivateNetworkDeviceDelegate&) =
      delete;
  ChromePrivateNetworkDeviceDelegate(ChromePrivateNetworkDeviceDelegate&&) =
      delete;
  ChromePrivateNetworkDeviceDelegate& operator=(
      ChromePrivateNetworkDeviceDelegate&) = delete;
  ChromePrivateNetworkDeviceDelegate& operator=(
      ChromePrivateNetworkDeviceDelegate&&) = delete;
  ~ChromePrivateNetworkDeviceDelegate();

  // Shows a chooser for the user. `callback` will be run when the prompt is
  // closed. Deleting the returned object will cancel the prompt.
  std::unique_ptr<ChromePrivateNetworkDeviceChooser> RunChooser(
      content::RenderFrameHost& frame,
      std::unique_ptr<blink::mojom::PrivateNetworkDevice> device,
      ChromePrivateNetworkDeviceChooser::EventHandler event_handler);

  // Functions to manage the set of Observer instances registered to this
  // object.
  void AddObserver(content::BrowserContext* browser_context,
                   Observer* observer);
  void RemoveObserver(content::BrowserContext* browser_context,
                      Observer* observer);

 private:
  base::ObserverList<ChromePrivateNetworkDeviceDelegate::Observer,
                     /*check_empty=*/true>
      observer_list_;
};

#endif  // CHROME_BROWSER_PRIVATE_NETWORK_DEVICE_DELEGATE_H_

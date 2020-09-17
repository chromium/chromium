// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BLUETOOTH_CHROME_BLUETOOTH_DELEGATE_H_
#define CHROME_BROWSER_BLUETOOTH_CHROME_BLUETOOTH_DELEGATE_H_

#include <string>
#include <vector>

#include "content/public/browser/bluetooth_delegate.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-forward.h"

namespace blink {
class WebBluetoothDeviceId;
}  // namespace blink

namespace content {
class RenderFrameHost;
}  // namespace content

namespace device {
class BluetoothDevice;
class BluetoothUUID;
}  // namespace device

// Provides an interface for managing device permissions for Web Bluetooth and
// Web Bluetooth Scanning API. This is the Chrome-specific implementation of the
// BluetoothDelegate.
class ChromeBluetoothDelegate : public content::BluetoothDelegate {
 public:
  ChromeBluetoothDelegate();
  ~ChromeBluetoothDelegate() override;

  // Move-only class.
  ChromeBluetoothDelegate(const ChromeBluetoothDelegate&) = delete;
  ChromeBluetoothDelegate& operator=(const ChromeBluetoothDelegate&) = delete;

  // BluetoothDelegate implementation:
  blink::WebBluetoothDeviceId GetWebBluetoothDeviceId(
      content::RenderFrameHost* frame,
      const std::string& device_address) override;
  std::string GetDeviceAddress(
      content::RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override;
  blink::WebBluetoothDeviceId AddScannedDevice(
      content::RenderFrameHost* frame,
      const std::string& device_address) override;
  blink::WebBluetoothDeviceId GrantServiceAccessPermission(
      content::RenderFrameHost* frame,
      const device::BluetoothDevice* device,
      const blink::mojom::WebBluetoothRequestDeviceOptions* options) override;
  bool HasDevicePermission(
      content::RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override;
  bool IsAllowedToAccessService(content::RenderFrameHost* frame,
                                const blink::WebBluetoothDeviceId& device_id,
                                const device::BluetoothUUID& service) override;
  bool IsAllowedToAccessAtLeastOneService(
      content::RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override;
  bool IsAllowedToAccessManufacturerData(
      content::RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id,
      uint16_t manufacturer_code) override;
  std::vector<blink::mojom::WebBluetoothDevicePtr> GetPermittedDevices(
      content::RenderFrameHost* frame) override;
};

#endif  // CHROME_BROWSER_BLUETOOTH_CHROME_BLUETOOTH_DELEGATE_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BLUETOOTH_CHROME_BLUETOOTH_DELEGATE_H_
#define CHROME_BROWSER_BLUETOOTH_CHROME_BLUETOOTH_DELEGATE_H_

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/permissions/chooser_context_base.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "content/public/browser/render_frame_host.h"
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
  std::unique_ptr<content::BluetoothChooser> RunBluetoothChooser(
      content::RenderFrameHost* frame,
      const content::BluetoothChooser::EventHandler& event_handler) override;
  std::unique_ptr<content::BluetoothScanningPrompt> ShowBluetoothScanningPrompt(
      content::RenderFrameHost* frame,
      const content::BluetoothScanningPrompt::EventHandler& event_handler)
      override;
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
  void AddFramePermissionObserver(FramePermissionObserver* observer) override;
  void RemoveFramePermissionObserver(
      FramePermissionObserver* observer) override;

 private:
  // Manages the FramePermissionObserver list for a particular RFH. Will
  // self-delete when the last observer is removed from the |owning_delegate|'s
  // |chooser_observers_| map.
  class ChooserContextPermissionObserver
      : public permissions::ChooserContextBase::PermissionObserver {
   public:
    explicit ChooserContextPermissionObserver(
        ChromeBluetoothDelegate* owning_delegate,
        permissions::ChooserContextBase* context);
    ~ChooserContextPermissionObserver() override;

    ChooserContextPermissionObserver(const ChooserContextPermissionObserver&) =
        delete;
    ChooserContextPermissionObserver& operator=(
        const ChooserContextPermissionObserver) = delete;

    // permissions::ChooserContextBase::PermissionObserver:
    void OnPermissionRevoked(const url::Origin& origin) override;

    void AddFramePermissionObserver(FramePermissionObserver* observer);
    void RemoveFramePermissionObserver(FramePermissionObserver* observer);

   private:
    ChromeBluetoothDelegate* owning_delegate_;
    base::ObserverList<FramePermissionObserver> observer_list_;
    std::list<FramePermissionObserver*> observers_pending_removal_;
    bool is_traversing_observers_ = false;
    base::ScopedObservation<permissions::ChooserContextBase,
                            permissions::ChooserContextBase::PermissionObserver>
        observer_{this};
  };

  std::map<content::RenderFrameHost*,
           std::unique_ptr<ChooserContextPermissionObserver>>
      chooser_observers_;
};

#endif  // CHROME_BROWSER_BLUETOOTH_CHROME_BLUETOOTH_DELEGATE_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_FLOSS_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_FLOSS_BRIDGE_H_

#include "ash/components/arc/mojom/bluetooth.mojom.h"
#include "chrome/browser/ash/arc/bluetooth/arc_bluetooth_bridge.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"

namespace arc {

// Floss specialization for Arc Bluetooth bridge. Use this class whenever the
// common |device::BluetoothAdapter| or |device::BluetoothDevice| class apis are
// insufficient.
class ArcFlossBridge : public ArcBluetoothBridge {
 public:
  ArcFlossBridge(content::BrowserContext* context,
                 ArcBridgeService* bridge_service);
  ~ArcFlossBridge() override;

  ArcFlossBridge(const ArcFlossBridge&) = delete;
  ArcFlossBridge& operator=(const ArcFlossBridge&) = delete;

  // ArcBluetoothBridge overrides
  void SendCachedDevices() const override;

  // Bluetooth Mojo host interface - Bluetooth SDP functions
  void GetSdpRecords(mojom::BluetoothAddressPtr remote_addr,
                     const device::BluetoothUUID& target_uuid) override;
  void CreateSdpRecord(mojom::BluetoothSdpRecordPtr record_mojo,
                       CreateSdpRecordCallback callback) override;
  void RemoveSdpRecord(uint32_t service_handle,
                       RemoveSdpRecordCallback callback) override;

 protected:
  floss::BluetoothAdapterFloss* GetAdapter() const;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_FLOSS_BRIDGE_H_

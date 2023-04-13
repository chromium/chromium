// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_BLUEZ_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_BLUEZ_BRIDGE_H_

#include "ash/components/arc/mojom/bluetooth.mojom.h"
#include "chrome/browser/ash/arc/bluetooth/arc_bluetooth_bridge.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"

namespace arc {

// Bluez specialization for Arc Bluetooth bridge. Use this class whenever the
// common |device::BluetoothAdapter| or |device::BluetoothDevice| class apis are
// insufficient.
class ArcBluezBridge : public ArcBluetoothBridge {
 public:
  ArcBluezBridge(content::BrowserContext* context,
                 ArcBridgeService* bridge_service);
  ~ArcBluezBridge() override;

  ArcBluezBridge(const ArcBluezBridge&) = delete;
  ArcBluezBridge& operator=(const ArcBluezBridge&) = delete;

  // Bluetooth Mojo host interface - Bluetooth SDP functions
  void GetSdpRecords(mojom::BluetoothAddressPtr remote_addr,
                     const device::BluetoothUUID& target_uuid) override;
  void CreateSdpRecord(mojom::BluetoothSdpRecordPtr record_mojo,
                       CreateSdpRecordCallback callback) override;
  void RemoveSdpRecord(uint32_t service_handle,
                       RemoveSdpRecordCallback callback) override;

  // Closes Bluetooth sockets. Releases the corresponding resources.
  void CloseBluetoothListeningSocket(BluetoothListeningSocket* socket);
  void CloseBluetoothConnectingSocket(BluetoothConnectingSocket* socket);

 protected:
  bluez::BluetoothAdapterBlueZ* GetAdapter() const;

  void HandlePoweredOn() override;

  void OnGetServiceRecordsDone(
      mojom::BluetoothAddressPtr remote_addr,
      const device::BluetoothUUID& target_uuid,
      const std::vector<bluez::BluetoothServiceRecordBlueZ>& records_bluez);
  void OnGetServiceRecordsError(
      mojom::BluetoothAddressPtr remote_addr,
      const device::BluetoothUUID& target_uuid,
      bluez::BluetoothServiceRecordBlueZ::ErrorCode error_code);

  // Called when the listening socket is ready to accept().
  void OnBluezListeningSocketReady(BluetoothListeningSocket* sock_wrapper);

  // Called when the connecting socket is ready.
  void OnBluezConnectingSocketReady(BluetoothConnectingSocket* sock_wrapper);

  void CreateBluetoothListenSocket(
      mojom::BluetoothSocketType type,
      mojom::BluetoothSocketFlagsPtr flags,
      int port,
      BluetoothSocketListenCallback callback) override;

  void CreateBluetoothConnectSocket(
      mojom::BluetoothSocketType type,
      mojom::BluetoothSocketFlagsPtr flags,
      mojom::BluetoothAddressPtr addr,
      int port,
      BluetoothSocketConnectCallback callback) override;

 private:
  // Bluetooth sockets that live in Chrome.
  std::set<std::unique_ptr<BluetoothListeningSocket>, base::UniquePtrComparator>
      listening_sockets_;
  std::set<std::unique_ptr<BluetoothConnectingSocket>,
           base::UniquePtrComparator>
      connecting_sockets_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcBluezBridge> weak_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_BLUEZ_BRIDGE_H_

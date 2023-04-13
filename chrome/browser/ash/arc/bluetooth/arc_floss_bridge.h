// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_FLOSS_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_FLOSS_BRIDGE_H_

#include "ash/components/arc/mojom/bluetooth.mojom.h"
#include "chrome/browser/ash/arc/bluetooth/arc_bluetooth_bridge.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/floss_adapter_client.h"

namespace arc {

// Floss specialization for Arc Bluetooth bridge. Use this class whenever the
// common |device::BluetoothAdapter| or |device::BluetoothDevice| class apis are
// insufficient.
class ArcFlossBridge : public ArcBluetoothBridge,
                       public floss::FlossAdapterClient::Observer {
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

  // Closes Bluetooth sockets. Releases the corresponding resources.
  void CloseBluetoothListeningSocket(BluetoothListeningSocket* socket);
  void CloseBluetoothConnectingSocket(BluetoothConnectingSocket* socket);

  // floss::FlossAdapterClient::Observer overrides
  void SdpSearchComplete(
      const floss::FlossDeviceId device,
      const device::BluetoothUUID uuid,
      const std::vector<floss::BtSdpRecord> records) override;

  void SdpRecordCreated(const floss::BtSdpRecord record,
                        const int32_t handle) override;

 protected:
  floss::BluetoothAdapterFloss* GetAdapter() const;

  void HandlePoweredOn() override;

  void OnSdpSearchResult(mojom::BluetoothAddressPtr remote_addr,
                         const device::BluetoothUUID& target_uuid,
                         floss::DBusResult<bool> result);

  void RemoveSdpRecordComplete(RemoveSdpRecordCallback callback,
                               floss::DBusResult<bool> result);

  void CreateSdpRecordComplete(device::BluetoothUUID uuid,
                               floss::DBusResult<bool> result);

  void CompleteCreateSdpRecord(
      device::BluetoothUUID uuid,
      arc::mojom::BluetoothCreateSdpRecordResultPtr result);

 private:
  bool AdapterReadyAndRegistered();

  // Map of SDP record UUIDs to CreateSdpRecordCallback for SdpRecordCreated to
  // use to resolve CreateSdpRecord calls.
  base::flat_map<device::BluetoothUUID, CreateSdpRecordCallback>
      create_sdp_record_callbacks_{};

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

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcFlossBridge> weak_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_FLOSS_BRIDGE_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_FLOSS_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_FLOSS_BRIDGE_H_

#include "ash/components/arc/mojom/bluetooth.mojom.h"
#include "chrome/browser/ash/arc/bluetooth/arc_bluetooth_bridge.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"
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
  void StartLEScanImpl() override;
  void ResetLEScanSession() override;
  bool IsDiscoveringOrScanning() override;

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

  void CreateBluetoothListenSocket(
      mojom::BluetoothSocketType type,
      mojom::BluetoothSocketFlagsPtr flags,
      int port,
      BluetoothSocketListenCallback callback) override;

  void OnCloseBluetoothListeningSocketComplete(
      BluetoothListeningSocket* socket,
      floss::DBusResult<floss::FlossDBusClient::BtifStatus> result);

  void CreateBluetoothConnectSocket(
      mojom::BluetoothSocketType type,
      mojom::BluetoothSocketFlagsPtr flags,
      mojom::BluetoothAddressPtr addr,
      int port,
      BluetoothSocketConnectCallback callback) override;

  void OnCreateListenSocketCallback(
      std::unique_ptr<ArcBluetoothBridge::BluetoothListeningSocket>
          sock_wrapper,
      int socket_ready_id,
      floss::DBusResult<floss::FlossDBusClient::BtifStatus> result);

  void OnCreateConnectSocketCallback(
      std::unique_ptr<ArcBluetoothBridge::BluetoothConnectingSocket>
          sock_wrapper,
      ArcFlossBridge::BluetoothSocketConnectCallback callback,
      floss::FlossDBusClient::BtifStatus status,
      std::optional<floss::FlossSocketManager::FlossSocket>&& socket);

  void OnBtifError(const std::string& error_message);

  void OnBtifSuccess();

  void OnConnectionStateChanged(
      ArcBluetoothBridge::BluetoothListeningSocket* sock_wrapper,
      int socket_ready_id,
      floss::FlossSocketManager::ServerSocketState state,
      floss::FlossSocketManager::FlossListeningSocket socket,
      floss::FlossSocketManager::BtifStatus status);

  void OnConnectionAccepted(
      const ArcBluetoothBridge::BluetoothListeningSocket* sock_wrapper,
      floss::FlossSocketManager::FlossSocket&& socket);

  int GetPortOrChannel(
      ArcBluetoothBridge::BluetoothListeningSocket* sock_wrapper,
      floss::FlossSocketManager::FlossListeningSocket socket);

  void CompleteListenSocketReady(
      int socket_ready_id,
      mojom::BluetoothStatus status,
      int port_or_channel,
      ArcBluetoothBridge::BluetoothListeningSocket* sock_wrapper);

  void CompleteListenSocketReady(
      int socket_ready_id,
      mojom::BluetoothStatus status,
      ArcBluetoothBridge::BluetoothListeningSocket* sock_wrapper);

  void CompleteListenSocketReady(
      int socket_ready_id,
      mojom::BluetoothStatus status,
      ArcBluetoothBridge::BluetoothListeningSocket* sock_wrapper,
      floss::FlossSocketManager::FlossListeningSocket socket);

 private:
  bool AdapterReadyAndRegistered();

  // Map of socket_ready_id -> listen callback
  base::flat_map<int, BluetoothSocketListenCallback> socket_ready_callbacks_{};
  // The next callback id to use. Starts at a non-zero number for easier
  // debugging.
  int next_socket_ready_callback_id_{42};

  // Map of SDP record UUIDs to CreateSdpRecordCallback for SdpRecordCreated to
  // use to resolve CreateSdpRecord calls.
  base::flat_map<device::BluetoothUUID, CreateSdpRecordCallback>
      create_sdp_record_callbacks_{};

  // Lookup for mapping listening socket pointers to SocketIds so they can be
  // closed with Floss.
  base::flat_map<
      ArcBluetoothBridge::BluetoothListeningSocket*,
      std::pair<std::unique_ptr<ArcBluetoothBridge::BluetoothListeningSocket>,
                floss::FlossSocketManager::SocketId>>
      listening_sockets_{};
  // Default SocketId until a real one is obtained.
  static constexpr floss::FlossSocketManager::SocketId empty_socket_id_ = 0;

  // Floss doesn't require us to close connecting sockets through it so we don't
  // need to keep the SocketId.
  std::set<std::unique_ptr<BluetoothConnectingSocket>,
           base::UniquePtrComparator>
      connecting_sockets_;

  // Map of device+channel combinations to service UUIDs
  base::flat_map<std::pair<std::string, int32_t>, device::BluetoothUUID>
      uuid_lookups_{};

  // LE scan session created by StartLEScanImpl()
  std::unique_ptr<device::BluetoothLowEnergyScanSession> ble_scan_session_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcFlossBridge> weak_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_FLOSS_BRIDGE_H_

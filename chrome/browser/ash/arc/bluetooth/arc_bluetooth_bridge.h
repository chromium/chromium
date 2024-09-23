// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_BLUETOOTH_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_BLUETOOTH_BRIDGE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ash/components/arc/mojom/bluetooth.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom-forward.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/arc/bluetooth/arc_bluetooth_task_queue.h"
#include "components/keyed_service/core/keyed_service.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

namespace mojom {
class AppInstance;
class IntentHelperInstance;
}  // namespace mojom

class ArcBridgeService;

class ArcBluetoothBridge
    : public KeyedService,
      public device::BluetoothAdapter::Observer,
      public device::BluetoothAdapterFactory::AdapterCallback,
      public device::BluetoothLocalGattService::Delegate,
      public device::BluetoothLowEnergyScanSession::Delegate,
      public ConnectionObserver<mojom::AppInstance>,
      public ConnectionObserver<mojom::IntentHelperInstance>,
      public mojom::BluetoothHost {
 public:
  using GattStatusCallback =
      base::OnceCallback<void(mojom::BluetoothGattStatus)>;
  using AdapterStateCallback =
      base::OnceCallback<void(mojom::BluetoothAdapterState)>;
  using GattReadCallback =
      base::OnceCallback<void(arc::mojom::BluetoothGattValuePtr)>;
  using CreateSdpRecordCallback =
      base::OnceCallback<void(arc::mojom::BluetoothCreateSdpRecordResultPtr)>;
  using RemoveSdpRecordCallback =
      base::OnceCallback<void(arc::mojom::BluetoothStatus)>;

  static constexpr int kAutoSockPort = 0;
  static constexpr int kMinRfcommChannel = 1;
  static constexpr int kMaxRfcommChannel = 30;

  // Copied from the values of L2CAP_PSM_LE_DYN_START and L2CAP_PSM_LE_DYN_END
  // in /include/net/bluetooth/l2cap.h
  static constexpr int kMinL2capLePsm = 0x0080;
  static constexpr int kMaxL2capLePsm = 0x00FF;

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcBluetoothBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcBluetoothBridge(content::BrowserContext* context,
                     ArcBridgeService* bridge_service);

  ArcBluetoothBridge(const ArcBluetoothBridge&) = delete;
  ArcBluetoothBridge& operator=(const ArcBluetoothBridge&) = delete;

  ~ArcBluetoothBridge() override;

  void OnAdapterInitialized(scoped_refptr<device::BluetoothAdapter> adapter);

  // Overridden from device::BluetoothAdapter::Observer
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;

  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  void DeviceAddressChanged(device::BluetoothAdapter* adapter,
                            device::BluetoothDevice* device,
                            const std::string& old_address) override;

  void DevicePairedChanged(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device,
                           bool new_paired_status) override;

  void DeviceMTUChanged(device::BluetoothAdapter* adapter,
                        device::BluetoothDevice* device,
                        uint16_t mtu) override;

  void DeviceAdvertisementReceived(device::BluetoothAdapter* adapter,
                                   device::BluetoothDevice* device,
                                   int16_t rssi,
                                   const std::vector<uint8_t>& eir) override;

  void DeviceConnectedStateChanged(device::BluetoothAdapter* adapter,
                                   device::BluetoothDevice* device,
                                   bool is_now_connected) override;

  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  void GattServiceAdded(device::BluetoothAdapter* adapter,
                        device::BluetoothDevice* device,
                        device::BluetoothRemoteGattService* service) override;

  void GattServiceRemoved(device::BluetoothAdapter* adapter,
                          device::BluetoothDevice* device,
                          device::BluetoothRemoteGattService* service) override;

  void GattServicesDiscovered(device::BluetoothAdapter* adapter,
                              device::BluetoothDevice* device) override;

  void GattDiscoveryCompleteForService(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattService* service) override;

  void GattNeedsDiscovery(device::BluetoothDevice* device) override;

  void GattServiceChanged(device::BluetoothAdapter* adapter,
                          device::BluetoothRemoteGattService* service) override;

  void GattCharacteristicAdded(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic) override;

  void GattCharacteristicRemoved(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic) override;

  void GattDescriptorAdded(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattDescriptor* descriptor) override;

  void GattDescriptorRemoved(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattDescriptor* descriptor) override;

  void GattCharacteristicValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;

  void GattDescriptorValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattDescriptor* descriptor,
      const std::vector<uint8_t>& value) override;

  // Overridden from device::BluetoothLocalGattService::Delegate
  void OnCharacteristicReadRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattCharacteristic* characteristic,
      int offset,
      ValueCallback callback) override;

  void OnCharacteristicWriteRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value,
      int offset,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;

  void OnCharacteristicPrepareWriteRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value,
      int offset,
      bool has_subsequent_write,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;

  void OnDescriptorReadRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattDescriptor* descriptor,
      int offset,
      ValueCallback callback) override;

  void OnDescriptorWriteRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattDescriptor* descriptor,
      const std::vector<uint8_t>& value,
      int offset,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;

  void OnNotificationsStart(
      const device::BluetoothDevice* device,
      device::BluetoothGattCharacteristic::NotificationType notification_type,
      const device::BluetoothLocalGattCharacteristic* characteristic) override;

  void OnNotificationsStop(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattCharacteristic* characteristic) override;

  // device::BluetoothLowEnergyScanSession::Delegate:
  void OnDeviceFound(device::BluetoothLowEnergyScanSession* scan_session,
                     device::BluetoothDevice* device) override;

  void OnDeviceLost(device::BluetoothLowEnergyScanSession* scan_session,
                    device::BluetoothDevice* device) override;

  void OnSessionStarted(
      device::BluetoothLowEnergyScanSession* scan_session,
      std::optional<device::BluetoothLowEnergyScanSession::ErrorCode>
          error_code) override;

  void OnSessionInvalidated(
      device::BluetoothLowEnergyScanSession* scan_session) override;

  // Bluetooth Mojo host interface
  void EnableAdapter(EnableAdapterCallback callback) override;
  void DisableAdapter(DisableAdapterCallback callback) override;

  void GetAdapterProperty(mojom::BluetoothPropertyType type) override;
  void SetAdapterProperty(mojom::BluetoothPropertyPtr property) override;

  void StartDiscovery() override;
  void CancelDiscovery() override;

  void CreateBond(mojom::BluetoothAddressPtr addr, int32_t transport) override;
  void RemoveBond(mojom::BluetoothAddressPtr addr) override;
  void CancelBond(mojom::BluetoothAddressPtr addr) override;

  void GetConnectionState(mojom::BluetoothAddressPtr addr,
                          GetConnectionStateCallback callback) override;

  // Bluetooth Mojo host interface - Bluetooth Gatt Client functions
  void StartLEScan() override;
  void StopLEScan() override;
  void ConnectLEDevice(mojom::BluetoothAddressPtr remote_addr) override;
  void DisconnectLEDevice(mojom::BluetoothAddressPtr remote_addr) override;
  void SearchService(mojom::BluetoothAddressPtr remote_addr) override;

  void GetGattDB(mojom::BluetoothAddressPtr remote_addr) override;
  void ReadGattCharacteristic(mojom::BluetoothAddressPtr remote_addr,
                              mojom::BluetoothGattServiceIDPtr service_id,
                              mojom::BluetoothGattIDPtr char_id,
                              ReadGattCharacteristicCallback callback) override;
  void WriteGattCharacteristic(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id,
      mojom::BluetoothGattValuePtr value,
      bool prepare,
      WriteGattCharacteristicCallback callback) override;
  void ReadGattDescriptor(mojom::BluetoothAddressPtr remote_addr,
                          mojom::BluetoothGattServiceIDPtr service_id,
                          mojom::BluetoothGattIDPtr char_id,
                          mojom::BluetoothGattIDPtr desc_id,
                          ReadGattDescriptorCallback callback) override;
  void WriteGattDescriptor(mojom::BluetoothAddressPtr remote_addr,
                           mojom::BluetoothGattServiceIDPtr service_id,
                           mojom::BluetoothGattIDPtr char_id,
                           mojom::BluetoothGattIDPtr desc_id,
                           mojom::BluetoothGattValuePtr value,
                           WriteGattDescriptorCallback callback) override;
  void ExecuteWrite(mojom::BluetoothAddressPtr remote_addr,
                    bool execute,
                    ExecuteWriteCallback callback) override;
  void RegisterForGattNotification(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id,
      RegisterForGattNotificationCallback callback) override;
  void DeregisterForGattNotification(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id,
      DeregisterForGattNotificationCallback callback) override;
  void ReadRemoteRssi(mojom::BluetoothAddressPtr remote_addr,
                      ReadRemoteRssiCallback callback) override;

  // Bluetooth Mojo host interface - Bluetooth Gatt Server functions
  // Android counterpart link:
  // https://source.android.com/devices/halref/bt__gatt__server_8h.html
  // Create a new service. Chrome will create an integer service handle based on
  // that BlueZ identifier that will pass back to Android in the callback.
  // num_handles: number of handle for characteristic / descriptor that will be
  //              created in this service
  void AddService(mojom::BluetoothGattServiceIDPtr service_id,
                  int32_t num_handles,
                  AddServiceCallback callback) override;
  // Add a characteristic to a service and pass the characteristic handle back.
  void AddCharacteristic(int32_t service_handle,
                         const device::BluetoothUUID& uuid,
                         int32_t properties,
                         int32_t permissions,
                         AddCharacteristicCallback callback) override;
  // Add a descriptor to the last characteristic added to the given service
  // and pass the descriptor handle back.
  void AddDescriptor(int32_t service_handle,
                     const device::BluetoothUUID& uuid,
                     int32_t permissions,
                     AddDescriptorCallback callback) override;
  // Start a local service.
  void StartService(int32_t service_handle,
                    StartServiceCallback callback) override;
  // Stop a local service.
  void StopService(int32_t service_handle,
                   StopServiceCallback callback) override;
  // Delete a local service.
  void DeleteService(int32_t service_handle,
                     DeleteServiceCallback callback) override;
  // Send value indication to a remote device.
  void SendIndication(int32_t attribute_handle,
                      mojom::BluetoothAddressPtr address,
                      bool confirm,
                      const std::vector<uint8_t>& value,
                      SendIndicationCallback callback) override;

  // Bluetooth Mojo host interface - Bluetooth socket functions
  void BluetoothSocketListen(mojom::BluetoothSocketType sock_type,
                             mojom::BluetoothSocketFlagsPtr sock_flags,
                             int32_t port,
                             BluetoothSocketListenCallback callback) override;
  void BluetoothSocketConnect(mojom::BluetoothSocketType sock_type,
                              mojom::BluetoothSocketFlagsPtr sock_flags,
                              mojom::BluetoothAddressPtr remote_addr,
                              int32_t port,
                              BluetoothSocketConnectCallback callback) override;

  // Set up or disable multiple advertising.
  void ReserveAdvertisementHandle(
      ReserveAdvertisementHandleCallback callback) override;
  void EnableAdvertisement(
      int32_t adv_handle,
      std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement,
      EnableAdvertisementCallback callback) override;
  void DisableAdvertisement(int32_t adv_handle,
                            DisableAdvertisementCallback callback) override;
  void ReleaseAdvertisementHandle(
      int32_t adv_handle,
      ReleaseAdvertisementHandleCallback callback) override;

  static void EnsureFactoryBuilt();

 protected:
  friend class ArcBluetoothBridgeTest;

  virtual void HandlePoweredOn() = 0;

  void ReserveAdvertisementHandleImpl(
      ReserveAdvertisementHandleCallback callback);
  void EnableAdvertisementImpl(
      int32_t adv_handle,
      std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement,
      EnableAdvertisementCallback callback);
  void DisableAdvertisementImpl(int32_t adv_handle,
                                DisableAdvertisementCallback callback);
  void ReleaseAdvertisementHandleImpl(
      int32_t adv_handle,
      ReleaseAdvertisementHandleCallback callback);

  // StartDiscovery() is used for scanning both BR/EDR and LE devices, while
  // StartLEScan() is only for LE devices.
  void StartDiscoveryImpl();
  void CancelDiscoveryImpl();
  virtual void StartLEScanImpl();
  void StopLEScanImpl();

  virtual void ResetLEScanSession();
  virtual bool IsDiscoveringOrScanning();
  void StartLEScanOffTimer();

  // The callback function triggered by le_scan_off_timer_.
  void StopLEScanByTimer();

  // Power state change on Bluetooth adapter.
  enum class AdapterPowerState { TURN_OFF, TURN_ON };

  // Chrome observer callbacks
  void OnPoweredOn(AdapterStateCallback callback, bool save_user_pref);
  void OnPoweredOff(AdapterStateCallback callback, bool save_user_pref);
  void OnPoweredError(AdapterStateCallback callback) const;
  void OnDiscoveryStarted(
      std::unique_ptr<device::BluetoothDiscoverySession> session);
  void OnDiscoveryError();
  void OnLEScanStarted(
      std::unique_ptr<device::BluetoothDiscoverySession> session);
  void OnLEScanError();
  void OnPairing(mojom::BluetoothAddressPtr addr) const;
  void OnPairedDone(mojom::BluetoothAddressPtr addr) const;
  void OnPairedError(
      mojom::BluetoothAddressPtr addr,
      device::BluetoothDevice::ConnectErrorCode error_code) const;
  void OnForgetDone(mojom::BluetoothAddressPtr addr);
  void OnForgetError(mojom::BluetoothAddressPtr addr) const;

  void OnGetServiceRecordsFinished(
      mojom::BluetoothAddressPtr remote_addr,
      const device::BluetoothUUID& target_uuid,
      const std::vector<bluez::BluetoothServiceRecordBlueZ>& records_bluez);
  void OnGetServiceRecordsError(
      mojom::BluetoothAddressPtr remote_addr,
      const device::BluetoothUUID& target_uuid,
      bluez::BluetoothServiceRecordBlueZ::ErrorCode error_code);

  void OnGattConnectStateChanged(mojom::BluetoothAddressPtr addr,
                                 bool connected) const;
  void OnGattConnect(
      mojom::BluetoothAddressPtr addr,
      std::unique_ptr<device::BluetoothGattConnection> connection,
      std::optional<device::BluetoothDevice::ConnectErrorCode> error_code);
  void OnGattDisconnected(mojom::BluetoothAddressPtr addr);

  void OnGattNotifyStartDone(
      GattStatusCallback callback,
      const std::string char_string_id,
      std::unique_ptr<device::BluetoothGattNotifySession> notify_session);

  // Indicates if a power change is initiated by Chrome / Android.
  bool IsPowerChangeInitiatedByRemote(
      ArcBluetoothBridge::AdapterPowerState powered) const;
  bool IsPowerChangeInitiatedByLocal(
      ArcBluetoothBridge::AdapterPowerState powered) const;

  // ConnectionObserver<mojom::AppInstance>:
  // ConnectionObserver<mojom::IntentHelperInstance>:
  void OnConnectionReady() override;

  // Manages the powered change intents sent to Android.
  void EnqueueLocalPowerChange(AdapterPowerState powered);
  void DequeueLocalPowerChange(AdapterPowerState powered);

  // Manages the powered change requests received from Android.
  void EnqueueRemotePowerChange(AdapterPowerState powered,
                                AdapterStateCallback callback);
  void DequeueRemotePowerChange(AdapterPowerState powered);

  // Sends properties of cached devices to Android. The list of cached devices
  // is got by BluetoothAdapter::GetDevices(), which includes all devices have
  // been discovered (not necessarily paired or connected) but not yet expired.
  // This function should be called when Bluetooth service in Android is ready.
  virtual void SendCachedDevices() const;

  std::vector<mojom::BluetoothPropertyPtr> GetDeviceProperties(
      mojom::BluetoothPropertyType type,
      const device::BluetoothDevice* device) const;
  std::vector<mojom::BluetoothPropertyPtr> GetAdapterProperties(
      mojom::BluetoothPropertyType type) const;
  std::vector<mojom::BluetoothAdvertisingDataPtr> GetAdvertisingData(
      const device::BluetoothDevice* device) const;

  device::BluetoothRemoteGattCharacteristic* FindGattCharacteristic(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id) const;

  device::BluetoothRemoteGattDescriptor* FindGattDescriptor(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id,
      mojom::BluetoothGattIDPtr desc_id) const;

  // Send the power status change to Android via an intent.
  void SendBluetoothPoweredStateBroadcast(AdapterPowerState powered) const;

  bool IsGattServerAttributeHandleAvailable(int need);
  int32_t GetNextGattServerAttributeHandle();
  template <class LocalGattAttribute>
  int32_t CreateGattAttributeHandle(LocalGattAttribute* attribute);

  // Common code for OnCharacteristicReadRequest and OnDescriptorReadRequest
  template <class LocalGattAttribute>
  void OnGattAttributeReadRequest(
      const device::BluetoothDevice* device,
      const LocalGattAttribute* attribute,
      int offset,
      mojom::BluetoothGattDBAttributeType attribute_type,
      ValueCallback callback);

  // Common code for OnCharacteristicWriteRequest and OnDescriptorWriteRequest
  // |is_prepare| is only set when a local characteristic receives a prepare
  // write request, and |has_subsequent_write| indicates whether there are
  // subsequent prepare write requests following the current one.
  template <class LocalGattAttribute>
  void OnGattAttributeWriteRequest(
      const device::BluetoothDevice* device,
      const LocalGattAttribute* attribute,
      const std::vector<uint8_t>& value,
      int offset,
      mojom::BluetoothGattDBAttributeType attribute_type,
      bool is_prepare,
      bool has_subsequent_write,
      base::OnceClosure success_callback,
      ErrorCallback error_callback);

  void OnSetDiscoverable(bool discoverable, bool success, uint32_t timeout);
  void SetDiscoverable(bool discoverable, uint32_t timeout);

  void OnSetAdapterProperty(mojom::BluetoothStatus success,
                            mojom::BluetoothPropertyPtr property);

  // Convenience function to set primary user's bluetooth pref.
  void SetPrimaryUserBluetoothPowerSetting(bool enabled) const;

  // Callbacks for managing advertisements registered from the instance.

  // Called when we have an open slot in the advertisement map and want to
  // register the advertisement given by |data| for handle |adv_handle|.
  void OnReadyToRegisterAdvertisement(
      GattStatusCallback callback,
      int32_t adv_handle,
      std::unique_ptr<device::BluetoothAdvertisement::Data> data);
  // Called when we've successfully registered a new advertisement for
  // handle |adv_handle|.
  void OnRegisterAdvertisementDone(
      GattStatusCallback callback,
      int32_t adv_handle,
      scoped_refptr<device::BluetoothAdvertisement> advertisement);
  // Called when the attempt to register an advertisement for handle
  // |adv_handle| has failed. |adv_handle| remains reserved, but no
  // advertisement is associated with it.
  void OnRegisterAdvertisementError(
      GattStatusCallback callback,
      int32_t adv_handle,
      device::BluetoothAdvertisement::ErrorCode error_code);
  void OnUnregisterAdvertisementDone(GattStatusCallback callback,
                                     int32_t adv_handle);
  void OnUnregisterAdvertisementError(
      GattStatusCallback callback,
      int32_t adv_handle,
      device::BluetoothAdvertisement::ErrorCode error_code);
  // Both of the following are called after we've tried to release |adv_handle|.
  // Either way, we will no longer be broadcasting this advertisement, so in
  // either case, the handle can be released.
  void OnReleaseAdvertisementHandleDone(GattStatusCallback callback,
                                        int32_t adv_handle);
  void OnReleaseAdvertisementHandleError(
      GattStatusCallback callback,
      int32_t adv_handle,
      device::BluetoothAdvertisement::ErrorCode error_code);
  // Find the next free advertisement handle and put it in *adv_handle,
  // or return false if the advertisement map is full.
  bool GetAdvertisementHandle(int32_t* adv_handle);

  void OnGattServerPrepareWrite(mojom::BluetoothAddressPtr addr,
                                bool has_subsequent_write,
                                base::OnceClosure success_callback,
                                ErrorCallback error_callback,
                                mojom::BluetoothGattStatus status);

  void SendDevice(const device::BluetoothDevice* device) const;

  // Detects the pairing state change from DeviceChanged(). Two actions will be
  // taken in this function:
  // - Updates |devices_pairing_| to reflect the set of ongoing pairing requests
  //   initiated by ARC;
  // - Notifies Android of the pairing failure.
  // Note that notifying Android of the pairing success is handled in
  // DevicePairedChange() but not in this function.
  void TrackPairingState(const device::BluetoothDevice* device);

  // Data structures for Bluetooth listening/connecting sockets that live in
  // Chrome.
  struct BluetoothListeningSocket {
    mojom::BluetoothSocketType sock_type;
    mojo::Remote<mojom::BluetoothListenSocketClient> remote;
    base::ScopedFD file;
    std::unique_ptr<base::FileDescriptorWatcher::Controller> controller;
    BluetoothListeningSocket();
    ~BluetoothListeningSocket();
  };
  struct BluetoothConnectingSocket {
    mojom::BluetoothSocketType sock_type;
    mojo::Remote<mojom::BluetoothConnectSocketClient> remote;
    base::ScopedFD file;
    std::unique_ptr<base::FileDescriptorWatcher::Controller> controller;
    BluetoothConnectingSocket();
    ~BluetoothConnectingSocket();
  };

  // Creates a Bluetooth socket with options in |flags| and starts listening to
  // it with the requested |port| number. The actual port number must be filled
  // in and sent to the callback in this function implementation. Returns a
  // BluetoothListeningSocket that holds the socket.
  virtual void CreateBluetoothListenSocket(
      mojom::BluetoothSocketType type,
      mojom::BluetoothSocketFlagsPtr flags,
      int port,
      BluetoothSocketListenCallback callback) = 0;
  // Creates a Bluetooth socket with socket option |optval|, and then calls
  // connect() to (|addr|, |port|). This connect() call is non-blocking.
  // Returns a BluetoothConnectingSocket that holds the socket.
  virtual void CreateBluetoothConnectSocket(
      mojom::BluetoothSocketType type,
      mojom::BluetoothSocketFlagsPtr flags,
      mojom::BluetoothAddressPtr addr,
      int port,
      BluetoothSocketConnectCallback callback) = 0;

  // Called when the listening socket is ready to accept().
  void OnBluetoothListeningSocketReady(
      ArcBluetoothBridge::BluetoothListeningSocket* socket);
  // Called when the connecting socket is ready.
  void OnBluetoothConnectingSocketReady(
      ArcBluetoothBridge::BluetoothConnectingSocket* socket);

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  scoped_refptr<device::BluetoothAdvertisement> advertisment_;
  // Discovery session created by StartDiscovery().
  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;
  // Discovery session created by StartLEScan().
  std::unique_ptr<device::BluetoothDiscoverySession> le_scan_session_;
  // Discovered devices in the current discovery session started by
  // StartDiscovery().
  std::set<std::string> discovered_devices_;
  // Scanned devices in the current scan session started by StartLEScan().
  std::set<std::string> scanned_devices_;
  std::unordered_map<std::string,
                     std::unique_ptr<device::BluetoothGattNotifySession>>
      notification_session_;
  // Map from Android int handle to Chrome (BlueZ) string identifier.
  std::unordered_map<int32_t, std::string> gatt_identifier_;
  // Map from Chrome (BlueZ) string identifier to android int handle.
  std::unordered_map<std::string, int32_t> gatt_handle_;
  // Store last GattCharacteristic added to each GattService for GattServer.
  std::unordered_map<int32_t, int32_t> last_characteristic_;
  // Monotonically increasing value to use as handle to give to Android side.
  int32_t gatt_server_attribute_next_handle_ = 0;

  // The devices that ARC has tried to pair by CreateBond() (including the
  // devices that the pair request is ongoing) but there is no
  // BluetoothGattConnection object associated to, during the lifetime of
  // Android BT service. The main reason we need to maintain this set is that we
  // need to manually close the links created by pair but not used by any GATT
  // connection when Android BT service goes down, otherwise these links will be
  // lingering.
  std::set<std::string> devices_paired_by_arc_;

  // The devices that ARC has tried to pair by CreateBond() and is in pairing
  // state. This should be a subset of |devices_paired_by_arc_|. We maintain
  // this set to detect the pairing failure signal. Please refer to
  // TrackPairingState() for more details.
  std::set<std::string> devices_pairing_;

  // {state, connection}
  // - For established connection from remote device, this is {CONNECTED, null}.
  // - For ongoing connection to remote device, this is {CONNECTING, null}.
  // - For established connection to remote device, this is {CONNECTED, ptr}.
  struct GattConnection {
    enum class ConnectionState { CONNECTING, CONNECTED } state;
    std::unique_ptr<device::BluetoothGattConnection> connection;
    // Indicates whether we should call BluetoothDevice::Disconnect() when
    // |connection| is closed. This field is true when the connection is
    // initiated by ARC, i.e, when ConnectLEDevice is called, either 1) the link
    // is down or 2) the remote device is paired by ARC firstly.
    // TODO(b/151573141): Remove this field when Chrome can perform hard
    // disconnect on a paired device correctly.
    bool need_hard_disconnect;
    GattConnection(ConnectionState state,
                   std::unique_ptr<device::BluetoothGattConnection> connection,
                   bool need_hard_disconnect);
    GattConnection();
    GattConnection(const GattConnection&) = delete;
    GattConnection& operator=(const GattConnection&) = delete;
    ~GattConnection();
    GattConnection(GattConnection&&);
    GattConnection& operator=(GattConnection&&);

   private:
  };
  std::unordered_map<std::string, GattConnection> gatt_connections_;

  // Timer to turn off discovery_session_.
  base::OneShotTimer discovery_off_timer_;
  // Timer to turn off le_scan_session_.
  // TODO(b/152463320): Remove this timer after the platform side supports
  // setting scan parameters and filters.
  base::OneShotTimer le_scan_off_timer_;
  // Timer to turn adapter discoverable off.
  base::OneShotTimer discoverable_off_timer_;
  // Adapter discoverable timeout value.
  std::optional<uint32_t> discoverable_off_timeout_ = std::nullopt;

  // Queue to track the powered state changes initiated by Android.
  base::queue<AdapterPowerState> remote_power_changes_;
  // Queue to track the powered state changes initiated by Chrome.
  base::queue<AdapterPowerState> local_power_changes_;
  // Timer to track the completion of power-changed intent sent to Android.
  base::OneShotTimer power_intent_timer_;

  // Holds advertising data registered by the instance.
  //
  // When a handle is reserved, an entry is placed into the advertisements_
  // map. This entry is not yet associated with a device::BluetoothAdvertisement
  // because the instance hasn't sent us any advertising data yet, so its
  // mapped value is nullptr until that happens. Thus we have three states for a
  // handle:
  // * unmapped -> free
  // * mapped to nullptr -> reserved, awaiting data
  // * mapped to a device::BluetoothAdvertisement -> in use, and the mapped
  //   BluetoothAdvertisement is currently registered with the adapter.
  // TODO(crbug.com/41282389) Change back to 5 when we support setting signal
  // strength per each advertisement slot.
  enum { kMaxAdvertisements = 1 };
  std::map<int32_t, scoped_refptr<device::BluetoothAdvertisement>>
      advertisements_;
  ArcBluetoothTaskQueue advertisement_queue_;
  // This queue will hold requests from both Start/CancelDiscovery() and
  // Start/StopLEScan().
  ArcBluetoothTaskQueue discovery_queue_;

  // Observes the ARC connection to Bluetooth service in Android. We need to do
  // some cleanup when it is down.
  class BluetoothArcConnectionObserver
      : public ConnectionObserver<mojom::BluetoothInstance> {
   public:
    explicit BluetoothArcConnectionObserver(
        ArcBluetoothBridge* arc_bluetooth_bridge);
    BluetoothArcConnectionObserver(const BluetoothArcConnectionObserver&) =
        delete;
    BluetoothArcConnectionObserver& operator=(
        const BluetoothArcConnectionObserver&) = delete;
    ~BluetoothArcConnectionObserver() override;
    // ConnectionObserver<mojom::BluetoothInstance> override.
    void OnConnectionClosed() override;

   private:
    raw_ptr<ArcBluetoothBridge> arc_bluetooth_bridge_;
  };
  BluetoothArcConnectionObserver bluetooth_arc_connection_observer_;

  THREAD_CHECKER(thread_checker_);

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcBluetoothBridge> weak_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_BLUETOOTH_BRIDGE_H_

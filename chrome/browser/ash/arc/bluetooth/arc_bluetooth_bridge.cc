// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/bluetooth/arc_bluetooth_bridge.h"

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/rfcomm.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/socket.h>

#include <iomanip>
#include <optional>
#include <string>
#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/bluetooth/bluetooth_type_converters.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/posix/eintr_wrapper.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/bluetooth/arc_bluez_bridge.h"
#include "chrome/browser/ash/arc/bluetooth/arc_floss_bridge.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/bluetooth/bluetooth_pairing_dialog.h"
#include "components/arc/common/intent_helper/arc_intent_helper_package.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_characteristic_bluez.h"
#include "device/bluetooth/bluez/bluetooth_remote_gatt_characteristic_bluez.h"
#include "device/bluetooth/floss/bluetooth_gatt_characteristic_floss.h"
#include "device/bluetooth/floss/floss_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothAdvertisement;
using device::BluetoothDevice;
using device::BluetoothDiscoveryFilter;
using device::BluetoothDiscoverySession;
using device::BluetoothGattCharacteristic;
using device::BluetoothGattConnection;
using device::BluetoothGattDescriptor;
using device::BluetoothGattNotifySession;
using device::BluetoothGattService;
using device::BluetoothLocalGattCharacteristic;
using device::BluetoothLocalGattDescriptor;
using device::BluetoothLocalGattService;
using device::BluetoothRemoteGattCharacteristic;
using device::BluetoothRemoteGattDescriptor;
using device::BluetoothRemoteGattService;
using device::BluetoothTransport;
using device::BluetoothUUID;

namespace {
// Bluetooth Spec Vol 3, Part G, 3.3.3.3 Client Characteristic Configuration.
constexpr uint8_t DISABLE_NOTIFICATION_VALUE = 0;
constexpr uint8_t ENABLE_NOTIFICATION_VALUE = 1;
constexpr uint8_t ENABLE_INDICATION_VALUE = 2;
constexpr int32_t kInvalidGattAttributeHandle = -1;
constexpr int32_t kInvalidAdvertisementHandle = -1;
// Bluetooth Specification Version 4.2 Vol 3 Part F Section 3.2.2
// An attribute handle of value 0xFFFF is known as the maximum attribute handle.
constexpr int32_t kMaxGattAttributeHandle = 0xFFFF;
// Bluetooth Specification Version 4.2 Vol 3 Part F Section 3.2.9
// The maximum length of an attribute value shall be 512 octets.
constexpr int kMaxGattAttributeLength = 512;
// Copied from Android at system/bt/stack/btm/btm_ble_int.h
// https://android.googlesource.com/platform/system/bt/+/android-n-preview-5/stack/btm/btm_ble_int.h?pli=1#109
constexpr uint16_t kAndroidMBluetoothVersionNumber = 95;
// Timeout for Bluetooth Discovery (scan)
// 120 seconds is used here as the upper bound of the time need to do device
// discovery once, 20 seconds for inquiry scan and 100 seconds for page scan
// for 100 new devices.
constexpr base::TimeDelta kDiscoveryTimeout = base::Seconds(120);
// From https://www.bluetooth.com/specifications/assigned-numbers/baseband
// The Class of Device for generic computer.
constexpr uint32_t kBluetoothComputerClass = 0x100;
// Timeout for Android to complete a disabling op to adapter.
// In the case where an enabling op happens immediately after a disabling op,
// Android takes the following enabling op as a no-op and waits 3~4 seconds for
// the previous disabling op to finish, so the enabling op will never be
// fulfilled by Android, and the disabling op will later routed back to Chrome
// while Chrome's adapter is enabled. This results in the wrong power state
// which should be enabled. Since the signaling from Android to Chrome for
// Bluetooth is via Bluetooth HAL layer which run on the same process as
// Bluetooth Service in Java space, so the signaling to Chrome about the
// to-be-happen sleep cannot be done. This timeout tries to ensure the validity
// and the order of toggles on power state sent to Android.
// If Android takes more than 8 seconds to complete the intent initiated by
// Chrome, Chrome will take EnableAdapter/DisableAdapter calls as a request from
// Android to toggle the power state. The power state will be synced on both
// Chrome and Android, but as a result, Bluetooth will be off.
constexpr base::TimeDelta kPowerIntentTimeout = base::Seconds(8);

// Client name for logging in BLE scanning.
constexpr char kScanClientName[] = "ARC";

arc::mojom::BluetoothGattStatus ConvertGattErrorCodeToStatus(
    const device::BluetoothGattService::GattErrorCode& error_code,
    bool is_read_operation) {
  switch (error_code) {
    case device::BluetoothGattService::GattErrorCode::kInvalidLength:
      return arc::mojom::BluetoothGattStatus::GATT_INVALID_ATTRIBUTE_LENGTH;
    case device::BluetoothGattService::GattErrorCode::kNotPermitted:
      return is_read_operation
                 ? arc::mojom::BluetoothGattStatus::GATT_READ_NOT_PERMITTED
                 : arc::mojom::BluetoothGattStatus::GATT_WRITE_NOT_PERMITTED;
    case device::BluetoothGattService::GattErrorCode::kNotAuthorized:
      return arc::mojom::BluetoothGattStatus::GATT_INSUFFICIENT_AUTHENTICATION;
    case device::BluetoothGattService::GattErrorCode::kNotSupported:
      return arc::mojom::BluetoothGattStatus::GATT_REQUEST_NOT_SUPPORTED;
    case device::BluetoothGattService::GattErrorCode::kUnknown:
    case device::BluetoothGattService::GattErrorCode::kFailed:
    case device::BluetoothGattService::GattErrorCode::kInProgress:
    case device::BluetoothGattService::GattErrorCode::kNotPaired:
    default:
      return arc::mojom::BluetoothGattStatus::GATT_FAILURE;
  }
}

// Example of identifier: /org/bluez/hci0/dev_E0_CF_65_8C_86_1A/service001a
// Convert the last 4 characters of |identifier| to an
// int, by interpreting them as hexadecimal digits.
std::optional<uint16_t> ConvertGattIdentifierToId(
    const std::string identifier) {
  uint32_t result;
  if (identifier.size() < 4 ||
      !base::HexStringToUInt(identifier.substr(identifier.size() - 4), &result))
    return std::nullopt;
  return result;
}

// Create GattDBElement and fill in common data for
// Gatt Service/Characteristic/Descriptor.
template <class RemoteGattAttribute>
arc::mojom::BluetoothGattDBElementPtr CreateGattDBElement(
    const arc::mojom::BluetoothGattDBAttributeType type,
    const RemoteGattAttribute* attribute) {
  std::optional<uint16_t> id =
      ConvertGattIdentifierToId(attribute->GetIdentifier());
  if (!id)
    return nullptr;

  arc::mojom::BluetoothGattDBElementPtr element =
      arc::mojom::BluetoothGattDBElement::New();
  element->type = type;
  element->uuid = attribute->GetUUID();
  element->element_id = element->attribute_handle = element->start_handle =
      element->end_handle = *id;
  // TODO(b/191129417) remove once ARC++ handles new field
  element->deprecated_id = *id;
  element->properties = 0;
  return element;
}

template <class RemoteGattAttribute>
RemoteGattAttribute* FindGattAttributeByUuid(
    const std::vector<RemoteGattAttribute*>& attributes,
    const BluetoothUUID& uuid) {
  auto it = base::ranges::find(attributes, uuid, &RemoteGattAttribute::GetUUID);
  return it != attributes.end() ? *it : nullptr;
}

// Common success callback for GATT operations that only need to report
// GattStatus back to Android.
void OnGattOperationDone(arc::ArcBluetoothBridge::GattStatusCallback callback) {
  std::move(callback).Run(arc::mojom::BluetoothGattStatus::GATT_SUCCESS);
}

// Common error callback for GATT operations that only need to report
// GattStatus back to Android.
void OnGattOperationError(arc::ArcBluetoothBridge::GattStatusCallback callback,
                          BluetoothGattService::GattErrorCode error_code) {
  std::move(callback).Run(ConvertGattErrorCodeToStatus(
      error_code, /* is_read_operation = */ false));
}

// Common callback (success and error) for ReadGattCharacteristic and
// ReadGattDescriptor.
void OnGattRead(
    arc::ArcBluetoothBridge::GattReadCallback callback,
    std::optional<device::BluetoothGattService::GattErrorCode> error_code,
    const std::vector<uint8_t>& result) {
  arc::mojom::BluetoothGattValuePtr gattValue =
      arc::mojom::BluetoothGattValue::New();

  if (error_code.has_value()) {
    gattValue->status = ConvertGattErrorCodeToStatus(
        error_code.value(), /*is_read_operation=*/true);
  } else {
    gattValue->status = arc::mojom::BluetoothGattStatus::GATT_SUCCESS;
  }
  gattValue->value = result;
  std::move(callback).Run(std::move(gattValue));
}

// Callback function for mojom::BluetoothInstance::RequestGattRead
void OnGattServerRead(
    BluetoothLocalGattService::Delegate::ValueCallback callback,
    arc::mojom::BluetoothGattStatus status,
    const std::vector<uint8_t>& value) {
  if (status == arc::mojom::BluetoothGattStatus::GATT_SUCCESS) {
    std::move(callback).Run(/*error_code=*/std::nullopt, value);
  } else {
    std::move(callback).Run(BluetoothGattService::GattErrorCode::kFailed,
                            /*value=*/std::vector<uint8_t>());
  }
}

// Callback function for mojom::BluetoothInstance::RequestGattWrite
void OnGattServerWrite(
    base::OnceClosure success_callback,
    BluetoothLocalGattService::Delegate::ErrorCallback error_callback,
    arc::mojom::BluetoothGattStatus status) {
  if (status == arc::mojom::BluetoothGattStatus::GATT_SUCCESS)
    std::move(success_callback).Run();
  else
    std::move(error_callback).Run();
}

bool IsGattOffsetValid(int offset) {
  return 0 <= offset && offset < kMaxGattAttributeLength;
}

// This is needed because Android only support UUID 16 bits in service data
// section in advertising data
std::optional<uint16_t> GetUUID16(const BluetoothUUID& uuid) {
  // Convert xxxxyyyy-xxxx-xxxx-xxxx-xxxxxxxxxxxx to int16 yyyy
  uint32_t result;
  if (uuid.canonical_value().size() < 8 ||
      !base::HexStringToUInt(uuid.canonical_value().substr(4, 4), &result))
    return std::nullopt;
  return result;
}

arc::mojom::BluetoothPropertyPtr GetDiscoveryTimeoutProperty(uint32_t timeout) {
  return arc::mojom::BluetoothProperty::NewDiscoveryTimeout(timeout);
}

const device::BluetoothLocalGattDescriptor* FindCCCD(
    const device::BluetoothLocalGattCharacteristic* characteristic) {
  for (auto descriptor : characteristic->GetDescriptors()) {
    if (descriptor->GetUUID() ==
        BluetoothGattDescriptor::ClientCharacteristicConfigurationUuid()) {
      return descriptor;
    }
  }
  return nullptr;
}

std::vector<uint8_t> MakeCCCDValue(uint8_t value) {
  return {value, 0};
}

void SendRssiOnGetConnectionInfoDone(
    arc::ArcBluetoothBridge::ReadRemoteRssiCallback callback,
    const device::BluetoothDevice::ConnectionInfo& conn_info) {
  std::move(callback).Run(conn_info.rssi);
}

}  // namespace

namespace arc {
namespace {

// Singleton factory for ArcAccessibilityHelperBridge.
class ArcBluezBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcBluezBridge,
          ArcBluezBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcBluezBridgeFactory";

  static ArcBluezBridgeFactory* GetInstance() {
    return base::Singleton<ArcBluezBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcBluezBridgeFactory>;
  ArcBluezBridgeFactory() = default;
  ~ArcBluezBridgeFactory() override = default;
};

// Singleton factory for ArcAccessibilityHelperBridge.
class ArcFlossBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcFlossBridge,
          ArcFlossBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcFlossBridgeFactory";

  static ArcFlossBridgeFactory* GetInstance() {
    return base::Singleton<ArcFlossBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcFlossBridgeFactory>;
  ArcFlossBridgeFactory() = default;
  ~ArcFlossBridgeFactory() override = default;
};

}  // namespace

// static
ArcBluetoothBridge* ArcBluetoothBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  if (floss::features::IsFlossEnabled()) {
    return ArcFlossBridgeFactory::GetForBrowserContext(context);
  } else {
    return ArcBluezBridgeFactory::GetForBrowserContext(context);
  }
}

ArcBluetoothBridge::ArcBluetoothBridge(content::BrowserContext* context,
                                       ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      bluetooth_arc_connection_observer_(this) {
  arc_bridge_service_->app()->AddObserver(this);
  arc_bridge_service_->intent_helper()->AddObserver(this);

  if (BluetoothAdapterFactory::IsBluetoothSupported()) {
    VLOG(1) << "Registering bluetooth adapter.";
    BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
        &ArcBluetoothBridge::OnAdapterInitialized, weak_factory_.GetWeakPtr()));
  } else {
    VLOG(1) << "Bluetooth not supported.";
  }
}

ArcBluetoothBridge::~ArcBluetoothBridge() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);

  arc_bridge_service_->app()->RemoveObserver(this);
  arc_bridge_service_->intent_helper()->RemoveObserver(this);
  arc_bridge_service_->bluetooth()->SetHost(nullptr);
}

void ArcBluetoothBridge::OnAdapterInitialized(
    scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK(adapter);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  bluetooth_adapter_ = adapter;

  if (!bluetooth_adapter_->HasObserver(this))
    bluetooth_adapter_->AddObserver(this);

  // Once the bluetooth adapter is ready, we can now signal the container that
  // the interface is ready to be interacted with. This avoids races in most
  // methods, since it's undesirable to implement a retry mechanism for the
  // cases when an inbound method is called and the adapter is not ready yet.
  arc_bridge_service_->bluetooth()->SetHost(this);
}

void ArcBluetoothBridge::AdapterPoweredChanged(BluetoothAdapter* adapter,
                                               bool powered) {
  AdapterPowerState power_change =
      powered ? AdapterPowerState::TURN_ON : AdapterPowerState::TURN_OFF;
  if (IsPowerChangeInitiatedByRemote(power_change))
    DequeueRemotePowerChange(power_change);
  else
    EnqueueLocalPowerChange(power_change);
}

void ArcBluetoothBridge::DeviceAdded(BluetoothAdapter* adapter,
                                     BluetoothDevice* device) {
  DeviceChanged(adapter, device);

  // We need to trigger this manually if the device is connected when it is
  // added. This may happen for a incoming connection from an unknown device.
  if (device->IsConnected())
    DeviceConnectedStateChanged(adapter, device, /*is_now_connected=*/true);
}

void ArcBluetoothBridge::DeviceChanged(BluetoothAdapter* adapter,
                                       BluetoothDevice* device) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;

  std::string addr = device->GetAddress();
  if (IsDiscoveringOrScanning() && discovered_devices_.insert(addr).second) {
    auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_bridge_service_->bluetooth(), OnDeviceFound);
    if (bluetooth_instance) {
      bluetooth_instance->OnDeviceFound(
          GetDeviceProperties(mojom::BluetoothPropertyType::ALL, device));
    }
  } else if (discovered_devices_.contains(addr)) {
    auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_bridge_service_->bluetooth(), OnDevicePropertiesChanged);
    if (bluetooth_instance) {
      bluetooth_instance->OnDevicePropertiesChanged(
          mojom::BluetoothAddress::From(addr),
          GetDeviceProperties(mojom::BluetoothPropertyType::ALL, device));
    }
  }

  TrackPairingState(device);
}

void ArcBluetoothBridge::TrackPairingState(const BluetoothDevice* device) {
  const std::string addr = device->GetAddress();

  // A device in pairing is in |devices_paired_by_arc_| from CreateBond() is
  // called until at least pairing is finished (either succeed or fail), so we
  // don't need to do anything here if the device is not in the list.
  if (devices_paired_by_arc_.find(addr) == devices_paired_by_arc_.end())
    return;

  const auto itr = devices_pairing_.find(addr);
  bool was_pairing = itr != devices_pairing_.end();

  // The actions we need to take depends on the combination of |was_pairing|,
  // IsConnecting() and IsPaired():
  // If not |was_pairing|:
  // - !IsConnecting() means device is not pairing, do nothing;
  // - IsPaired() means device has already been paired, do nothing;
  // - IsConnecting() && !IsPaired() means device is pairing now, we should add
  //   it into our list.
  // If |was_pairing|:
  // - IsPaired() means pairing succeeded, we should remove the device from our
  //   list.
  // - IsConnecting() && !IsPaired() means device is still in pairing, do
  //   nothing;
  // - !IsConnecting() && !IsPaired() means pairing failed, we should notify
  //   Android, and remove the device from our list;
  if (!was_pairing) {
    if (device->IsConnecting() && !device->IsPaired())
      devices_pairing_.insert(addr);
    return;
  }

  if (device->IsPaired()) {
    devices_pairing_.erase(itr);
  } else if (!device->IsConnecting()) {
    LOG(WARNING) << "Pairing failed for device " << addr;
    OnPairedError(mojom::BluetoothAddress::From(addr),
                  BluetoothDevice::ERROR_FAILED);
    devices_pairing_.erase(itr);
  }
}

void ArcBluetoothBridge::DeviceAddressChanged(BluetoothAdapter* adapter,
                                              BluetoothDevice* device,
                                              const std::string& old_address) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;

  std::string new_address = device->GetAddress();
  if (old_address == new_address)
    return;

  if (!(device->GetType() & device::BLUETOOTH_TRANSPORT_LE))
    return;

  if (devices_paired_by_arc_.erase(old_address) == 1)
    devices_paired_by_arc_.insert(new_address);

  auto it = gatt_connections_.find(old_address);
  if (it == gatt_connections_.end())
    return;

  gatt_connections_.emplace(new_address, std::move(it->second));
  gatt_connections_.erase(it);

  auto* btle_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnLEDeviceAddressChange);
  if (!btle_instance)
    return;

  btle_instance->OnLEDeviceAddressChange(
      mojom::BluetoothAddress::From(old_address),
      mojom::BluetoothAddress::From(new_address));
}

void ArcBluetoothBridge::DevicePairedChanged(BluetoothAdapter* adapter,
                                             BluetoothDevice* device,
                                             bool new_paired_status) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;

  DCHECK(adapter);
  DCHECK(device);

  mojom::BluetoothAddressPtr addr =
      mojom::BluetoothAddress::From(device->GetAddress());

  if (new_paired_status) {
    // OnBondStateChanged must be called with BluetoothBondState::BONDING to
    // make sure the bond state machine on Android is ready to take the
    // pair-done event. Otherwise the pair-done event will be dropped as an
    // invalid change of paired status.
    OnPairing(addr->Clone());
    OnPairedDone(std::move(addr));
  } else {
    OnForgetDone(std::move(addr));
  }
}

void ArcBluetoothBridge::DeviceMTUChanged(BluetoothAdapter* adapter,
                                          BluetoothDevice* device,
                                          uint16_t mtu) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;

  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnMTUReceived);
  if (!device->IsConnected() || bluetooth_instance == nullptr)
    return;
  bluetooth_instance->OnMTUReceived(
      mojom::BluetoothAddress::From(device->GetAddress()), mtu);
}

void ArcBluetoothBridge::DeviceAdvertisementReceived(
    BluetoothAdapter* adapter,
    BluetoothDevice* device,
    int16_t rssi,
    const std::vector<uint8_t>& eir) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;

  mojom::BluetoothAddressPtr addr =
      mojom::BluetoothAddress::From(device->GetAddress());

  if ((IsDiscoveringOrScanning() &&
       scanned_devices_.insert(device->GetAddress()).second) ||
      scanned_devices_.contains(device->GetAddress())) {
    auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_bridge_service_->bluetooth(), OnLEDeviceFound);
    if (bluetooth_instance) {
      bluetooth_instance->OnLEDeviceFound(std::move(addr), rssi, eir);
    }
  }
}

void ArcBluetoothBridge::DeviceConnectedStateChanged(BluetoothAdapter* adapter,
                                                     BluetoothDevice* device,
                                                     bool is_now_connected) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;

  const std::string addr = device->GetAddress();

  // If this event is about 1) an device supports LE becomes disconnected and 2)
  // we are holding the connection object for this device, we need to remove
  // this object and notify Android.
  bool support_le = device->GetType() & device::BLUETOOTH_TRANSPORT_LE;
  auto it = gatt_connections_.find(addr);
  if (support_le && it != gatt_connections_.end() && !is_now_connected)
    OnGattDisconnected(mojom::BluetoothAddress::From(addr));

  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnConnectionStateChanged);
  if (!bluetooth_instance)
    return;

  bluetooth_instance->OnConnectionStateChanged(
      mojom::BluetoothAddress::From(addr), device->GetType(), is_now_connected);
}

void ArcBluetoothBridge::DeviceRemoved(BluetoothAdapter* adapter,
                                       BluetoothDevice* device) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;

  DCHECK(adapter);
  DCHECK(device);

  std::string address = device->GetAddress();
  if (gatt_connections_.find(address) != gatt_connections_.end())
    OnGattDisconnected(mojom::BluetoothAddress::From(address));
  OnForgetDone(mojom::BluetoothAddress::From(address));
}

void ArcBluetoothBridge::OnGetServiceRecordsFinished(
    mojom::BluetoothAddressPtr remote_addr,
    const BluetoothUUID& target_uuid,
    const std::vector<bluez::BluetoothServiceRecordBlueZ>& records_bluez) {
  // TODO(b/288866953): ARCVM crashes if records are empty
  if (records_bluez.size() == 0) {
    return;
  }

  auto* sdp_bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnGetSdpRecords);
  if (!sdp_bluetooth_instance) {
    LOG(ERROR) << "Could not get bluetooth instance to return SDP records";
    return;
  }

  std::vector<mojom::BluetoothSdpRecordPtr> records;
  for (const auto& r : records_bluez) {
    records.push_back(mojom::BluetoothSdpRecord::From(r));
  }

  sdp_bluetooth_instance->OnGetSdpRecords(mojom::BluetoothStatus::SUCCESS,
                                          std::move(remote_addr), target_uuid,
                                          std::move(records));
}

void ArcBluetoothBridge::OnGetServiceRecordsError(
    mojom::BluetoothAddressPtr remote_addr,
    const BluetoothUUID& target_uuid,
    bluez::BluetoothServiceRecordBlueZ::ErrorCode error_code) {
  auto* sdp_bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnGetSdpRecords);
  if (!sdp_bluetooth_instance) {
    LOG(ERROR) << "Could not get bluetooth instance to return SDP error";
    return;
  }

  mojom::BluetoothStatus status;

  switch (error_code) {
    case bluez::BluetoothServiceRecordBlueZ::ErrorCode::ERROR_ADAPTER_NOT_READY:
      status = mojom::BluetoothStatus::NOT_READY;
      break;
    case bluez::BluetoothServiceRecordBlueZ::ErrorCode::
        ERROR_DEVICE_DISCONNECTED:
      status = mojom::BluetoothStatus::RMT_DEV_DOWN;
      break;
    default:
      status = mojom::BluetoothStatus::FAIL;
      break;
  }

  sdp_bluetooth_instance->OnGetSdpRecords(
      status, std::move(remote_addr), target_uuid,
      std::vector<mojom::BluetoothSdpRecordPtr>());
}

void ArcBluetoothBridge::GattServiceAdded(BluetoothAdapter* adapter,
                                          BluetoothDevice* device,
                                          BluetoothRemoteGattService* service) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;
  // Placeholder for GATT client functionality
  GattServiceChanged(adapter, service);
}

void ArcBluetoothBridge::GattServiceRemoved(
    BluetoothAdapter* adapter,
    BluetoothDevice* device,
    BluetoothRemoteGattService* service) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;
  // Placeholder for GATT client functionality
  GattServiceChanged(adapter, service);
}

void ArcBluetoothBridge::GattServicesDiscovered(BluetoothAdapter* adapter,
                                                BluetoothDevice* device) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;

  auto* btle_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnSearchComplete);
  if (!btle_instance)
    return;

  mojom::BluetoothAddressPtr addr =
      mojom::BluetoothAddress::From(device->GetAddress());

  btle_instance->OnSearchComplete(std::move(addr),
                                  mojom::BluetoothGattStatus::GATT_SUCCESS);
}

void ArcBluetoothBridge::GattDiscoveryCompleteForService(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattService* service) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattNeedsDiscovery(BluetoothDevice* device) {
  if (!arc_bridge_service_->bluetooth()->IsConnected()) {
    return;
  }

  // This is a bit of a misnomer from ARC side: OnServiceChanged needs to be
  // called when we get the signal that something is changed on the peer side,
  // so ARC can start to re-discover everything again.
  // However, the GattServiceChanged below indicates we have updated a service,
  // so it doesn't actually mean ARC needs to re-discover everything.
  auto* btle_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnServiceChanged);
  if (!btle_instance) {
    return;
  }
  btle_instance->OnServiceChanged(
      mojom::BluetoothAddress::From(device->GetAddress()));
}

// TODO(b/284429795) This is wrong. See GattNeedsDiscovery above.
void ArcBluetoothBridge::GattServiceChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattService* service) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;
  BluetoothDevice* device = service->GetDevice();
  if (!device)
    return;

  auto* btle_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnServiceChanged);
  if (!btle_instance)
    return;
  btle_instance->OnServiceChanged(
      mojom::BluetoothAddress::From(device->GetAddress()));
}

void ArcBluetoothBridge::GattCharacteristicAdded(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattCharacteristicRemoved(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattDescriptorAdded(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattDescriptor* descriptor) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattDescriptorRemoved(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattDescriptor* descriptor) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;
  // Placeholder for GATT client functionality
}

void ArcBluetoothBridge::GattCharacteristicValueChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;

  auto* btle_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnGattNotify);
  if (!btle_instance)
    return;

  const std::optional<uint16_t> char_inst_id =
      ConvertGattIdentifierToId(characteristic->GetIdentifier());
  if (!char_inst_id)
    return;

  BluetoothRemoteGattService* service = characteristic->GetService();
  const std::optional<uint16_t> service_inst_id =
      ConvertGattIdentifierToId(service->GetIdentifier());
  if (!service_inst_id)
    return;

  BluetoothDevice* device = service->GetDevice();
  mojom::BluetoothAddressPtr address =
      mojom::BluetoothAddress::From(device->GetAddress());
  mojom::BluetoothGattServiceIDPtr service_id =
      mojom::BluetoothGattServiceID::New();
  service_id->is_primary = service->IsPrimary();
  service_id->id = mojom::BluetoothGattID::New();
  service_id->id->instance_id = *service_inst_id;
  // TODO(b/191129417) remove once ARC++ handles new field
  service_id->id->deprecated_inst_id = *service_inst_id;
  service_id->id->uuid = service->GetUUID();

  mojom::BluetoothGattIDPtr char_id = mojom::BluetoothGattID::New();
  char_id->instance_id = *char_inst_id;
  // TODO(b/191129417) remove once ARC++ handles new field
  char_id->deprecated_inst_id = *char_inst_id;
  char_id->uuid = characteristic->GetUUID();

  btle_instance->OnGattNotify(std::move(address), std::move(service_id),
                              std::move(char_id), true /* is_notify */, value);
}

void ArcBluetoothBridge::GattDescriptorValueChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattDescriptor* descriptor,
    const std::vector<uint8_t>& value) {
  if (!arc_bridge_service_->bluetooth()->IsConnected())
    return;
  // Placeholder for GATT client functionality
}

template <class LocalGattAttribute>
void ArcBluetoothBridge::OnGattAttributeReadRequest(
    const BluetoothDevice* device,
    const LocalGattAttribute* attribute,
    int offset,
    mojom::BluetoothGattDBAttributeType attribute_type,
    ValueCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), RequestGattRead);
  if (!bluetooth_instance || !IsGattOffsetValid(offset)) {
    std::move(callback).Run(BluetoothGattService::GattErrorCode::kFailed,
                            /*value=*/std::vector<uint8_t>());
    return;
  }

  DCHECK(gatt_handle_.find(attribute->GetIdentifier()) != gatt_handle_.end());

  bluetooth_instance->RequestGattRead(
      mojom::BluetoothAddress::From(device->GetAddress()),
      gatt_handle_[attribute->GetIdentifier()], offset, false /* is_long */,
      attribute_type, base::BindOnce(&OnGattServerRead, std::move(callback)));
}

void ArcBluetoothBridge::OnGattServerPrepareWrite(
    mojom::BluetoothAddressPtr addr,
    bool has_subsequent_write,
    base::OnceClosure success_callback,
    ErrorCallback error_callback,
    mojom::BluetoothGattStatus status) {
  bool success = (status == mojom::BluetoothGattStatus::GATT_SUCCESS);
  if (success && has_subsequent_write) {
    std::move(success_callback).Run();
    return;
  }

  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), RequestGattExecuteWrite);
  if (bluetooth_instance == nullptr) {
    std::move(error_callback).Run();
    return;
  }

  if (!success) {
    auto split_callback = base::SplitOnceCallback(std::move(error_callback));
    success_callback = std::move(split_callback.first);
    error_callback = std::move(split_callback.second);
  }
  bluetooth_instance->RequestGattExecuteWrite(
      std::move(addr), success,
      base::BindOnce(&OnGattServerWrite, std::move(success_callback),
                     std::move(error_callback)));
}

template <class LocalGattAttribute>
void ArcBluetoothBridge::OnGattAttributeWriteRequest(
    const BluetoothDevice* device,
    const LocalGattAttribute* attribute,
    const std::vector<uint8_t>& value,
    int offset,
    mojom::BluetoothGattDBAttributeType attribute_type,
    bool is_prepare,
    bool has_subsequent_write,
    base::OnceClosure success_callback,
    ErrorCallback error_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), RequestGattWrite);
  if (!bluetooth_instance || !IsGattOffsetValid(offset)) {
    std::move(error_callback).Run();
    return;
  }

  GattStatusCallback callback =
      is_prepare
          ? base::BindOnce(&ArcBluetoothBridge::OnGattServerPrepareWrite,
                           weak_factory_.GetWeakPtr(),
                           mojom::BluetoothAddress::From(device->GetAddress()),
                           has_subsequent_write, std::move(success_callback),
                           std::move(error_callback))
          : base::BindOnce(&OnGattServerWrite, std::move(success_callback),
                           std::move(error_callback));
  DCHECK(gatt_handle_.find(attribute->GetIdentifier()) != gatt_handle_.end());
  bluetooth_instance->RequestGattWrite(
      mojom::BluetoothAddress::From(device->GetAddress()),
      gatt_handle_[attribute->GetIdentifier()], offset, value, attribute_type,
      is_prepare, std::move(callback));
}

void ArcBluetoothBridge::OnCharacteristicReadRequest(
    const BluetoothDevice* device,
    const BluetoothLocalGattCharacteristic* characteristic,
    int offset,
    ValueCallback callback) {
  OnGattAttributeReadRequest(
      device, characteristic, offset,
      mojom::BluetoothGattDBAttributeType::BTGATT_DB_CHARACTERISTIC,
      std::move(callback));
}

void ArcBluetoothBridge::OnCharacteristicWriteRequest(
    const BluetoothDevice* device,
    const BluetoothLocalGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value,
    int offset,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  OnGattAttributeWriteRequest(
      device, characteristic, value, offset,
      mojom::BluetoothGattDBAttributeType::BTGATT_DB_CHARACTERISTIC,
      /* is_prepare = */ false, /* has_subsequent_write, = */ false,
      std::move(callback), std::move(error_callback));
}

void ArcBluetoothBridge::OnCharacteristicPrepareWriteRequest(
    const BluetoothDevice* device,
    const BluetoothLocalGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value,
    int offset,
    bool has_subsequent_write,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  OnGattAttributeWriteRequest(
      device, characteristic, value, offset,
      mojom::BluetoothGattDBAttributeType::BTGATT_DB_CHARACTERISTIC,
      /* is_prepare = */ true, has_subsequent_write, std::move(callback),
      std::move(error_callback));
}

void ArcBluetoothBridge::OnDescriptorReadRequest(
    const BluetoothDevice* device,
    const BluetoothLocalGattDescriptor* descriptor,
    int offset,
    ValueCallback callback) {
  OnGattAttributeReadRequest(
      device, descriptor, offset,
      mojom::BluetoothGattDBAttributeType::BTGATT_DB_DESCRIPTOR,
      std::move(callback));
}

void ArcBluetoothBridge::OnDescriptorWriteRequest(
    const BluetoothDevice* device,
    const BluetoothLocalGattDescriptor* descriptor,
    const std::vector<uint8_t>& value,
    int offset,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  OnGattAttributeWriteRequest(
      device, descriptor, value, offset,
      mojom::BluetoothGattDBAttributeType::BTGATT_DB_DESCRIPTOR,
      /* is_prepare = */ false, /* has_subsequent_write = */ false,
      std::move(callback), std::move(error_callback));
}

void ArcBluetoothBridge::OnNotificationsStart(
    const BluetoothDevice* device,
    device::BluetoothGattCharacteristic::NotificationType notification_type,
    const BluetoothLocalGattCharacteristic* characteristic) {
  const BluetoothLocalGattDescriptor* cccd = FindCCCD(characteristic);
  if (cccd == nullptr)
    return;
  OnDescriptorWriteRequest(
      device, cccd,
      MakeCCCDValue(notification_type ==
                            device::BluetoothRemoteGattCharacteristic::
                                NotificationType::kNotification
                        ? ENABLE_NOTIFICATION_VALUE
                        : ENABLE_INDICATION_VALUE),
      0, base::DoNothing(), base::DoNothing());
}

void ArcBluetoothBridge::OnNotificationsStop(
    const BluetoothDevice* device,
    const BluetoothLocalGattCharacteristic* characteristic) {
  const BluetoothLocalGattDescriptor* cccd = FindCCCD(characteristic);
  if (cccd == nullptr)
    return;
  OnDescriptorWriteRequest(device, cccd,
                           MakeCCCDValue(DISABLE_NOTIFICATION_VALUE), 0,
                           base::DoNothing(), base::DoNothing());
}

void ArcBluetoothBridge::OnDeviceFound(
    device::BluetoothLowEnergyScanSession* scan_session,
    device::BluetoothDevice* device) {
  DeviceAdded(bluetooth_adapter_.get(), device);
}

void ArcBluetoothBridge::OnDeviceLost(
    device::BluetoothLowEnergyScanSession* scan_session,
    device::BluetoothDevice* device) {}

void ArcBluetoothBridge::OnSessionStarted(
    device::BluetoothLowEnergyScanSession* scan_session,
    std::optional<device::BluetoothLowEnergyScanSession::ErrorCode>
        error_code) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (error_code) {
    LOG(WARNING) << "failed to start LE scan, error_code = "
                 << static_cast<int>(error_code.value());
    ResetLEScanSession();
  } else {
    StartLEScanOffTimer();
  }

  discovery_queue_.Pop();
}

void ArcBluetoothBridge::OnSessionInvalidated(
    device::BluetoothLowEnergyScanSession* scan_session) {
  LOG(WARNING) << "LE scan session was invalidated";
  ResetLEScanSession();
}

void ArcBluetoothBridge::EnableAdapter(EnableAdapterCallback callback) {
  DCHECK(bluetooth_adapter_);
  if (IsPowerChangeInitiatedByLocal(AdapterPowerState::TURN_ON)) {
    BLUETOOTH_LOG(EVENT) << "Received a request to enable adapter (local)";
    DequeueLocalPowerChange(AdapterPowerState::TURN_ON);
  } else {
    BLUETOOTH_LOG(EVENT) << "Received a request to enable adapter (remote)";
    if (!bluetooth_adapter_->IsPowered()) {
      EnqueueRemotePowerChange(AdapterPowerState::TURN_ON, std::move(callback));
      return;
    }
  }

  OnPoweredOn(std::move(callback), false /* save_user_pref */);
}

void ArcBluetoothBridge::DisableAdapter(DisableAdapterCallback callback) {
  DCHECK(bluetooth_adapter_);
  if (IsPowerChangeInitiatedByLocal(AdapterPowerState::TURN_OFF)) {
    BLUETOOTH_LOG(EVENT) << "Received a request to disable adapter (local)";
    DequeueLocalPowerChange(AdapterPowerState::TURN_OFF);
  } else {
    BLUETOOTH_LOG(EVENT) << "Received a request to disable adapter (remote)";
    // Silently ignore any request to turn off Bluetooth from Android.
    // Android will still receive the success callback.
    // (https://crbug.com/851097)
  }

  OnPoweredOff(std::move(callback), false /* save_user_pref */);
}

void ArcBluetoothBridge::GetAdapterProperty(mojom::BluetoothPropertyType type) {
  DCHECK(bluetooth_adapter_);
  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnAdapterProperties);
  if (!bluetooth_instance)
    return;

  std::vector<mojom::BluetoothPropertyPtr> properties =
      GetAdapterProperties(type);

  bluetooth_instance->OnAdapterProperties(mojom::BluetoothStatus::SUCCESS,
                                          std::move(properties));
}

void ArcBluetoothBridge::OnSetDiscoverable(bool discoverable,
                                           bool success,
                                           uint32_t timeout) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (success && discoverable && timeout > 0) {
    discoverable_off_timer_.Start(
        FROM_HERE, base::Seconds(timeout),
        base::BindOnce(&ArcBluetoothBridge::SetDiscoverable,
                       weak_factory_.GetWeakPtr(), false, 0));
  }

  auto status =
      success ? mojom::BluetoothStatus::SUCCESS : mojom::BluetoothStatus::FAIL;
  OnSetAdapterProperty(status, GetDiscoveryTimeoutProperty(timeout));
}

// Set discoverable state to on / off.
// In case of turning on, start timer to turn it back off in |timeout| seconds.
void ArcBluetoothBridge::SetDiscoverable(bool discoverable, uint32_t timeout) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(bluetooth_adapter_);

  bool currently_discoverable = bluetooth_adapter_->IsDiscoverable();

  if (!discoverable && !currently_discoverable)
    return;

  if (discoverable && currently_discoverable) {
    if (base::Seconds(timeout) > discoverable_off_timer_.GetCurrentDelay()) {
      // Restart discoverable_off_timer_ if new timeout is greater
      OnSetDiscoverable(true, true, timeout);
    } else {
      // Just send message to Android if new timeout is lower.
      OnSetAdapterProperty(mojom::BluetoothStatus::SUCCESS,
                           GetDiscoveryTimeoutProperty(timeout));
    }
    return;
  }

  bluetooth_adapter_->SetDiscoverable(
      discoverable,
      base::BindOnce(&ArcBluetoothBridge::OnSetDiscoverable,
                     weak_factory_.GetWeakPtr(), discoverable, true, timeout),
      base::BindOnce(&ArcBluetoothBridge::OnSetDiscoverable,
                     weak_factory_.GetWeakPtr(), discoverable, false, timeout));
}

void ArcBluetoothBridge::OnSetAdapterProperty(
    mojom::BluetoothStatus status,
    mojom::BluetoothPropertyPtr property) {
  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnAdapterProperties);
  if (!bluetooth_instance)
    return;

  std::vector<arc::mojom::BluetoothPropertyPtr> properties;
  properties.push_back(std::move(property));

  bluetooth_instance->OnAdapterProperties(status, std::move(properties));
}

void ArcBluetoothBridge::SetAdapterProperty(
    mojom::BluetoothPropertyPtr property) {
  DCHECK(bluetooth_adapter_);

  if (property->is_discovery_timeout()) {
    uint32_t discovery_timeout = property->get_discovery_timeout();
    discoverable_off_timeout_ = std::make_optional(discovery_timeout);
    OnSetAdapterProperty(mojom::BluetoothStatus::SUCCESS, std::move(property));
  } else if (property->is_bdname()) {
    auto property_clone = property.Clone();
    const std::string bdname = property->get_bdname();
    bluetooth_adapter_->SetName(
        bdname,
        base::BindOnce(&ArcBluetoothBridge::OnSetAdapterProperty,
                       weak_factory_.GetWeakPtr(),
                       mojom::BluetoothStatus::SUCCESS, std::move(property)),
        base::BindOnce(&ArcBluetoothBridge::OnSetAdapterProperty,
                       weak_factory_.GetWeakPtr(), mojom::BluetoothStatus::FAIL,
                       std::move(property_clone)));
  } else if (property->is_adapter_scan_mode()) {
    // Only set the BT scan mode to discoverable if requested and Android has
    // set a discovery timeout previously.
    if (property->get_adapter_scan_mode() ==
            mojom::BluetoothScanMode::CONNECTABLE_DISCOVERABLE &&
        discoverable_off_timeout_.has_value()) {
      SetDiscoverable(/*discoverable=*/true, discoverable_off_timeout_.value());
    } else {
      SetDiscoverable(/*discoverable=*/false, /*timeout=*/0);
    }
    OnSetAdapterProperty(mojom::BluetoothStatus::SUCCESS, std::move(property));
  } else {
    // Android does not set any other property type.
    OnSetAdapterProperty(mojom::BluetoothStatus::UNSUPPORTED,
                         std::move(property));
  }
}

void ArcBluetoothBridge::StartDiscovery() {
  discovery_queue_.Push(base::BindOnce(&ArcBluetoothBridge::StartDiscoveryImpl,
                                       weak_factory_.GetWeakPtr()));
}

void ArcBluetoothBridge::StartDiscoveryImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!bluetooth_adapter_) {
    LOG(DFATAL) << "Bluetooth adapter does not exist.";
    return;
  }

  if (discovery_session_) {
    LOG(ERROR) << "Discovery session already running; Reset timeout.";
    discovery_off_timer_.Start(
        FROM_HERE, kDiscoveryTimeout,
        base::BindOnce(&ArcBluetoothBridge::CancelDiscovery,
                       weak_factory_.GetWeakPtr()));
    discovered_devices_.clear();
    discovery_queue_.Pop();
    return;
  }

  bluetooth_adapter_->StartDiscoverySession(
      kScanClientName,
      base::BindOnce(&ArcBluetoothBridge::OnDiscoveryStarted,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&ArcBluetoothBridge::OnDiscoveryError,
                     weak_factory_.GetWeakPtr()));
}

void ArcBluetoothBridge::CancelDiscovery() {
  discovery_queue_.Push(base::BindOnce(&ArcBluetoothBridge::CancelDiscoveryImpl,
                                       weak_factory_.GetWeakPtr()));
}

void ArcBluetoothBridge::StartLEScanImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!bluetooth_adapter_) {
    LOG(DFATAL) << "Bluetooth adapter does not exist.";
    return;
  }

  if (le_scan_session_) {
    LOG(ERROR) << "Discovery session for LE scan already running.";
    StartLEScanOffTimer();
    scanned_devices_.clear();
    discovery_queue_.Pop();
    return;
  }

  bluetooth_adapter_->StartDiscoverySessionWithFilter(
      std::make_unique<BluetoothDiscoveryFilter>(
          device::BLUETOOTH_TRANSPORT_LE),
      kScanClientName,
      base::BindOnce(&ArcBluetoothBridge::OnLEScanStarted,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&ArcBluetoothBridge::OnLEScanError,
                     weak_factory_.GetWeakPtr()));
}

void ArcBluetoothBridge::CancelDiscoveryImpl() {
  discovery_off_timer_.Stop();
  discovery_session_ = nullptr;
  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnDiscoveryStateChanged);
  if (bluetooth_instance != nullptr) {
    bluetooth_instance->OnDiscoveryStateChanged(
        mojom::BluetoothDiscoveryState::STOPPED);
  }
  discovery_queue_.Pop();
}

void ArcBluetoothBridge::StartLEScanOffTimer() {
  // TODO(b/152463320): Android expects to stop the LE scan by itself but not by
  // a timer automatically. We set this timer here due to the potential
  // complains about the power consumption since we cannot set scan parameters
  // and filters now.
  le_scan_off_timer_.Start(
      FROM_HERE, kDiscoveryTimeout,
      base::BindOnce(&ArcBluetoothBridge::StopLEScanByTimer,
                     weak_factory_.GetWeakPtr()));
}

void ArcBluetoothBridge::ResetLEScanSession() {
  le_scan_session_ = nullptr;
}

bool ArcBluetoothBridge::IsDiscoveringOrScanning() {
  return discovery_session_ || le_scan_session_;
}

void ArcBluetoothBridge::StopLEScanImpl() {
  le_scan_off_timer_.Stop();
  ResetLEScanSession();
  discovery_queue_.Pop();
}

void ArcBluetoothBridge::OnPoweredOn(
    ArcBluetoothBridge::AdapterStateCallback callback,
    bool save_user_pref) {
  // Saves the power state to user preference only if Android initiated it.
  if (save_user_pref)
    SetPrimaryUserBluetoothPowerSetting(true);

  std::move(callback).Run(mojom::BluetoothAdapterState::ON);

  // Sends cached devices to Android after its Bluetooth stack is ready. We
  // should do this after the above callback since Android will clear its
  // device cache after receiving the "ON" state of adapter.
  SendCachedDevices();

  // Allow derived classes to perform additional logic if desired.
  HandlePoweredOn();
}

void ArcBluetoothBridge::OnPoweredOff(
    ArcBluetoothBridge::AdapterStateCallback callback,
    bool save_user_pref) {
  // Saves the power state to user preference only if Android initiated it.
  if (save_user_pref)
    SetPrimaryUserBluetoothPowerSetting(false);

  std::move(callback).Run(mojom::BluetoothAdapterState::OFF);
}

void ArcBluetoothBridge::OnPoweredError(
    ArcBluetoothBridge::AdapterStateCallback callback) const {
  LOG(WARNING) << "failed to change power state";

  std::move(callback).Run(bluetooth_adapter_->IsPowered()
                              ? mojom::BluetoothAdapterState::ON
                              : mojom::BluetoothAdapterState::OFF);
}

void ArcBluetoothBridge::OnDiscoveryStarted(
    std::unique_ptr<BluetoothDiscoverySession> session) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // We need to set timer to turn device discovery off because of the difference
  // between Android API (do device discovery once) and Chrome API (do device
  // discovery until user turns it off).
  discovery_off_timer_.Start(
      FROM_HERE, kDiscoveryTimeout,
      base::BindOnce(&ArcBluetoothBridge::CancelDiscovery,
                     weak_factory_.GetWeakPtr()));
  discovery_session_ = std::move(session);
  discovered_devices_.clear();

  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnDiscoveryStateChanged);
  if (bluetooth_instance != nullptr) {
    bluetooth_instance->OnDiscoveryStateChanged(
        mojom::BluetoothDiscoveryState::STARTED);
  }
  discovery_queue_.Pop();
}

void ArcBluetoothBridge::OnLEScanStarted(
    std::unique_ptr<BluetoothDiscoverySession> session) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  StartLEScanOffTimer();
  le_scan_session_ = std::move(session);
  scanned_devices_.clear();

  // Android doesn't need a callback for discovery started event for a LE scan.
  discovery_queue_.Pop();
}

void ArcBluetoothBridge::CreateBond(mojom::BluetoothAddressPtr addr,
                                    int32_t transport) {
  std::string addr_str = addr->To<std::string>();
  BluetoothDevice* device = bluetooth_adapter_->GetDevice(addr_str);
  if (!device || !device->IsPairable()) {
    VLOG(1) << __func__ << ": device " << addr_str
            << " is no longer valid or pairable";
    OnPairedError(std::move(addr), BluetoothDevice::ERROR_FAILED);
    return;
  }

  if (device->IsPaired()) {
    OnPairedDone(std::move(addr));
    return;
  }

  devices_paired_by_arc_.insert(addr_str);

  // BluetoothPairingDialog will automatically pair the device and handle all
  // the incoming pairing requests.
  ash::BluetoothPairingDialog::ShowDialog(device->GetAddress());
}

void ArcBluetoothBridge::RemoveBond(mojom::BluetoothAddressPtr addr) {
  // Forget the device if it is no longer valid or not even paired.
  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(addr->To<std::string>());
  if (!device || !device->IsPaired()) {
    OnForgetDone(std::move(addr));
    return;
  }

  // If unpairing finished successfully, DevicePairedChanged will notify Android
  // on paired state change event, so DoNothing is passed as a success callback.
  device->Forget(base::DoNothing(),
                 base::BindOnce(&ArcBluetoothBridge::OnForgetError,
                                weak_factory_.GetWeakPtr(), std::move(addr)));
}

void ArcBluetoothBridge::CancelBond(mojom::BluetoothAddressPtr addr) {
  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(addr->To<std::string>());
  if (!device) {
    OnForgetDone(std::move(addr));
    return;
  }

  device->CancelPairing();
  OnForgetDone(std::move(addr));
}

void ArcBluetoothBridge::GetConnectionState(
    mojom::BluetoothAddressPtr addr,
    GetConnectionStateCallback callback) {
  if (!bluetooth_adapter_) {
    std::move(callback).Run(false);
    return;
  }

  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(addr->To<std::string>());
  if (!device) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(device->IsConnected());
}

void ArcBluetoothBridge::StartLEScan() {
  discovery_queue_.Push(base::BindOnce(&ArcBluetoothBridge::StartLEScanImpl,
                                       weak_factory_.GetWeakPtr()));
}

void ArcBluetoothBridge::StopLEScan() {
  discovery_queue_.Push(base::BindOnce(&ArcBluetoothBridge::StopLEScanImpl,
                                       weak_factory_.GetWeakPtr()));
}

void ArcBluetoothBridge::StopLEScanByTimer() {
  // If the scan is stopped by the timer, it is possible that the following scan
  // client in Android cannot start the scan successfully but that client will
  // not get an error.
  LOG(WARNING) << "The discovery session for LE scan is stopped by the timer";
  discovery_queue_.Push(base::BindOnce(&ArcBluetoothBridge::StopLEScanImpl,
                                       weak_factory_.GetWeakPtr()));
}

void ArcBluetoothBridge::OnGattConnectStateChanged(
    mojom::BluetoothAddressPtr addr,
    bool connected) const {
  auto* btle_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnLEConnectionStateChange);
  if (!btle_instance)
    return;

  DCHECK(addr);

  btle_instance->OnLEConnectionStateChange(std::move(addr), connected);
}

void ArcBluetoothBridge::OnGattConnect(
    mojom::BluetoothAddressPtr addr,
    std::unique_ptr<BluetoothGattConnection> connection,
    std::optional<BluetoothDevice::ConnectErrorCode> error_code) {
  if (error_code.has_value()) {
    LOG(WARNING) << "GattConnectError: error_code = " << error_code.value();
    OnGattDisconnected(std::move(addr));
    return;
  }
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const std::string addr_str = addr->To<std::string>();
  GattConnection& conn = gatt_connections_[addr_str];
  conn.state = GattConnection::ConnectionState::CONNECTED;
  conn.connection = std::move(connection);
  devices_paired_by_arc_.erase(addr_str);
  OnGattConnectStateChanged(std::move(addr), true);
}

void ArcBluetoothBridge::OnGattDisconnected(mojom::BluetoothAddressPtr addr) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (gatt_connections_.erase(addr->To<std::string>()) == 0) {
    LOG(WARNING) << "OnGattDisconnected called, "
                 << "but no gatt connection was found";
    return;
  }
  OnGattConnectStateChanged(std::move(addr), false);
}

void ArcBluetoothBridge::ConnectLEDevice(
    mojom::BluetoothAddressPtr remote_addr) {
  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnLEConnectionStateChange);
  if (!bluetooth_instance)
    return;

  std::string addr = remote_addr->To<std::string>();
  BluetoothDevice* device = bluetooth_adapter_->GetDevice(addr);

  if (!device) {
    LOG(ERROR) << "Unknown device " << addr;
    OnGattConnect(std::move(remote_addr),
                  /*connection=*/nullptr,
                  BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  auto it = gatt_connections_.find(addr);
  if (it != gatt_connections_.end()) {
    if (it->second.state == GattConnection::ConnectionState::CONNECTED) {
      bluetooth_instance->OnLEConnectionStateChange(std::move(remote_addr),
                                                    true);
    } else {
      OnGattConnect(std::move(remote_addr),
                    /*connection=*/nullptr,
                    BluetoothDevice::ConnectErrorCode::ERROR_INPROGRESS);
    }
    return;
  }

  bool need_hard_disconnect =
      devices_paired_by_arc_.find(addr) != devices_paired_by_arc_.end() ||
      !device->IsConnected();

  // Also pass disconnect callback in error case since it would be disconnected
  // anyway.
  gatt_connections_.emplace(
      addr, GattConnection(GattConnection::ConnectionState::CONNECTING, nullptr,
                           need_hard_disconnect));
  device->CreateGattConnection(
      base::BindOnce(&ArcBluetoothBridge::OnGattConnect,
                     weak_factory_.GetWeakPtr(), std::move(remote_addr)));
}

void ArcBluetoothBridge::DisconnectLEDevice(
    mojom::BluetoothAddressPtr remote_addr) {
  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnLEConnectionStateChange);
  if (!bluetooth_instance)
    return;

  const std::string addr_str = remote_addr->To<std::string>();

  BluetoothDevice* device = bluetooth_adapter_->GetDevice(addr_str);
  const auto conn_itr = gatt_connections_.find(remote_addr->To<std::string>());

  if (!device || !device->IsConnected() ||
      conn_itr == gatt_connections_.end()) {
    bluetooth_instance->OnLEConnectionStateChange(std::move(remote_addr),
                                                  false);
    return;
  }

  if (conn_itr->second.need_hard_disconnect)
    device->Disconnect(base::DoNothing(), base::DoNothing());

  // Removes the connection object held by us and notifies Android.
  OnGattDisconnected(std::move(remote_addr));
}

void ArcBluetoothBridge::SearchService(mojom::BluetoothAddressPtr remote_addr) {
  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnSearchComplete);
  if (!bluetooth_instance)
    return;

  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(remote_addr->To<std::string>());
  if (!device) {
    LOG(ERROR) << "Unknown device " << remote_addr->To<std::string>();
    bluetooth_instance->OnSearchComplete(
        std::move(remote_addr), mojom::BluetoothGattStatus::GATT_FAILURE);
    return;
  }

  // Call the callback if discovery is completed
  if (device->IsGattServicesDiscoveryComplete()) {
    bluetooth_instance->OnSearchComplete(
        std::move(remote_addr), mojom::BluetoothGattStatus::GATT_SUCCESS);
    return;
  }

  // Discard result. Will call the callback when discovery is completed.
  device->GetGattServices();
}

void ArcBluetoothBridge::GetGattDB(mojom::BluetoothAddressPtr remote_addr) {
  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnGetGattDB);
  if (!bluetooth_instance)
    return;

  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(remote_addr->To<std::string>());
  std::vector<mojom::BluetoothGattDBElementPtr> db;

  if (!device) {
    LOG(ERROR) << "Unknown device " << remote_addr->To<std::string>();
    bluetooth_instance->OnGetGattDB(std::move(remote_addr), std::move(db));
    return;
  }

  for (auto* service : device->GetGattServices()) {
    mojom::BluetoothGattDBElementPtr service_element = CreateGattDBElement(
        service->IsPrimary()
            ? mojom::BluetoothGattDBAttributeType::BTGATT_DB_PRIMARY_SERVICE
            : mojom::BluetoothGattDBAttributeType::BTGATT_DB_SECONDARY_SERVICE,
        service);
    if (!service_element)
      continue;

    const auto& characteristics = service->GetCharacteristics();
    if (characteristics.size() > 0) {
      const auto& descriptors = characteristics.back()->GetDescriptors();
      const std::optional<uint16_t> start_handle =
          ConvertGattIdentifierToId(characteristics.front()->GetIdentifier());
      if (!start_handle)
        continue;

      const std::optional<uint16_t> end_handle = ConvertGattIdentifierToId(
          descriptors.size() > 0 ? descriptors.back()->GetIdentifier()
                                 : characteristics.back()->GetIdentifier());
      if (!end_handle)
        continue;

      service_element->start_handle = *start_handle;
      service_element->end_handle = *end_handle;
    }
    db.push_back(std::move(service_element));

    for (auto* characteristic : characteristics) {
      mojom::BluetoothGattDBElementPtr characteristic_element =
          CreateGattDBElement(
              mojom::BluetoothGattDBAttributeType::BTGATT_DB_CHARACTERISTIC,
              characteristic);
      if (!characteristic_element)
        continue;

      characteristic_element->properties = characteristic->GetProperties();
      db.push_back(std::move(characteristic_element));

      for (auto* descriptor : characteristic->GetDescriptors()) {
        mojom::BluetoothGattDBElementPtr descriptor_element =
            CreateGattDBElement(
                mojom::BluetoothGattDBAttributeType::BTGATT_DB_DESCRIPTOR,
                descriptor);
        if (!descriptor_element)
          continue;

        db.push_back(std::move(descriptor_element));
      }
    }
  }

  bluetooth_instance->OnGetGattDB(std::move(remote_addr), std::move(db));
}

BluetoothRemoteGattCharacteristic* ArcBluetoothBridge::FindGattCharacteristic(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattServiceIDPtr service_id,
    mojom::BluetoothGattIDPtr char_id) const {
  DCHECK(remote_addr);
  DCHECK(service_id);
  DCHECK(char_id);

  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(remote_addr->To<std::string>());
  if (!device)
    return nullptr;

  BluetoothRemoteGattService* service =
      FindGattAttributeByUuid(device->GetGattServices(), service_id->id->uuid);
  if (!service)
    return nullptr;

  return FindGattAttributeByUuid(service->GetCharacteristics(), char_id->uuid);
}

BluetoothRemoteGattDescriptor* ArcBluetoothBridge::FindGattDescriptor(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattServiceIDPtr service_id,
    mojom::BluetoothGattIDPtr char_id,
    mojom::BluetoothGattIDPtr desc_id) const {
  BluetoothRemoteGattCharacteristic* characteristic = FindGattCharacteristic(
      std::move(remote_addr), std::move(service_id), std::move(char_id));
  if (!characteristic)
    return nullptr;

  return FindGattAttributeByUuid(characteristic->GetDescriptors(),
                                 desc_id->uuid);
}

void ArcBluetoothBridge::SendBluetoothPoweredStateBroadcast(
    AdapterPowerState powered) const {
  auto* intent_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->intent_helper(), SendBroadcast);
  if (!intent_instance)
    return;

  base::Value::Dict extras;
  extras.Set("enable", powered == AdapterPowerState::TURN_ON);
  std::string extras_json;
  bool write_success = base::JSONWriter::Write(extras, &extras_json);
  DCHECK(write_success);

  BLUETOOTH_LOG(EVENT) << "Sending Android intent to set power: "
                       << (powered == AdapterPowerState::TURN_ON);
  intent_instance->SendBroadcast(
      ArcIntentHelperBridge::AppendStringToIntentHelperPackageName(
          "SET_BLUETOOTH_STATE"),
      kArcIntentHelperPackageName,
      ArcIntentHelperBridge::AppendStringToIntentHelperPackageName(
          "SettingsReceiver"),
      extras_json);
}

void ArcBluetoothBridge::ReadGattCharacteristic(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattServiceIDPtr service_id,
    mojom::BluetoothGattIDPtr char_id,
    ReadGattCharacteristicCallback callback) {
  BluetoothRemoteGattCharacteristic* characteristic = FindGattCharacteristic(
      std::move(remote_addr), std::move(service_id), std::move(char_id));
  if (!characteristic) {
    // TODO(b/201737474): Investigate in what case this could happen.
    LOG(ERROR) << "Requested GATT characteristic does not exist";
    OnGattRead(std::move(callback),
               device::BluetoothGattService::GattErrorCode::kFailed,
               /*result=*/{});
    return;
  }

  characteristic->ReadRemoteCharacteristic(
      base::BindOnce(&OnGattRead, std::move(callback)));
}

void ArcBluetoothBridge::WriteGattCharacteristic(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattServiceIDPtr service_id,
    mojom::BluetoothGattIDPtr char_id,
    mojom::BluetoothGattValuePtr value,
    bool prepare,
    WriteGattCharacteristicCallback callback) {
  BluetoothRemoteGattCharacteristic* characteristic = FindGattCharacteristic(
      std::move(remote_addr), std::move(service_id), std::move(char_id));
  if (!characteristic) {
    BLUETOOTH_LOG(ERROR) << __func__
                         << " failed to write GATT characteristic, "
                            "characteristic does not exist.";
    std::move(callback).Run(mojom::BluetoothGattStatus::GATT_FAILURE);
    return;
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  if (prepare) {
    characteristic->PrepareWriteRemoteCharacteristic(
        value->value,
        base::BindOnce(&OnGattOperationDone, std::move(split_callback.first)),
        base::BindOnce(&OnGattOperationError,
                       std::move(split_callback.second)));
  } else {
    characteristic->DeprecatedWriteRemoteCharacteristic(
        value->value,
        base::BindOnce(&OnGattOperationDone, std::move(split_callback.first)),
        base::BindOnce(&OnGattOperationError,
                       std::move(split_callback.second)));
  }
}

void ArcBluetoothBridge::ReadGattDescriptor(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattServiceIDPtr service_id,
    mojom::BluetoothGattIDPtr char_id,
    mojom::BluetoothGattIDPtr desc_id,
    ReadGattDescriptorCallback callback) {
  BluetoothRemoteGattDescriptor* descriptor =
      FindGattDescriptor(std::move(remote_addr), std::move(service_id),
                         std::move(char_id), std::move(desc_id));

  if (!descriptor) {
    BLUETOOTH_LOG(ERROR)
        << __func__
        << " failed to read GATT descriptor, descriptor does not exist.";
    arc::mojom::BluetoothGattValuePtr gatt_value =
        mojom::BluetoothGattValue::New();
    gatt_value->status = mojom::BluetoothGattStatus::GATT_FAILURE;
    std::move(callback).Run(std::move(gatt_value));
    return;
  }

  descriptor->ReadRemoteDescriptor(
      base::BindOnce(&OnGattRead, std::move(callback)));
}

void ArcBluetoothBridge::WriteGattDescriptor(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattServiceIDPtr service_id,
    mojom::BluetoothGattIDPtr char_id,
    mojom::BluetoothGattIDPtr desc_id,
    mojom::BluetoothGattValuePtr value,
    WriteGattDescriptorCallback callback) {
  BluetoothRemoteGattDescriptor* descriptor =
      FindGattDescriptor(std::move(remote_addr), std::move(service_id),
                         std::move(char_id), std::move(desc_id));

  if (!descriptor) {
    BLUETOOTH_LOG(ERROR)
        << __func__
        << " failed to write GATT descriptor, descriptor does not exist.";
    std::move(callback).Run(mojom::BluetoothGattStatus::GATT_FAILURE);
    return;
  }

  if (value->value.empty()) {
    std::move(callback).Run(mojom::BluetoothGattStatus::GATT_FAILURE);
    return;
  }

  if (descriptor->GetUUID() !=
      BluetoothGattDescriptor::ClientCharacteristicConfigurationUuid()) {
    auto split_callback = base::SplitOnceCallback(std::move(callback));
    descriptor->WriteRemoteDescriptor(
        value->value,
        base::BindOnce(&OnGattOperationDone, std::move(split_callback.first)),
        base::BindOnce(&OnGattOperationError,
                       std::move(split_callback.second)));
    return;
  }

  BluetoothRemoteGattCharacteristic* characteristic =
      descriptor->GetCharacteristic();
  std::string char_id_str = characteristic->GetIdentifier();
  auto it = notification_session_.find(char_id_str);
  if (it != notification_session_.end()) {
    // Stop the previous session while keeping this client registered.
    it->second.reset();
  }
  // If notification session does not exist, a write to gatt descriptor will
  // only enable peer device to notify us, but our clients won't hear the change
  // events. Though this use case is not typical, we should still allow the API
  // to succeed since the public API doesn't disallow this
  // (https://developer.android.com/reference/android/bluetooth/BluetoothGatt#writeDescriptor(android.bluetooth.BluetoothGattDescriptor)).

  switch (value->value[0]) {
    case DISABLE_NOTIFICATION_VALUE:
      std::move(callback).Run(mojom::BluetoothGattStatus::GATT_SUCCESS);
      return;
    case ENABLE_NOTIFICATION_VALUE: {
      auto split_callback = base::SplitOnceCallback(std::move(callback));
      characteristic->StartNotifySession(
          device::BluetoothGattCharacteristic::NotificationType::kNotification,
          base::BindOnce(&ArcBluetoothBridge::OnGattNotifyStartDone,
                         weak_factory_.GetWeakPtr(),
                         std::move(split_callback.first), char_id_str),
          base::BindOnce(&OnGattOperationError,
                         std::move(split_callback.second)));
      return;
    }
    case ENABLE_INDICATION_VALUE: {
      auto split_callback = base::SplitOnceCallback(std::move(callback));
      characteristic->StartNotifySession(
          device::BluetoothGattCharacteristic::NotificationType::kIndication,
          base::BindOnce(&ArcBluetoothBridge::OnGattNotifyStartDone,
                         weak_factory_.GetWeakPtr(),
                         std::move(split_callback.first), char_id_str),
          base::BindOnce(&OnGattOperationError,
                         std::move(split_callback.second)));
      return;
    }
    default:
      std::move(callback).Run(mojom::BluetoothGattStatus::GATT_FAILURE);
  }
}

void ArcBluetoothBridge::ExecuteWrite(mojom::BluetoothAddressPtr remote_addr,
                                      bool execute,
                                      ExecuteWriteCallback callback) {
  device::BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(remote_addr->To<std::string>());

  if (device == nullptr) {
    std::move(callback).Run(mojom::BluetoothGattStatus::GATT_FAILURE);
    return;
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  if (execute) {
    device->ExecuteWrite(
        base::BindOnce(&OnGattOperationDone, std::move(split_callback.first)),
        base::BindOnce(&OnGattOperationError,
                       std::move(split_callback.second)));
  } else {
    device->AbortWrite(
        base::BindOnce(&OnGattOperationDone, std::move(split_callback.first)),
        base::BindOnce(&OnGattOperationError,
                       std::move(split_callback.second)));
  }
}

void ArcBluetoothBridge::OnGattNotifyStartDone(
    ArcBluetoothBridge::GattStatusCallback callback,
    const std::string char_string_id,
    std::unique_ptr<BluetoothGattNotifySession> notify_session) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Hold on to |notify_session|. Destruction of |notify_session| is equivalent
  // to stopping this session.
  notification_session_[char_string_id] = std::move(notify_session);
  std::move(callback).Run(mojom::BluetoothGattStatus::GATT_SUCCESS);
}

void ArcBluetoothBridge::RegisterForGattNotification(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattServiceIDPtr service_id,
    mojom::BluetoothGattIDPtr char_id,
    RegisterForGattNotificationCallback callback) {
  BluetoothRemoteGattCharacteristic* characteristic = FindGattCharacteristic(
      std::move(remote_addr), std::move(service_id), std::move(char_id));
  if (characteristic == nullptr) {
    LOG(WARNING) << __func__ << " Characteristic is not existed.";
    std::move(callback).Run(mojom::BluetoothGattStatus::GATT_FAILURE);
    return;
  }

  std::string char_id_str = characteristic->GetIdentifier();
  // Create an empty notification session object - which will be fully
  // instantiated by a future call to WriteGattDescriptor - if one does not yet
  // exist. Note that the notification session object may have been created as
  // a result of a previous call to WriteGattDescriptor, which can be left
  // as-is.
  if (!base::Contains(notification_session_, char_id_str)) {
    notification_session_.emplace(char_id_str, nullptr);
  }

  std::move(callback).Run(mojom::BluetoothGattStatus::GATT_SUCCESS);
}

void ArcBluetoothBridge::DeregisterForGattNotification(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattServiceIDPtr service_id,
    mojom::BluetoothGattIDPtr char_id,
    DeregisterForGattNotificationCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  BluetoothRemoteGattCharacteristic* characteristic = FindGattCharacteristic(
      std::move(remote_addr), std::move(service_id), std::move(char_id));
  if (characteristic == nullptr) {
    LOG(WARNING) << __func__ << " Characteristic is not existed.";
    std::move(callback).Run(mojom::BluetoothGattStatus::GATT_FAILURE);
    return;
  }

  std::string char_id_str = characteristic->GetIdentifier();
  auto it = notification_session_.find(char_id_str);
  if (it == notification_session_.end()) {
    LOG(WARNING) << "Notification session not found " << char_id_str;
    std::move(callback).Run(mojom::BluetoothGattStatus::GATT_FAILURE);
    return;
  }
  notification_session_.erase(it);
  std::move(callback).Run(mojom::BluetoothGattStatus::GATT_SUCCESS);
}

void ArcBluetoothBridge::ReadRemoteRssi(mojom::BluetoothAddressPtr remote_addr,
                                        ReadRemoteRssiCallback callback) {
  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(remote_addr->To<std::string>());
  if (!device) {
    std::move(callback).Run(mojom::kUnknownPower);
    return;
  }

  if (device->IsConnected()) {
    device->GetConnectionInfo(
        base::BindOnce(&SendRssiOnGetConnectionInfoDone, std::move(callback)));
  } else {
    std::move(callback).Run(
        device->GetInquiryRSSI().value_or(mojom::kUnknownPower));
  }
}

bool ArcBluetoothBridge::IsGattServerAttributeHandleAvailable(int need) {
  return gatt_server_attribute_next_handle_ + need <= kMaxGattAttributeHandle;
}

int32_t ArcBluetoothBridge::GetNextGattServerAttributeHandle() {
  return IsGattServerAttributeHandleAvailable(1)
             ? ++gatt_server_attribute_next_handle_
             : kInvalidGattAttributeHandle;
}

template <class LocalGattAttribute>
int32_t ArcBluetoothBridge::CreateGattAttributeHandle(
    LocalGattAttribute* attribute) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!attribute)
    return kInvalidGattAttributeHandle;
  int32_t handle = GetNextGattServerAttributeHandle();
  if (handle == kInvalidGattAttributeHandle)
    return kInvalidGattAttributeHandle;
  const std::string& identifier = attribute->GetIdentifier();
  gatt_identifier_[handle] = identifier;
  gatt_handle_[identifier] = handle;
  return handle;
}

void ArcBluetoothBridge::AddService(mojom::BluetoothGattServiceIDPtr service_id,
                                    int32_t num_handles,
                                    AddServiceCallback callback) {
  if (!IsGattServerAttributeHandleAvailable(num_handles)) {
    std::move(callback).Run(kInvalidGattAttributeHandle);
    return;
  }

  base::WeakPtr<BluetoothLocalGattService> service =
      bluetooth_adapter_->CreateLocalGattService(
          service_id->id->uuid, service_id->is_primary, this /* delegate */);
  std::move(callback).Run(CreateGattAttributeHandle(service.get()));
}

void ArcBluetoothBridge::AddCharacteristic(int32_t service_handle,
                                           const BluetoothUUID& uuid,
                                           int32_t properties,
                                           int32_t permissions,
                                           AddCharacteristicCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto gatt_id = gatt_identifier_.find(service_handle);
  DCHECK(gatt_id != gatt_identifier_.end());

  if (!IsGattServerAttributeHandleAvailable(1)) {
    std::move(callback).Run(kInvalidGattAttributeHandle);
    return;
  }

  const auto& [bluez_properties, bluez_permissions] =
      floss::BluetoothGattCharacteristicFloss::ConvertPropsAndPermsFromFloss(
          static_cast<uint8_t>(properties), static_cast<uint16_t>(permissions));

  auto* service = bluetooth_adapter_->GetGattService(gatt_id->second);
  if (!service) {
    return;
  }

  base::WeakPtr<BluetoothLocalGattCharacteristic> characteristic =
      service->CreateCharacteristic(uuid, bluez_properties, bluez_permissions);
  int32_t characteristic_handle =
      CreateGattAttributeHandle(characteristic.get());
  last_characteristic_[service_handle] = characteristic_handle;
  std::move(callback).Run(characteristic_handle);
}

void ArcBluetoothBridge::AddDescriptor(int32_t service_handle,
                                       const BluetoothUUID& uuid,
                                       int32_t permissions,
                                       AddDescriptorCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsGattServerAttributeHandleAvailable(1)) {
    std::move(callback).Run(kInvalidGattAttributeHandle);
    return;
  }

  DCHECK(gatt_identifier_.find(service_handle) != gatt_identifier_.end());
  BluetoothLocalGattService* service =
      bluetooth_adapter_->GetGattService(gatt_identifier_[service_handle]);
  DCHECK(service);
  // Since the Android API does not give information about which characteristic
  // is the parent of the new descriptor, we assume that it would be the last
  // characteristic that was added to the given service. This matches the
  // Android framework code at android/bluetooth/BluetoothGattServer.java#594.
  // https://android.googlesource.com/platform/frameworks/base/+/android-6.0.1_r55/core/java/android/bluetooth/BluetoothGattServer.java#586
  DCHECK(last_characteristic_.find(service_handle) !=
         last_characteristic_.end());
  int32_t last_characteristic_handle = last_characteristic_[service_handle];

  DCHECK(gatt_identifier_.find(last_characteristic_handle) !=
         gatt_identifier_.end());
  BluetoothLocalGattCharacteristic* characteristic =
      service->GetCharacteristic(gatt_identifier_[last_characteristic_handle]);
  DCHECK(characteristic);

  const auto& [unused, bluez_permissions] =
      floss::BluetoothGattCharacteristicFloss::ConvertPropsAndPermsFromFloss(
          /*properties=*/0, static_cast<uint16_t>(permissions));

  base::WeakPtr<BluetoothLocalGattDescriptor> descriptor =
      BluetoothLocalGattDescriptor::Create(uuid, bluez_permissions,
                                           characteristic);
  std::move(callback).Run(CreateGattAttributeHandle(descriptor.get()));
}

void ArcBluetoothBridge::StartService(int32_t service_handle,
                                      StartServiceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(gatt_identifier_.find(service_handle) != gatt_identifier_.end());
  BluetoothLocalGattService* service =
      bluetooth_adapter_->GetGattService(gatt_identifier_[service_handle]);
  DCHECK(service);

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  service->Register(
      base::BindOnce(&OnGattOperationDone, std::move(split_callback.first)),
      base::BindOnce(&OnGattOperationError, std::move(split_callback.second)));
}

void ArcBluetoothBridge::StopService(int32_t service_handle,
                                     StopServiceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(gatt_identifier_.find(service_handle) != gatt_identifier_.end());
  BluetoothLocalGattService* service =
      bluetooth_adapter_->GetGattService(gatt_identifier_[service_handle]);
  DCHECK(service);

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  service->Unregister(
      base::BindOnce(&OnGattOperationDone, std::move(split_callback.first)),
      base::BindOnce(&OnGattOperationError, std::move(split_callback.second)));
}

void ArcBluetoothBridge::DeleteService(int32_t service_handle,
                                       DeleteServiceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto itr = gatt_identifier_.find(service_handle);
  if (itr == gatt_identifier_.end()) {
    LOG(WARNING) << "DeleteService called with invalid service handle.";
    return;
  }

  BluetoothLocalGattService* service =
      bluetooth_adapter_->GetGattService(itr->second);
  DCHECK(service);
  gatt_identifier_.erase(itr);
  gatt_handle_.erase(service->GetIdentifier());
  service->Delete();
  OnGattOperationDone(std::move(callback));
}

void ArcBluetoothBridge::SendIndication(int32_t attribute_handle,
                                        mojom::BluetoothAddressPtr address,
                                        bool confirm,
                                        const std::vector<uint8_t>& value,
                                        SendIndicationCallback callback) {
  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(address->To<std::string>());
  auto identifier = gatt_identifier_.find(attribute_handle);
  if (device == nullptr || identifier == gatt_identifier_.end()) {
    std::move(callback).Run(mojom::BluetoothGattStatus::GATT_FAILURE);
    return;
  }

  for (const auto& service_handle : last_characteristic_) {
    auto it = gatt_identifier_.find(service_handle.first);
    if (it == gatt_identifier_.end())
      continue;
    BluetoothLocalGattService* service =
        bluetooth_adapter_->GetGattService(it->second);
    if (service == nullptr)
      continue;
    BluetoothLocalGattCharacteristic* characteristic =
        service->GetCharacteristic(identifier->second);
    if (characteristic == nullptr)
      continue;
    characteristic->NotifyValueChanged(device, value, confirm);
    std::move(callback).Run(mojom::BluetoothGattStatus::GATT_SUCCESS);
    return;
  }

  std::move(callback).Run(mojom::BluetoothGattStatus::GATT_FAILURE);
}

bool ArcBluetoothBridge::GetAdvertisementHandle(int32_t* adv_handle) {
  for (int i = 0; i < kMaxAdvertisements; i++) {
    if (advertisements_.find(i) == advertisements_.end()) {
      *adv_handle = i;
      return true;
    }
  }
  return false;
}

void ArcBluetoothBridge::ReserveAdvertisementHandle(
    ReserveAdvertisementHandleCallback callback) {
  advertisement_queue_.Push(
      base::BindOnce(&ArcBluetoothBridge::ReserveAdvertisementHandleImpl,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcBluetoothBridge::ReserveAdvertisementHandleImpl(
    ReserveAdvertisementHandleCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Find an empty advertisement slot.
  int32_t adv_handle;
  if (!GetAdvertisementHandle(&adv_handle)) {
    LOG(WARNING) << "Out of space for advertisement data";
    std::move(callback).Run(mojom::BluetoothGattStatus::GATT_FAILURE,
                            kInvalidAdvertisementHandle);
    advertisement_queue_.Pop();
    return;
  }

  // We have a handle. Put an entry in the map to reserve it.
  advertisements_[adv_handle] = nullptr;

  // The advertisement will be registered when we get the call
  // to SetAdvertisingData. For now, just return the adv_handle.
  std::move(callback).Run(mojom::BluetoothGattStatus::GATT_SUCCESS, adv_handle);
  advertisement_queue_.Pop();
}

void ArcBluetoothBridge::EnableAdvertisement(
    int32_t adv_handle,
    std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement,
    EnableAdvertisementCallback callback) {
  advertisement_queue_.Push(base::BindOnce(
      &ArcBluetoothBridge::EnableAdvertisementImpl, weak_factory_.GetWeakPtr(),
      adv_handle, std::move(advertisement), std::move(callback)));
}

void ArcBluetoothBridge::EnableAdvertisementImpl(
    int32_t adv_handle,
    std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement,
    EnableAdvertisementCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  base::OnceClosure done_callback = base::BindOnce(
      &ArcBluetoothBridge::OnReadyToRegisterAdvertisement,
      weak_factory_.GetWeakPtr(), std::move(split_callback.first), adv_handle,
      std::move(advertisement));
  base::OnceCallback<void(BluetoothAdvertisement::ErrorCode)> error_callback =
      base::BindOnce(&ArcBluetoothBridge::OnRegisterAdvertisementError,
                     weak_factory_.GetWeakPtr(),
                     std::move(split_callback.second), adv_handle);

  auto it = advertisements_.find(adv_handle);
  if (it == advertisements_.end()) {
    std::move(error_callback)
        .Run(BluetoothAdvertisement::ErrorCode::
                 ERROR_ADVERTISEMENT_DOES_NOT_EXIST);
    return;
  }
  if (it->second == nullptr) {
    std::move(done_callback).Run();
    return;
  }
  it->second->Unregister(std::move(done_callback), std::move(error_callback));
}

void ArcBluetoothBridge::DisableAdvertisement(
    int32_t adv_handle,
    EnableAdvertisementCallback callback) {
  advertisement_queue_.Push(base::BindOnce(
      &ArcBluetoothBridge::DisableAdvertisementImpl, weak_factory_.GetWeakPtr(),
      adv_handle, std::move(callback)));
}

void ArcBluetoothBridge::DisableAdvertisementImpl(
    int32_t adv_handle,
    EnableAdvertisementCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  base::OnceClosure done_callback = base::BindOnce(
      &ArcBluetoothBridge::OnUnregisterAdvertisementDone,
      weak_factory_.GetWeakPtr(), std::move(split_callback.first), adv_handle);
  base::OnceCallback<void(BluetoothAdvertisement::ErrorCode)> error_callback =
      base::BindOnce(&ArcBluetoothBridge::OnUnregisterAdvertisementError,
                     weak_factory_.GetWeakPtr(),
                     std::move(split_callback.second), adv_handle);

  auto it = advertisements_.find(adv_handle);
  if (it == advertisements_.end()) {
    std::move(error_callback)
        .Run(BluetoothAdvertisement::ErrorCode::
                 ERROR_ADVERTISEMENT_DOES_NOT_EXIST);
    return;
  }
  if (it->second == nullptr) {
    std::move(done_callback).Run();
    return;
  }
  it->second->Unregister(std::move(done_callback), std::move(error_callback));
}

void ArcBluetoothBridge::ReleaseAdvertisementHandle(
    int32_t adv_handle,
    ReleaseAdvertisementHandleCallback callback) {
  advertisement_queue_.Push(base::BindOnce(
      &ArcBluetoothBridge::ReleaseAdvertisementHandleImpl,
      weak_factory_.GetWeakPtr(), adv_handle, std::move(callback)));
}

void ArcBluetoothBridge::ReleaseAdvertisementHandleImpl(
    int32_t adv_handle,
    ReleaseAdvertisementHandleCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (advertisements_.find(adv_handle) == advertisements_.end()) {
    std::move(callback).Run(mojom::BluetoothGattStatus::GATT_FAILURE);
    advertisement_queue_.Pop();
    return;
  }

  if (!advertisements_[adv_handle]) {
    advertisements_.erase(adv_handle);
    std::move(callback).Run(mojom::BluetoothGattStatus::GATT_SUCCESS);
    advertisement_queue_.Pop();
    return;
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  advertisements_[adv_handle]->Unregister(
      base::BindOnce(&ArcBluetoothBridge::OnReleaseAdvertisementHandleDone,
                     weak_factory_.GetWeakPtr(),
                     std::move(split_callback.first), adv_handle),
      base::BindOnce(&ArcBluetoothBridge::OnReleaseAdvertisementHandleError,
                     weak_factory_.GetWeakPtr(),
                     std::move(split_callback.second), adv_handle));
}

void ArcBluetoothBridge::OnReadyToRegisterAdvertisement(
    ArcBluetoothBridge::GattStatusCallback callback,
    int32_t adv_handle,
    std::unique_ptr<device::BluetoothAdvertisement::Data> data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  bluetooth_adapter_->RegisterAdvertisement(
      std::move(data),
      base::BindOnce(&ArcBluetoothBridge::OnRegisterAdvertisementDone,
                     weak_factory_.GetWeakPtr(),
                     std::move(split_callback.first), adv_handle),
      base::BindOnce(&ArcBluetoothBridge::OnRegisterAdvertisementError,
                     weak_factory_.GetWeakPtr(),
                     std::move(split_callback.second), adv_handle));
}

void ArcBluetoothBridge::OnRegisterAdvertisementDone(
    ArcBluetoothBridge::GattStatusCallback callback,
    int32_t adv_handle,
    scoped_refptr<BluetoothAdvertisement> advertisement) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  advertisements_[adv_handle] = std::move(advertisement);
  std::move(callback).Run(mojom::BluetoothGattStatus::GATT_SUCCESS);
  advertisement_queue_.Pop();
}

void ArcBluetoothBridge::OnRegisterAdvertisementError(
    ArcBluetoothBridge::GattStatusCallback callback,
    int32_t adv_handle,
    BluetoothAdvertisement::ErrorCode error_code) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LOG(WARNING) << "Failed to register advertisement for handle " << adv_handle
               << ", error code = " << error_code;
  advertisements_[adv_handle] = nullptr;
  std::move(callback).Run(mojom::BluetoothGattStatus::GATT_FAILURE);
  advertisement_queue_.Pop();
}

void ArcBluetoothBridge::OnUnregisterAdvertisementDone(
    ArcBluetoothBridge::GattStatusCallback callback,
    int32_t adv_handle) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  advertisements_[adv_handle] = nullptr;
  std::move(callback).Run(mojom::BluetoothGattStatus::GATT_SUCCESS);
  advertisement_queue_.Pop();
}

void ArcBluetoothBridge::OnUnregisterAdvertisementError(
    ArcBluetoothBridge::GattStatusCallback callback,
    int32_t adv_handle,
    BluetoothAdvertisement::ErrorCode error_code) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LOG(WARNING) << "Failed to unregister advertisement for handle " << adv_handle
               << ", error code = " << error_code;
  advertisements_[adv_handle] = nullptr;
  std::move(callback).Run(mojom::BluetoothGattStatus::GATT_FAILURE);
  advertisement_queue_.Pop();
}

void ArcBluetoothBridge::OnReleaseAdvertisementHandleDone(
    ArcBluetoothBridge::GattStatusCallback callback,
    int32_t adv_handle) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  advertisements_.erase(adv_handle);
  std::move(callback).Run(mojom::BluetoothGattStatus::GATT_SUCCESS);
  advertisement_queue_.Pop();
}

void ArcBluetoothBridge::OnReleaseAdvertisementHandleError(
    ArcBluetoothBridge::GattStatusCallback callback,
    int32_t adv_handle,
    BluetoothAdvertisement::ErrorCode error_code) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LOG(WARNING) << "Failed to relase advertisement handle " << adv_handle
               << ", error code = " << error_code;
  advertisements_.erase(adv_handle);
  std::move(callback).Run(mojom::BluetoothGattStatus::GATT_FAILURE);
  advertisement_queue_.Pop();
}

void ArcBluetoothBridge::OnDiscoveryError() {
  LOG(WARNING) << "failed to change discovery state";
  discovery_queue_.Pop();
}

void ArcBluetoothBridge::OnLEScanError() {
  LOG(WARNING) << "failed to start LE scan";
  discovery_queue_.Pop();
}

void ArcBluetoothBridge::OnPairing(mojom::BluetoothAddressPtr addr) const {
  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnBondStateChanged);
  if (!bluetooth_instance)
    return;

  bluetooth_instance->OnBondStateChanged(mojom::BluetoothStatus::SUCCESS,
                                         std::move(addr),
                                         mojom::BluetoothBondState::BONDING);
}

void ArcBluetoothBridge::OnPairedDone(mojom::BluetoothAddressPtr addr) const {
  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnBondStateChanged);
  if (!bluetooth_instance)
    return;

  bluetooth_instance->OnBondStateChanged(mojom::BluetoothStatus::SUCCESS,
                                         std::move(addr),
                                         mojom::BluetoothBondState::BONDED);
}

void ArcBluetoothBridge::OnPairedError(
    mojom::BluetoothAddressPtr addr,
    BluetoothDevice::ConnectErrorCode error_code) const {
  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnBondStateChanged);
  if (!bluetooth_instance)
    return;

  bluetooth_instance->OnBondStateChanged(mojom::BluetoothStatus::FAIL,
                                         std::move(addr),
                                         mojom::BluetoothBondState::NONE);
}

void ArcBluetoothBridge::OnForgetDone(mojom::BluetoothAddressPtr addr) {
  devices_paired_by_arc_.erase(addr->To<std::string>());
  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnBondStateChanged);
  if (!bluetooth_instance)
    return;

  bluetooth_instance->OnBondStateChanged(mojom::BluetoothStatus::SUCCESS,
                                         std::move(addr),
                                         mojom::BluetoothBondState::NONE);
}

void ArcBluetoothBridge::OnForgetError(mojom::BluetoothAddressPtr addr) const {
  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnBondStateChanged);
  if (!bluetooth_instance)
    return;

  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(addr->To<std::string>());
  mojom::BluetoothBondState bond_state = mojom::BluetoothBondState::NONE;
  if (device && device->IsPaired()) {
    bond_state = mojom::BluetoothBondState::BONDED;
  }
  bluetooth_instance->OnBondStateChanged(mojom::BluetoothStatus::FAIL,
                                         std::move(addr), bond_state);
}

bool ArcBluetoothBridge::IsPowerChangeInitiatedByRemote(
    ArcBluetoothBridge::AdapterPowerState powered) const {
  return !remote_power_changes_.empty() &&
         remote_power_changes_.front() == powered;
}

bool ArcBluetoothBridge::IsPowerChangeInitiatedByLocal(
    ArcBluetoothBridge::AdapterPowerState powered) const {
  return !local_power_changes_.empty() &&
         local_power_changes_.front() == powered;
}

void ArcBluetoothBridge::OnConnectionReady() {
  if (!bluetooth_adapter_ || !bluetooth_adapter_->IsPowered()) {
    // The default power state of Bluetooth on Android is off, so there is no
    // need to send an intent to turn off Bluetooth if the initial power state
    // is off.
    return;
  }

  // Send initial power state in case both, Intent Helper and App instances are
  // present. Intent Helper is required to dispatch this event and App is sign
  // that ARC is fully started. In case of initial boot, App instance is started
  // after the Intent Helper instance. In case of next boot Intent Helper and
  // App instances are started at almost the same time and order of start is not
  // determined.
  if (!arc_bridge_service_->app()->IsConnected() ||
      !arc_bridge_service_->intent_helper()->IsConnected()) {
    return;
  }

  EnqueueLocalPowerChange(AdapterPowerState::TURN_ON);
}

void ArcBluetoothBridge::EnqueueLocalPowerChange(
    ArcBluetoothBridge::AdapterPowerState powered) {
  local_power_changes_.push(powered);

  if (power_intent_timer_.IsRunning())
    return;

  SendBluetoothPoweredStateBroadcast(local_power_changes_.front());
  power_intent_timer_.Start(
      FROM_HERE, kPowerIntentTimeout,
      base::BindOnce(&ArcBluetoothBridge::DequeueLocalPowerChange,
                     weak_factory_.GetWeakPtr(), powered));
}

void ArcBluetoothBridge::DequeueLocalPowerChange(
    ArcBluetoothBridge::AdapterPowerState powered) {
  power_intent_timer_.Stop();

  if (!IsPowerChangeInitiatedByLocal(powered))
    return;

  AdapterPowerState current_change = local_power_changes_.front();
  AdapterPowerState last_change = local_power_changes_.back();

  // Compress the queue for power intent to reduce the amount of intents being
  // sent to Android so that the powered state will be synced between Android
  // and Chrome even if the state is toggled repeatedly on Chrome.
  base::queue<AdapterPowerState> empty_queue;
  std::swap(local_power_changes_, empty_queue);

  if (last_change == current_change)
    return;

  local_power_changes_.push(last_change);

  SendBluetoothPoweredStateBroadcast(last_change);
  power_intent_timer_.Start(
      FROM_HERE, kPowerIntentTimeout,
      base::BindOnce(&ArcBluetoothBridge::DequeueLocalPowerChange,
                     weak_factory_.GetWeakPtr(), last_change));
}

void ArcBluetoothBridge::EnqueueRemotePowerChange(
    ArcBluetoothBridge::AdapterPowerState powered,
    ArcBluetoothBridge::AdapterStateCallback callback) {
  remote_power_changes_.push(powered);

  bool turn_on = (powered == AdapterPowerState::TURN_ON);
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  BLUETOOTH_LOG(EVENT) << "ARC bluetooth set power: " << turn_on;
  bluetooth_adapter_->SetPowered(
      turn_on,
      base::BindOnce(turn_on ? &ArcBluetoothBridge::OnPoweredOn
                             : &ArcBluetoothBridge::OnPoweredOff,
                     weak_factory_.GetWeakPtr(),
                     std::move(split_callback.first),
                     /*save_user_pref=*/true),
      base::BindOnce(&ArcBluetoothBridge::OnPoweredError,
                     weak_factory_.GetWeakPtr(),
                     std::move(split_callback.second)));
}

void ArcBluetoothBridge::DequeueRemotePowerChange(
    ArcBluetoothBridge::AdapterPowerState powered) {
  remote_power_changes_.pop();
}

void ArcBluetoothBridge::SendCachedDevices() const {
  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnDevicePropertiesChanged);
  if (!bluetooth_instance)
    return;

  for (const auto* device : bluetooth_adapter_->GetDevices()) {
    // Since a cached device may not be a currently available device, we use
    // OnDevicePropertiesChanged() instead of OnDeviceFound() to avoid trigger
    // the logic of device found in Android.
    bluetooth_instance->OnDevicePropertiesChanged(
        mojom::BluetoothAddress::From(device->GetAddress()),
        GetDeviceProperties(mojom::BluetoothPropertyType::ALL, device));
  }
}

std::vector<mojom::BluetoothPropertyPtr>
ArcBluetoothBridge::GetDeviceProperties(mojom::BluetoothPropertyType type,
                                        const BluetoothDevice* device) const {
  std::vector<mojom::BluetoothPropertyPtr> properties;

  if (!device) {
    return properties;
  }

  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::BDNAME) {
    properties.push_back(mojom::BluetoothProperty::NewBdname(
        device->GetName() ? device->GetName().value() : ""));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::BDADDR) {
    properties.push_back(mojom::BluetoothProperty::NewBdaddr(
        mojom::BluetoothAddress::From(device->GetAddress())));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::UUIDS) {
    BluetoothDevice::UUIDSet uuids = device->GetUUIDs();
    if (uuids.size() > 0) {
      properties.push_back(mojom::BluetoothProperty::NewUuids(
          std::vector<BluetoothUUID>(uuids.begin(), uuids.end())));
    }
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::CLASS_OF_DEVICE) {
    properties.push_back(
        mojom::BluetoothProperty::NewDeviceClass(device->GetBluetoothClass()));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::TYPE_OF_DEVICE) {
    properties.push_back(
        mojom::BluetoothProperty::NewDeviceType(device->GetType()));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::REMOTE_FRIENDLY_NAME) {
    properties.push_back(mojom::BluetoothProperty::NewRemoteFriendlyName(
        base::UTF16ToUTF8(device->GetNameForDisplay())));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::REMOTE_RSSI) {
    std::optional<int8_t> rssi = device->GetInquiryRSSI();
    if (rssi.has_value()) {
      properties.push_back(
          mojom::BluetoothProperty::NewRemoteRssi(rssi.value()));
    }
  }
  // TODO(smbarber): Add remote version info

  return properties;
}

std::vector<mojom::BluetoothPropertyPtr>
ArcBluetoothBridge::GetAdapterProperties(
    mojom::BluetoothPropertyType type) const {
  // TODO(crbug.com/40189401): Since this function is invoked from ARC side, it
  // is possible that adapter is not present or powered here. It's not
  // meaningful to return any property when that happens.
  const std::string adapter_address = bluetooth_adapter_->GetAddress();
  if (adapter_address.empty()) {
    LOG(ERROR) << "Bluetooth adapter does not have a valid address";
    return {};
  }

  std::vector<mojom::BluetoothPropertyPtr> properties;

  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::BDNAME) {
    properties.push_back(
        mojom::BluetoothProperty::NewBdname(bluetooth_adapter_->GetName()));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::BDADDR) {
    properties.push_back(mojom::BluetoothProperty::NewBdaddr(
        mojom::BluetoothAddress::From(adapter_address)));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::UUIDS) {
    properties.push_back(
        mojom::BluetoothProperty::NewUuids(bluetooth_adapter_->GetUUIDs()));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::CLASS_OF_DEVICE) {
    properties.push_back(
        mojom::BluetoothProperty::NewDeviceClass(kBluetoothComputerClass));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::TYPE_OF_DEVICE) {
    // Assume that all ChromeOS devices are dual mode Bluetooth device.
    properties.push_back(mojom::BluetoothProperty::NewDeviceType(
        device::BLUETOOTH_TRANSPORT_DUAL));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::ADAPTER_SCAN_MODE) {
    mojom::BluetoothScanMode scan_mode = mojom::BluetoothScanMode::CONNECTABLE;

    if (bluetooth_adapter_->IsDiscoverable())
      scan_mode = mojom::BluetoothScanMode::CONNECTABLE_DISCOVERABLE;

    properties.push_back(
        mojom::BluetoothProperty::NewAdapterScanMode(scan_mode));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::ADAPTER_BONDED_DEVICES) {
    BluetoothAdapter::DeviceList devices = bluetooth_adapter_->GetDevices();

    std::vector<mojom::BluetoothAddressPtr> bonded_devices;

    for (auto* device : devices) {
      if (!device->IsPaired())
        continue;

      mojom::BluetoothAddressPtr addr =
          mojom::BluetoothAddress::From(device->GetAddress());
      bonded_devices.push_back(std::move(addr));
    }

    properties.push_back(
        mojom::BluetoothProperty::NewBondedDevices(std::move(bonded_devices)));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::ADAPTER_DISCOVERY_TIMEOUT) {
    properties.push_back(
        mojom::BluetoothProperty::NewDiscoveryTimeout(static_cast<uint32_t>(
            bluetooth_adapter_->GetDiscoverableTimeout().InSeconds())));
  }
  if (type == mojom::BluetoothPropertyType::ALL ||
      type == mojom::BluetoothPropertyType::LOCAL_LE_FEATURES) {
    // TODO(crbug.com/40480399) Investigate all the le_features.
    mojom::BluetoothLocalLEFeaturesPtr le_features =
        mojom::BluetoothLocalLEFeatures::New();
    le_features->version_supported = kAndroidMBluetoothVersionNumber;
    le_features->local_privacy_enabled = 0;
    le_features->max_adv_instance = kMaxAdvertisements;
    le_features->rpa_offload_supported = 0;
    le_features->max_irk_list_size = 0;
    le_features->max_adv_filter_supported = 0;
    le_features->activity_energy_info_supported = 0;
    le_features->scan_result_storage_size = 0;
    le_features->total_trackable_advertisers = 0;
    le_features->extended_scan_support = false;
    le_features->debug_logging_supported = false;
    properties.push_back(
        mojom::BluetoothProperty::NewLocalLeFeatures(std::move(le_features)));
  }

  return properties;
}

// Android support 6 types of Advertising Data which are Advertising Data Flags,
// Local Name, Service UUIDs, Tx Power Level, Service Data, and Manufacturer
// Data. Note that we need to use 16-bit UUID in Service Data section because
// Android does not support 128-bit UUID there.
std::vector<mojom::BluetoothAdvertisingDataPtr>
ArcBluetoothBridge::GetAdvertisingData(const BluetoothDevice* device) const {
  std::vector<mojom::BluetoothAdvertisingDataPtr> advertising_data;

  // Advertising Data Flags
  if (device->GetAdvertisingDataFlags().has_value()) {
    advertising_data.push_back(mojom::BluetoothAdvertisingData::NewFlags(
        device->GetAdvertisingDataFlags().value()));
  }

  // Local Name
  advertising_data.push_back(mojom::BluetoothAdvertisingData::NewLocalName(
      device->GetName() ? device->GetName().value() : ""));

  // Service UUIDs
  BluetoothDevice::UUIDSet uuid_set = device->GetUUIDs();
  for (const BluetoothRemoteGattService* gatt_service :
       device->GetGattServices()) {
    uuid_set.erase(gatt_service->GetUUID());
  }
  if (uuid_set.size() > 0) {
    advertising_data.push_back(mojom::BluetoothAdvertisingData::NewServiceUuids(
        std::vector<BluetoothUUID>(uuid_set.begin(), uuid_set.end())));
  }

  // Tx Power Level
  if (device->GetInquiryTxPower().has_value()) {
    advertising_data.push_back(mojom::BluetoothAdvertisingData::NewTxPowerLevel(
        device->GetInquiryTxPower().value()));
  }

  // Service Data
  for (const BluetoothUUID& uuid : device->GetServiceDataUUIDs()) {
    std::optional<uint16_t> uuid16 = GetUUID16(uuid);
    if (!uuid16)
      continue;

    mojom::BluetoothServiceDataPtr service_data =
        mojom::BluetoothServiceData::New();

    // Android only supports UUID 16 bit here.
    service_data->uuid_16bit = *uuid16;

    const std::vector<uint8_t>* data = device->GetServiceDataForUUID(uuid);
    DCHECK(data != nullptr);

    service_data->data = *data;

    advertising_data.push_back(mojom::BluetoothAdvertisingData::NewServiceData(
        std::move(service_data)));
  }

  // Manufacturer Data
  if (!device->GetManufacturerData().empty()) {
    std::vector<uint8_t> manufacturer_data;
    for (const auto& pair : device->GetManufacturerData()) {
      uint16_t id = pair.first;
      // Use little endian here.
      manufacturer_data.push_back(id & 0xff);
      manufacturer_data.push_back(id >> 8);
      manufacturer_data.insert(manufacturer_data.end(), pair.second.begin(),
                               pair.second.end());
    }
    advertising_data.push_back(
        mojom::BluetoothAdvertisingData::NewManufacturerData(
            std::move(manufacturer_data)));
  }

  return advertising_data;
}

void ArcBluetoothBridge::SetPrimaryUserBluetoothPowerSetting(
    bool enabled) const {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
  DCHECK(profile);
  profile->GetPrefs()->SetBoolean(ash::prefs::kUserBluetoothAdapterEnabled,
                                  enabled);
}

ArcBluetoothBridge::GattConnection::GattConnection(
    ArcBluetoothBridge::GattConnection::ConnectionState state,
    std::unique_ptr<device::BluetoothGattConnection> connection,
    bool need_hard_disconnect)
    : state(state),
      connection(std::move(connection)),
      need_hard_disconnect(need_hard_disconnect) {}
ArcBluetoothBridge::GattConnection::GattConnection()
    : connection(nullptr), need_hard_disconnect(false) {}
ArcBluetoothBridge::GattConnection::~GattConnection() = default;
ArcBluetoothBridge::GattConnection::GattConnection(
    ArcBluetoothBridge::GattConnection&&) = default;
ArcBluetoothBridge::GattConnection&
ArcBluetoothBridge::GattConnection::operator=(
    ArcBluetoothBridge::GattConnection&&) = default;

namespace {

bool IsValidPort(mojom::BluetoothSocketType sock_type, int port) {
  switch (sock_type) {
    case mojom::BluetoothSocketType::TYPE_RFCOMM:
      return port <= ArcBluetoothBridge::kMaxRfcommChannel &&
             port >= ArcBluetoothBridge::kMinRfcommChannel;
    case mojom::BluetoothSocketType::TYPE_L2CAP_LE:
      return port <= ArcBluetoothBridge::kMaxL2capLePsm &&
             port >= ArcBluetoothBridge::kMinL2capLePsm;
  }
}

}  // namespace

void ArcBluetoothBridge::BluetoothSocketListen(
    mojom::BluetoothSocketType sock_type,
    mojom::BluetoothSocketFlagsPtr sock_flags,
    int32_t port,
    BluetoothSocketListenCallback callback) {
  if (!mojom::IsKnownEnumValue(sock_type)) {
    LOG(ERROR) << "Unsupported sock type " << sock_type;
    std::move(callback).Run(
        mojom::BluetoothStatus::UNSUPPORTED, /*port=*/0,
        mojo::PendingReceiver<mojom::BluetoothListenSocketClient>());
    return;
  }

  if (port != kAutoSockPort && !IsValidPort(sock_type, port)) {
    LOG(ERROR) << "Invalid port number " << port;
    std::move(callback).Run(
        mojom::BluetoothStatus::FAIL, /*port=*/0,
        mojo::PendingReceiver<mojom::BluetoothListenSocketClient>());
    return;
  }

  CreateBluetoothListenSocket(sock_type, std::move(sock_flags), port,
                              std::move(callback));
}

void ArcBluetoothBridge::BluetoothSocketConnect(
    mojom::BluetoothSocketType sock_type,
    mojom::BluetoothSocketFlagsPtr sock_flags,
    mojom::BluetoothAddressPtr remote_addr,
    int32_t port,
    BluetoothSocketConnectCallback callback) {
  if (!mojom::IsKnownEnumValue(sock_type)) {
    LOG(ERROR) << "Unsupported sock type " << sock_type;
    std::move(callback).Run(
        mojom::BluetoothStatus::UNSUPPORTED,
        mojo::PendingReceiver<arc::mojom::BluetoothConnectSocketClient>());
    return;
  }

  if (!IsValidPort(sock_type, port)) {
    LOG(ERROR) << "Invalid port number " << port;
    std::move(callback).Run(
        mojom::BluetoothStatus::FAIL,
        mojo::PendingReceiver<arc::mojom::BluetoothConnectSocketClient>());
    return;
  }

  CreateBluetoothConnectSocket(sock_type, std::move(sock_flags),
                               std::move(remote_addr), port,
                               std::move(callback));
}

// static
void ArcBluetoothBridge::EnsureFactoryBuilt() {
  ArcBluezBridgeFactory::GetInstance();
  ArcFlossBridgeFactory::GetInstance();
}

ArcBluetoothBridge::BluetoothListeningSocket::BluetoothListeningSocket() =
    default;
ArcBluetoothBridge::BluetoothListeningSocket::~BluetoothListeningSocket() =
    default;
ArcBluetoothBridge::BluetoothConnectingSocket::BluetoothConnectingSocket() =
    default;
ArcBluetoothBridge::BluetoothConnectingSocket::~BluetoothConnectingSocket() =
    default;

ArcBluetoothBridge::BluetoothArcConnectionObserver::
    BluetoothArcConnectionObserver(ArcBluetoothBridge* arc_bluetooth_bridge)
    : arc_bluetooth_bridge_(arc_bluetooth_bridge) {
  arc_bluetooth_bridge_->arc_bridge_service_->bluetooth()->AddObserver(this);
}

ArcBluetoothBridge::BluetoothArcConnectionObserver::
    ~BluetoothArcConnectionObserver() {
  arc_bluetooth_bridge_->arc_bridge_service_->bluetooth()->RemoveObserver(this);
}

void ArcBluetoothBridge::BluetoothArcConnectionObserver::OnConnectionClosed() {
  // Stops the ongoing discovery sessions.
  arc_bluetooth_bridge_->CancelDiscovery();
  arc_bluetooth_bridge_->StopLEScan();

  // Cleanup for CreateBond().
  for (const auto& addr : arc_bluetooth_bridge_->devices_paired_by_arc_) {
    BluetoothDevice* device =
        arc_bluetooth_bridge_->bluetooth_adapter_->GetDevice(addr);
    if (!device)
      continue;
    if (device->IsPaired()) {
      device->Disconnect(base::DoNothing(), base::DoNothing());
    } else {
      device->CancelPairing();
    }
  }
  arc_bluetooth_bridge_->devices_paired_by_arc_.clear();
  arc_bluetooth_bridge_->devices_pairing_.clear();

  // Cleanup for GATT connections.
  // TODO(b/151573141): Remove the following loops when Chrome can perform hard
  // disconnect on a paired device correctly.
  for (const auto& addr_conn : arc_bluetooth_bridge_->gatt_connections_) {
    BluetoothDevice* device =
        arc_bluetooth_bridge_->bluetooth_adapter_->GetDevice(addr_conn.first);
    if (!device || !device->IsPaired() || !device->IsConnected())
      continue;
    device->Disconnect(base::DoNothing(), base::DoNothing());
  }
  arc_bluetooth_bridge_->gatt_connections_.clear();
}

}  // namespace arc

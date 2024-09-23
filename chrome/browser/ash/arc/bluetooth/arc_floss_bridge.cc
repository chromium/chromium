// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/rfcomm.h>

#include "ash/components/arc/bluetooth/bluetooth_type_converters.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/arc/bluetooth/arc_floss_bridge.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_socket_manager.h"

#include "base/logging.h"

#include "ash/components/arc/bluetooth/bluetooth_type_converters.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "device/bluetooth/floss/bluetooth_device_floss.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_sdp_types.h"

using device::BluetoothUUID;
using floss::BluetoothDeviceFloss;

namespace arc {

ArcFlossBridge::ArcFlossBridge(content::BrowserContext* context,
                               ArcBridgeService* bridge_service)
    : ArcBluetoothBridge(context, bridge_service) {}

ArcFlossBridge::~ArcFlossBridge() {
  if (GetAdapter() && GetAdapter()->IsPowered() &&
      floss::FlossDBusManager::Get()->GetAdapterClient()->HasObserver(this)) {
    floss::FlossDBusManager::Get()->GetAdapterClient()->RemoveObserver(this);
  }
}

floss::BluetoothAdapterFloss* ArcFlossBridge::GetAdapter() const {
  return static_cast<floss::BluetoothAdapterFloss*>(bluetooth_adapter_.get());
}

void ArcFlossBridge::HandlePoweredOn() {
  if (!floss::FlossDBusManager::Get()->GetAdapterClient()->HasObserver(this)) {
    floss::FlossDBusManager::Get()->GetAdapterClient()->AddObserver(this);
  }
}

void ArcFlossBridge::OnSdpSearchResult(mojom::BluetoothAddressPtr remote_addr,
                                       const device::BluetoothUUID& target_uuid,
                                       floss::DBusResult<bool> result) {
  if (!result.has_value()) {
    OnGetServiceRecordsError(
        std::move(remote_addr), target_uuid,
        bluez::BluetoothServiceRecordBlueZ::ErrorCode::UNKNOWN);
    return;
  }

  if (!*result) {
    OnGetServiceRecordsError(
        std::move(remote_addr), target_uuid,
        bluez::BluetoothServiceRecordBlueZ::ErrorCode::UNKNOWN);
    return;
  }
}

void ArcFlossBridge::GetSdpRecords(mojom::BluetoothAddressPtr remote_addr,
                                   const BluetoothUUID& target_uuid) {
  if (!AdapterReadyAndRegistered()) {
    OnGetServiceRecordsError(
        std::move(remote_addr), target_uuid,
        bluez::BluetoothServiceRecordBlueZ::ErrorCode::ERROR_ADAPTER_NOT_READY);
    return;
  }

  const floss::FlossDeviceId remote_device = floss::FlossDeviceId(
      {.address = remote_addr->To<std::string>(), .name = ""});
  floss::ResponseCallback<bool> response_callback = base::BindOnce(
      &ArcFlossBridge::OnSdpSearchResult, weak_factory_.GetWeakPtr(),
      std::move(remote_addr), target_uuid);

  floss::FlossDBusManager::Get()->GetAdapterClient()->SdpSearch(
      std::move(response_callback), remote_device, target_uuid);
}

void ArcFlossBridge::CreateSdpRecordComplete(device::BluetoothUUID uuid,
                                             floss::DBusResult<bool> result) {
  if (!result.has_value()) {
    arc::mojom::BluetoothCreateSdpRecordResultPtr callback_result =
        arc::mojom::BluetoothCreateSdpRecordResult::New();
    callback_result->status = mojom::BluetoothStatus::FAIL;
    CompleteCreateSdpRecord(uuid, std::move(callback_result));
    return;
  }

  if (!*result) {
    arc::mojom::BluetoothCreateSdpRecordResultPtr callback_result =
        arc::mojom::BluetoothCreateSdpRecordResult::New();
    callback_result->status = mojom::BluetoothStatus::FAIL;
    CompleteCreateSdpRecord(uuid, std::move(callback_result));
    return;
  }
}

void ArcFlossBridge::CreateSdpRecord(mojom::BluetoothSdpRecordPtr record_mojo,
                                     CreateSdpRecordCallback callback) {
  if (!AdapterReadyAndRegistered()) {
    arc::mojom::BluetoothCreateSdpRecordResultPtr result =
        arc::mojom::BluetoothCreateSdpRecordResult::New();
    result->status = mojom::BluetoothStatus::NOT_READY;
    std::move(callback).Run(std::move(result));
    return;
  }
  const floss::BtSdpRecord sdp_record =
      mojo::TypeConverter<floss::BtSdpRecord,
                          bluez::BluetoothServiceRecordBlueZ>::
          Convert(mojo::TypeConverter<
                  bluez::BluetoothServiceRecordBlueZ,
                  mojom::BluetoothSdpRecordPtr>::Convert(record_mojo));
  const std::optional<device::BluetoothUUID> uuid =
      floss::GetUUIDFromSdpRecord(sdp_record);
  if (!uuid.has_value()) {
    arc::mojom::BluetoothCreateSdpRecordResultPtr result =
        arc::mojom::BluetoothCreateSdpRecordResult::New();
    result->status = mojom::BluetoothStatus::PARM_INVALID;
    std::move(callback).Run(std::move(result));
    return;
  }
  create_sdp_record_callbacks_.insert_or_assign(*uuid, std::move(callback));

  floss::ResponseCallback<bool> response_callback =
      base::BindOnce(&ArcFlossBridge::CreateSdpRecordComplete,
                     weak_factory_.GetWeakPtr(), *uuid);
  floss::FlossDBusManager::Get()->GetAdapterClient()->CreateSdpRecord(
      std::move(response_callback), sdp_record);
}

void ArcFlossBridge::RemoveSdpRecordComplete(RemoveSdpRecordCallback callback,
                                             floss::DBusResult<bool> result) {
  if (!result.has_value()) {
    std::move(callback).Run(arc::mojom::BluetoothStatus::FAIL);
    return;
  }

  if (!*result) {
    std::move(callback).Run(arc::mojom::BluetoothStatus::FAIL);
    return;
  }

  std::move(callback).Run(arc::mojom::BluetoothStatus::SUCCESS);
}

void ArcFlossBridge::RemoveSdpRecord(uint32_t service_handle,
                                     RemoveSdpRecordCallback callback) {
  if (!AdapterReadyAndRegistered()) {
    std::move(callback).Run(arc::mojom::BluetoothStatus::NOT_READY);
    return;
  }

  floss::ResponseCallback<bool> response_callback =
      base::BindOnce(&ArcFlossBridge::RemoveSdpRecordComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  floss::FlossDBusManager::Get()->GetAdapterClient()->RemoveSdpRecord(
      std::move(response_callback), service_handle);
}

namespace {

void OnNoOpBtifResult(
    floss::DBusResult<floss::FlossDBusClient::BtifStatus> result) {}

floss::FlossSocketManager::Security GetSecureFromFlags(
    const mojom::BluetoothSocketFlagsPtr flags) {
  // From Floss's masking logic secure = encrypt+auth
  return (flags->encrypt && flags->auth)
             ? floss::FlossSocketManager::Security::kSecure
             : floss::FlossSocketManager::Security::kInsecure;
}

}  // namespace

void ArcFlossBridge::OnCloseBluetoothListeningSocketComplete(
    BluetoothListeningSocket* socket,
    floss::DBusResult<floss::FlossDBusClient::BtifStatus> result) {
  // We are not going to keep the socket around regardless of whether Floss
  // succeeded, but we give it a chance.
  listening_sockets_.erase(socket);
}

void ArcFlossBridge::CloseBluetoothListeningSocket(
    BluetoothListeningSocket* ptr) {
  if (!listening_sockets_.contains(ptr)) {
    return;
  }

  floss::ResponseCallback<floss::FlossAdapterClient::BtifStatus>
      response_callback = base::BindOnce(
          &ArcFlossBridge::OnCloseBluetoothListeningSocketComplete,
          weak_factory_.GetWeakPtr(), ptr);
  floss::FlossDBusManager::Get()->GetSocketManager()->Close(
      listening_sockets_[ptr].second, std::move(response_callback));
}

void ArcFlossBridge::CloseBluetoothConnectingSocket(
    BluetoothConnectingSocket* ptr) {
  auto found = connecting_sockets_.find(ptr);
  connecting_sockets_.erase(found);
}

void ArcFlossBridge::SdpSearchComplete(
    const floss::FlossDeviceId device,
    const device::BluetoothUUID uuid,
    const std::vector<floss::BtSdpRecord> records) {
  mojom::BluetoothAddressPtr address =
      mojom::BluetoothAddress::From(device.address);
  std::vector<bluez::BluetoothServiceRecordBlueZ> records_bluez;
  for (auto record : records) {
    records_bluez.push_back(
        mojo::TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                            floss::BtSdpRecord>::Convert(record));
    std::optional<floss::BtSdpHeaderOverlay> header =
        GetHeaderOverlayFromSdpRecord(record);
    if (!header.has_value()) {
      continue;
    }
    // This record may be for an L2CAP service, but callers have to specify what
    // protocol they want to use anyway.
    uuid_lookups_.insert_or_assign(
        std::make_pair(device.address, header->rfcomm_channel_number),
        header->uuid);
  }
  OnGetServiceRecordsFinished(std::move(address), uuid, records_bluez);
}

void ArcFlossBridge::SdpRecordCreated(const floss::BtSdpRecord record,
                                      const int32_t handle) {
  const std::optional<device::BluetoothUUID> uuid =
      floss::GetUUIDFromSdpRecord(record);
  if (!uuid.has_value()) {
    return;
  }
  arc::mojom::BluetoothCreateSdpRecordResultPtr callback_result =
      arc::mojom::BluetoothCreateSdpRecordResult::New();
  callback_result->status = mojom::BluetoothStatus::SUCCESS;
  callback_result->service_handle = handle;
  CompleteCreateSdpRecord(*uuid, std::move(callback_result));
}

void ArcFlossBridge::SendCachedDevices() const {
  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnDevicePropertiesChanged);
  if (!bluetooth_instance) {
    return;
  }

  for (const auto* device : bluetooth_adapter_->GetDevices()) {
    const BluetoothDeviceFloss* floss_device =
        static_cast<const BluetoothDeviceFloss*>(device);
    if (!floss_device->HasReadProperties()) {
      VLOG(1) << "Skipping device that hasn't read properties: "
              << floss_device->GetAddress();
      continue;
    }

    // Since a cached device may not be a currently available device, we use
    // OnDevicePropertiesChanged() instead of OnDeviceFound() to avoid trigger
    // the logic of device found in Android.
    bluetooth_instance->OnDevicePropertiesChanged(
        mojom::BluetoothAddress::From(device->GetAddress()),
        GetDeviceProperties(mojom::BluetoothPropertyType::ALL, device));
  }
}

void ArcFlossBridge::StartLEScanImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!bluetooth_adapter_) {
    LOG(DFATAL) << "Bluetooth adapter does not exist.";
    return;
  }

  if (ble_scan_session_) {
    LOG(ERROR) << "LE scan already running.";
    StartLEScanOffTimer();
    scanned_devices_.clear();
    discovery_queue_.Pop();
    return;
  }

  ble_scan_session_ = bluetooth_adapter_->StartLowEnergyScanSession(
      nullptr, weak_factory_.GetWeakPtr());
}

void ArcFlossBridge::ResetLEScanSession() {
  if (ble_scan_session_) {
    ble_scan_session_.reset();
  }
}

bool ArcFlossBridge::IsDiscoveringOrScanning() {
  return discovery_session_ || ble_scan_session_;
}

void ArcFlossBridge::CreateBluetoothListenSocket(
    mojom::BluetoothSocketType type,
    mojom::BluetoothSocketFlagsPtr flags,
    int port,
    ArcFlossBridge::BluetoothSocketListenCallback callback) {
  if (!AdapterReadyAndRegistered()) {
    return;
  }
  if (type != mojom::BluetoothSocketType::TYPE_RFCOMM &&
      type != mojom::BluetoothSocketType::TYPE_L2CAP_LE) {
    std::move(callback).Run(
        mojom::BluetoothStatus::FAIL, /*port=*/0,
        mojo::PendingReceiver<mojom::BluetoothListenSocketClient>());
    return;
  }
  auto sock_wrapper = std::make_unique<BluetoothListeningSocket>();
  sock_wrapper->sock_type = type;
  auto connection_accepted_callback =
      base::BindRepeating(&ArcFlossBridge::OnConnectionAccepted,
                          weak_factory_.GetWeakPtr(), sock_wrapper.get());
  int socket_ready_callback_id = next_socket_ready_callback_id_++;
  socket_ready_callbacks_.insert_or_assign(socket_ready_callback_id,
                                           std::move(callback));
  auto connection_state_changed_callback = base::BindRepeating(
      &ArcFlossBridge::OnConnectionStateChanged, weak_factory_.GetWeakPtr(),
      // TODO(crbug.com/40061562): Remove `UnsafeDanglingUntriaged`
      base::UnsafeDanglingUntriaged(sock_wrapper.get()),
      socket_ready_callback_id);
  floss::ResponseCallback<floss::FlossDBusClient::BtifStatus>
      response_callback =
          base::BindOnce(&ArcFlossBridge::OnCreateListenSocketCallback,
                         weak_factory_.GetWeakPtr(), std::move(sock_wrapper),
                         socket_ready_callback_id);
  switch (type) {
    case mojom::BluetoothSocketType::TYPE_RFCOMM: {
      floss::FlossDBusManager::Get()->GetSocketManager()->ListenUsingRfcommAlt(
          std::nullopt, std::nullopt, port,
          floss::FlossSocketManager::GetRawFlossFlagsFromBluetoothFlags(
              flags->encrypt, flags->auth, flags->auth_mitm,
              flags->auth_16_digit, /*no_sdp=*/true),
          std::move(response_callback),
          std::move(connection_state_changed_callback),
          std::move(connection_accepted_callback));
      break;
    }
    case mojom::BluetoothSocketType::TYPE_L2CAP_LE: {
      floss::FlossDBusManager::Get()->GetSocketManager()->ListenUsingL2capLe(
          GetSecureFromFlags(std::move(flags)), std::move(response_callback),
          std::move(connection_state_changed_callback),
          std::move(connection_accepted_callback));
      break;
    }
    default: {
      return;
    }
  }
}

void ArcFlossBridge::CreateBluetoothConnectSocket(
    mojom::BluetoothSocketType type,
    mojom::BluetoothSocketFlagsPtr flags,
    mojom::BluetoothAddressPtr addr,
    int port,
    ArcFlossBridge::BluetoothSocketConnectCallback callback) {
  if (!AdapterReadyAndRegistered()) {
    return;
  }
  const floss::FlossDeviceId remote_device =
      floss::FlossDeviceId({.address = addr->To<std::string>(), .name = ""});
  auto sock_wrapper = std::make_unique<BluetoothConnectingSocket>();
  sock_wrapper->sock_type = type;
  switch (type) {
    case mojom::BluetoothSocketType::TYPE_RFCOMM: {
      const std::pair<std::string, int32_t> uuid_lookup_key =
          std::make_pair(remote_device.address, port);
      if (!uuid_lookups_.contains(uuid_lookup_key)) {
        std::move(callback).Run(
            mojom::BluetoothStatus::FAIL,
            mojo::PendingReceiver<mojom::BluetoothConnectSocketClient>());
        return;
      }
      floss::FlossDBusManager::Get()->GetSocketManager()->ConnectUsingRfcomm(
          remote_device, uuid_lookups_.at(uuid_lookup_key),
          GetSecureFromFlags(std::move(flags)),
          base::BindOnce(&ArcFlossBridge::OnCreateConnectSocketCallback,
                         weak_factory_.GetWeakPtr(), std::move(sock_wrapper),
                         std::move(callback)));
      break;
    }
    case mojom::BluetoothSocketType::TYPE_L2CAP_LE: {
      floss::FlossDBusManager::Get()->GetSocketManager()->ConnectUsingL2capLe(
          remote_device, port, GetSecureFromFlags(std::move(flags)),
          base::BindOnce(&ArcFlossBridge::OnCreateConnectSocketCallback,
                         weak_factory_.GetWeakPtr(), std::move(sock_wrapper),
                         std::move(callback)));
      break;
    }
    default: {
      std::move(callback).Run(
          mojom::BluetoothStatus::FAIL,
          mojo::PendingReceiver<mojom::BluetoothConnectSocketClient>());
      return;
    }
  }
}

void ArcFlossBridge::OnCreateListenSocketCallback(
    std::unique_ptr<ArcBluetoothBridge::BluetoothListeningSocket> sock_wrapper,
    int socket_ready_callback_id,
    floss::DBusResult<floss::FlossDBusClient::BtifStatus> result) {
  ArcBluetoothBridge::BluetoothListeningSocket* sock_wrapper_to_pass =
      sock_wrapper.get();
  listening_sockets_.insert_or_assign(
      sock_wrapper.get(),
      std::make_pair(std::move(sock_wrapper), empty_socket_id_));
  if (!result.has_value()) {
    CompleteListenSocketReady(socket_ready_callback_id,
                              mojom::BluetoothStatus::FAIL,
                              sock_wrapper_to_pass);
    return;
  }

  if (*result != floss::FlossDBusClient::BtifStatus::kSuccess) {
    CompleteListenSocketReady(socket_ready_callback_id,
                              mojom::BluetoothStatus::FAIL,
                              sock_wrapper_to_pass);
    return;
  }
}

void ArcFlossBridge::OnCreateConnectSocketCallback(
    std::unique_ptr<ArcBluetoothBridge::BluetoothConnectingSocket> sock_wrapper,
    ArcFlossBridge::BluetoothSocketConnectCallback callback,
    floss::FlossDBusClient::BtifStatus status,
    std::optional<floss::FlossSocketManager::FlossSocket>&& socket) {
  if (status != floss::FlossDBusClient::BtifStatus::kSuccess) {
    std::move(callback).Run(
        mojom::BluetoothStatus::FAIL,
        mojo::PendingReceiver<mojom::BluetoothConnectSocketClient>());
    return;
  }

  if (!socket || !socket->fd) {
    std::move(callback).Run(
        mojom::BluetoothStatus::FAIL,
        mojo::PendingReceiver<mojom::BluetoothConnectSocketClient>());
    return;
  }

  sock_wrapper->file = std::move(socket->fd.value());
  std::move(callback).Run(mojom::BluetoothStatus::SUCCESS,
                          sock_wrapper->remote.BindNewPipeAndPassReceiver());
  auto connection = mojom::BluetoothSocketConnection::New();
  mojo::ScopedHandle handle = mojo::WrapPlatformHandle(
      mojo::PlatformHandle(std::move(sock_wrapper->file)));
  connection->sock = std::move(handle);
  switch (sock_wrapper->sock_type) {
    case mojom::BluetoothSocketType::TYPE_RFCOMM:
    case mojom::BluetoothSocketType::TYPE_L2CAP_LE:
      connection->addr = mojom::BluetoothAddress::From<std::string>(
          socket->remote_device.address);
      connection->port = socket->port;
      break;
    default:
      LOG(ERROR) << "Unknown socket type " << sock_wrapper->sock_type;
      return;
  }
  sock_wrapper->remote->OnConnected(std::move(connection));
  connecting_sockets_.insert(std::move(sock_wrapper));
}

void ArcFlossBridge::OnConnectionStateChanged(
    ArcBluetoothBridge::BluetoothListeningSocket* sock_wrapper,
    int socket_ready_callback_id,
    floss::FlossSocketManager::ServerSocketState state,
    floss::FlossSocketManager::FlossListeningSocket socket,
    floss::FlossSocketManager::BtifStatus status) {
  if (!sock_wrapper) {
    // sock_wrapper has been disposed (or worse), nothing to do, but resolve
    // callback with failure if it is still around.
    CompleteListenSocketReady(socket_ready_callback_id,
                              mojom::BluetoothStatus::FAIL, sock_wrapper,
                              socket);
    return;
  }

  if (status != floss::FlossSocketManager::BtifStatus::kSuccess) {
    LOG(ERROR) << "Received OnConnectionStateChanged callback with "
                  "non-Success status: "
               << static_cast<int>(status);
    CompleteListenSocketReady(socket_ready_callback_id,
                              mojom::BluetoothStatus::FAIL, sock_wrapper,
                              socket);
    return;
  }

  if (state != floss::FlossSocketManager::ServerSocketState::kReady) {
    // Socket isn't ready for accepting connections, nothing to do
    return;
  }

  // At this point we may assume that listening socket creation was a
  // success. Let CompleteListenSocketReady know if it doesn't already.
  CompleteListenSocketReady(socket_ready_callback_id,
                            mojom::BluetoothStatus::SUCCESS, sock_wrapper,
                            socket);
  if (!AdapterReadyAndRegistered()) {
    return;
  }

  floss::ResponseCallback<floss::FlossDBusClient::BtifStatus>
      response_callback = base::BindOnce(&OnNoOpBtifResult);
  // TODO: figure out the correct timeout here
  floss::FlossDBusManager::Get()->GetSocketManager()->Accept(
      socket.id, /*timeout_ms=*/std::nullopt, std::move(response_callback));
}

void ArcFlossBridge::OnConnectionAccepted(
    const ArcBluetoothBridge::BluetoothListeningSocket* sock_wrapper,
    floss::FlossSocketManager::FlossSocket&& socket) {
  if (!sock_wrapper) {
    // sock_wrapper has been disposed (or worse), nothing to do
    return;
  }

  if (!socket.is_valid() || !socket.fd.has_value()) {
    LOG(ERROR) << "New socket connection was accepted with invalid socket";
    return;
  }

  mojo::ScopedHandle handle =
      mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(*socket.fd)));
  auto connection = mojom::BluetoothSocketConnection::New();
  connection->sock = std::move(handle);
  connection->addr =
      mojom::BluetoothAddress::From(socket.remote_device.address);
  connection->port = socket.port;
  switch (socket.type) {
    case floss::FlossSocketManager::SocketType::kRfcomm:
    case floss::FlossSocketManager::SocketType::kL2cap:
      sock_wrapper->remote->OnAccepted(std::move(connection));
      break;
    default:
      return;
  }
}

int ArcFlossBridge::GetPortOrChannel(
    ArcBluetoothBridge::BluetoothListeningSocket* sock_wrapper,
    floss::FlossSocketManager::FlossListeningSocket socket) {
  switch (sock_wrapper->sock_type) {
    case mojom::BluetoothSocketType::TYPE_RFCOMM:
      if (!socket.channel) {
        return 0;
      }
      return *socket.channel;
    case mojom::BluetoothSocketType::TYPE_L2CAP_LE:
      if (!socket.psm) {
        return 0;
      }
      return *socket.psm;
    default:
      return 0;
  }
}

void ArcFlossBridge::CompleteListenSocketReady(
    int socket_ready_callback_id,
    mojom::BluetoothStatus status,
    ArcBluetoothBridge::BluetoothListeningSocket* sock_wrapper) {
  CompleteListenSocketReady(socket_ready_callback_id, status,
                            /*port_or_channel*/ 0, sock_wrapper);
}

void ArcFlossBridge::CompleteListenSocketReady(
    int socket_ready_callback_id,
    mojom::BluetoothStatus status,
    int port_or_channel,
    ArcBluetoothBridge::BluetoothListeningSocket* sock_wrapper) {
  if (!socket_ready_callbacks_.contains(socket_ready_callback_id)) {
    return;
  }

  BluetoothSocketListenCallback callback =
      std::move(socket_ready_callbacks_[socket_ready_callback_id]);
  socket_ready_callbacks_.erase(socket_ready_callback_id);
  if (status != mojom::BluetoothStatus::SUCCESS) {
    std::move(callback).Run(
        mojom::BluetoothStatus::FAIL, /*port=*/0,
        mojo::PendingReceiver<mojom::BluetoothListenSocketClient>());
  } else {
    std::move(callback).Run(mojom::BluetoothStatus::SUCCESS, port_or_channel,
                            sock_wrapper->remote.BindNewPipeAndPassReceiver());
    sock_wrapper->remote.set_disconnect_handler(
        base::BindOnce(&ArcFlossBridge::CloseBluetoothListeningSocket,
                       weak_factory_.GetWeakPtr(), sock_wrapper));
  }
}

void ArcFlossBridge::CompleteListenSocketReady(
    int socket_ready_callback_id,
    mojom::BluetoothStatus status,
    ArcBluetoothBridge::BluetoothListeningSocket* sock_wrapper,
    floss::FlossSocketManager::FlossListeningSocket socket) {
  if (!socket_ready_callbacks_.contains(socket_ready_callback_id)) {
    return;
  }
  if (!listening_sockets_.contains(sock_wrapper)) {
    return;
  }
  listening_sockets_[sock_wrapper].second = socket.id;
  CompleteListenSocketReady(socket_ready_callback_id, status,
                            GetPortOrChannel(sock_wrapper, socket),
                            sock_wrapper);
}

void ArcFlossBridge::CompleteCreateSdpRecord(
    device::BluetoothUUID uuid,
    arc::mojom::BluetoothCreateSdpRecordResultPtr result) {
  if (!base::Contains(create_sdp_record_callbacks_, uuid)) {
    return;
  }

  CreateSdpRecordCallback callback =
      std::move(create_sdp_record_callbacks_[uuid]);
  create_sdp_record_callbacks_.erase(uuid);
  std::move(callback).Run(std::move(result));
}

bool ArcFlossBridge::AdapterReadyAndRegistered() {
  if (!GetAdapter() || !GetAdapter()->IsPresent()) {
    return false;
  }

  if (!floss::FlossDBusManager::Get()->GetAdapterClient()->HasObserver(this)) {
    floss::FlossDBusManager::Get()->GetAdapterClient()->AddObserver(this);
  }

  return true;
}

}  // namespace arc

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/bluetooth/arc_bluez_bridge.h"

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/rfcomm.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/socket.h>

#include "ash/components/arc/bluetooth/bluetooth_type_converters.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/posix/eintr_wrapper.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"

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

// Bluetooth SDP Service Class ID List Attribute identifier
constexpr uint16_t kServiceClassIDListAttributeID = 0x0001;

void OnCreateServiceRecordDone(
    arc::ArcBluetoothBridge::CreateSdpRecordCallback callback,
    uint32_t service_handle) {
  arc::mojom::BluetoothCreateSdpRecordResultPtr result =
      arc::mojom::BluetoothCreateSdpRecordResult::New();
  result->status = arc::mojom::BluetoothStatus::SUCCESS;
  result->service_handle = service_handle;

  std::move(callback).Run(std::move(result));
}

void OnCreateServiceRecordError(
    arc::ArcBluetoothBridge::CreateSdpRecordCallback callback,
    bluez::BluetoothServiceRecordBlueZ::ErrorCode error_code) {
  arc::mojom::BluetoothCreateSdpRecordResultPtr result =
      arc::mojom::BluetoothCreateSdpRecordResult::New();
  if (error_code ==
      bluez::BluetoothServiceRecordBlueZ::ErrorCode::ERROR_ADAPTER_NOT_READY) {
    result->status = arc::mojom::BluetoothStatus::NOT_READY;
  } else {
    result->status = arc::mojom::BluetoothStatus::FAIL;
  }

  std::move(callback).Run(std::move(result));
}

void OnRemoveServiceRecordDone(
    arc::ArcBluetoothBridge::RemoveSdpRecordCallback callback) {
  std::move(callback).Run(arc::mojom::BluetoothStatus::SUCCESS);
}

void OnRemoveServiceRecordError(
    arc::ArcBluetoothBridge::RemoveSdpRecordCallback callback,
    bluez::BluetoothServiceRecordBlueZ::ErrorCode error_code) {
  arc::mojom::BluetoothStatus status;
  if (error_code ==
      bluez::BluetoothServiceRecordBlueZ::ErrorCode::ERROR_ADAPTER_NOT_READY) {
    status = arc::mojom::BluetoothStatus::NOT_READY;
  } else {
    status = arc::mojom::BluetoothStatus::FAIL;
  }

  std::move(callback).Run(status);
}

}  // namespace

namespace arc {

ArcBluezBridge::ArcBluezBridge(content::BrowserContext* context,
                               ArcBridgeService* bridge_service)
    : ArcBluetoothBridge(context, bridge_service) {}

ArcBluezBridge::~ArcBluezBridge() = default;

bluez::BluetoothAdapterBlueZ* ArcBluezBridge::GetAdapter() const {
  return static_cast<bluez::BluetoothAdapterBlueZ*>(bluetooth_adapter_.get());
}

void ArcBluezBridge::HandlePoweredOn() {}

void ArcBluezBridge::GetSdpRecords(mojom::BluetoothAddressPtr remote_addr,
                                   const BluetoothUUID& target_uuid) {
  BluetoothDevice* device =
      GetAdapter()->GetDevice(remote_addr->To<std::string>());
  if (!device) {
    OnGetServiceRecordsError(std::move(remote_addr), target_uuid,
                             bluez::BluetoothServiceRecordBlueZ::ErrorCode::
                                 ERROR_DEVICE_DISCONNECTED);
    return;
  }

  bluez::BluetoothDeviceBlueZ* device_bluez =
      static_cast<bluez::BluetoothDeviceBlueZ*>(device);

  mojom::BluetoothAddressPtr remote_addr_clone = remote_addr.Clone();

  device_bluez->GetServiceRecords(
      base::BindOnce(&ArcBluezBridge::OnGetServiceRecordsFinished,
                     weak_factory_.GetWeakPtr(), std::move(remote_addr),
                     target_uuid),
      base::BindOnce(&ArcBluezBridge::OnGetServiceRecordsError,
                     weak_factory_.GetWeakPtr(), std::move(remote_addr_clone),
                     target_uuid));
}

void ArcBluezBridge::CreateSdpRecord(
    mojom::BluetoothSdpRecordPtr record_mojo,
    arc::ArcBluetoothBridge::CreateSdpRecordCallback callback) {
  auto record = record_mojo.To<bluez::BluetoothServiceRecordBlueZ>();

  // Check if ServiceClassIDList attribute (attribute ID 0x0001) is included
  // after type conversion, since it is mandatory for creating a service record.
  if (!record.IsAttributePresented(kServiceClassIDListAttributeID)) {
    mojom::BluetoothCreateSdpRecordResultPtr result =
        mojom::BluetoothCreateSdpRecordResult::New();
    result->status = mojom::BluetoothStatus::FAIL;
    std::move(callback).Run(std::move(result));
    return;
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  GetAdapter()->CreateServiceRecord(
      record,
      base::BindOnce(&OnCreateServiceRecordDone,
                     std::move(split_callback.first)),
      base::BindOnce(&OnCreateServiceRecordError,
                     std::move(split_callback.second)));
}

void ArcBluezBridge::RemoveSdpRecord(uint32_t service_handle,
                                     RemoveSdpRecordCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  GetAdapter()->RemoveServiceRecord(
      service_handle,
      base::BindOnce(&OnRemoveServiceRecordDone,
                     std::move(split_callback.first)),
      base::BindOnce(&OnRemoveServiceRecordError,
                     std::move(split_callback.second)));
}

void ArcBluezBridge::CloseBluetoothListeningSocket(
    BluetoothListeningSocket* ptr) {
  auto itr = listening_sockets_.find(ptr);
  listening_sockets_.erase(itr);
}

void ArcBluezBridge::CloseBluetoothConnectingSocket(
    BluetoothConnectingSocket* ptr) {
  auto itr = connecting_sockets_.find(ptr);
  connecting_sockets_.erase(itr);
}

void ArcBluezBridge::OnGetServiceRecordsDone(
    mojom::BluetoothAddressPtr remote_addr,
    const BluetoothUUID& target_uuid,
    const std::vector<bluez::BluetoothServiceRecordBlueZ>& records_bluez) {
  auto* sdp_bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnGetSdpRecords);
  if (!sdp_bluetooth_instance) {
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

void ArcBluezBridge::OnGetServiceRecordsError(
    mojom::BluetoothAddressPtr remote_addr,
    const BluetoothUUID& target_uuid,
    bluez::BluetoothServiceRecordBlueZ::ErrorCode error_code) {
  auto* sdp_bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnGetSdpRecords);
  if (!sdp_bluetooth_instance) {
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

namespace {

union BluetoothSocketAddress {
  sockaddr sock;
  sockaddr_rc rfcomm;
  sockaddr_l2 l2cap;
};

int32_t GetSockOptvalFromFlags(mojom::BluetoothSocketType sock_type,
                               mojom::BluetoothSocketFlagsPtr sock_flags) {
  int optval = 0;
  switch (sock_type) {
    case mojom::BluetoothSocketType::TYPE_RFCOMM:
      optval |= sock_flags->encrypt ? RFCOMM_LM_ENCRYPT : 0;
      optval |= sock_flags->auth ? RFCOMM_LM_AUTH : 0;
      optval |= sock_flags->auth_mitm ? RFCOMM_LM_SECURE : 0;
      optval |= sock_flags->auth_16_digit ? RFCOMM_LM_SECURE : 0;
      return optval;
    case mojom::BluetoothSocketType::TYPE_L2CAP_LE:
      optval |= sock_flags->encrypt ? L2CAP_LM_ENCRYPT : 0;
      optval |= sock_flags->auth ? L2CAP_LM_AUTH : 0;
      optval |= sock_flags->auth_mitm ? L2CAP_LM_SECURE : 0;
      optval |= sock_flags->auth_16_digit ? L2CAP_LM_SECURE : 0;
      return optval;
  }
}

// Opens an AF_BLUETOOTH socket with |sock_type|, sets L2CAP_LM or RFCOMM_LM
// with |optval|, and binds the socket to address with |port|.
base::ScopedFD OpenBluetoothSocketImpl(mojom::BluetoothSocketType sock_type,
                                       int32_t optval,
                                       uint16_t port) {
  int protocol;
  int level;
  int optname;
  switch (sock_type) {
    case mojom::BluetoothSocketType::TYPE_RFCOMM:
      protocol = BTPROTO_RFCOMM;
      level = SOL_RFCOMM;
      optname = RFCOMM_LM;
      break;
    case mojom::BluetoothSocketType::TYPE_L2CAP_LE:
      protocol = BTPROTO_L2CAP;
      level = SOL_L2CAP;
      optname = L2CAP_LM;
      break;
    default:
      LOG(ERROR) << "Unknown socket type " << sock_type;
      return {};
  }

  base::ScopedFD sock(socket(AF_BLUETOOTH, SOCK_STREAM, protocol));
  if (!sock.is_valid()) {
    PLOG(ERROR) << "Failed to open bluetooth socket.";
    return {};
  }
  if (setsockopt(sock.get(), level, optname, &optval, sizeof(optval)) == -1) {
    PLOG(ERROR) << "Failed to setopt() on socket.";
    return {};
  }
  if (fcntl(sock.get(), F_SETFL, O_NONBLOCK | fcntl(sock.get(), F_GETFL)) ==
      -1) {
    PLOG(ERROR) << "Failed to fcntl() on socket.";
    return {};
  }

  BluetoothSocketAddress sa = {};
  switch (sock_type) {
    case mojom::BluetoothSocketType::TYPE_RFCOMM:
      sa.rfcomm.rc_family = AF_BLUETOOTH;
      sa.rfcomm.rc_channel = port;
      break;
    case mojom::BluetoothSocketType::TYPE_L2CAP_LE:
      sa.l2cap.l2_family = AF_BLUETOOTH;
      sa.l2cap.l2_psm = htobs(port);
      sa.l2cap.l2_bdaddr_type = BDADDR_LE_PUBLIC;
      break;
    default:
      LOG(ERROR) << "Unknown socket type " << sock_type;
      return {};
  }

  if (bind(sock.get(), &sa.sock, sizeof(sa)) == -1) {
    PLOG(ERROR) << "Failed to bind()";
    return {};
  }

  return sock;
}

}  // namespace

void ArcBluezBridge::OnBluezListeningSocketReady(
    ArcBluezBridge::BluetoothListeningSocket* sock_wrapper) {
  BluetoothSocketAddress sa;
  socklen_t addr_len = sizeof(sa);
  base::ScopedFD accept_fd(
      accept(sock_wrapper->file.get(), &sa.sock, &addr_len));
  if (!accept_fd.is_valid()) {
    PLOG(ERROR) << "Failed to accept()";
    return;
  }
  if (fcntl(accept_fd.get(), F_SETFL,
            O_NONBLOCK | fcntl(accept_fd.get(), F_GETFL)) == -1) {
    PLOG(ERROR) << "Failed to fnctl()";
    return;
  }

  mojo::ScopedHandle handle =
      mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(accept_fd)));

  // Tells Android we successfully accept() a new connection.
  auto connection = mojom::BluetoothSocketConnection::New();
  connection->sock = std::move(handle);
  switch (sock_wrapper->sock_type) {
    case mojom::BluetoothSocketType::TYPE_RFCOMM:
      connection->addr =
          mojom::BluetoothAddress::From<bdaddr_t>(sa.rfcomm.rc_bdaddr);
      connection->port = sa.rfcomm.rc_channel;
      break;
    case mojom::BluetoothSocketType::TYPE_L2CAP_LE:
      connection->addr =
          mojom::BluetoothAddress::From<bdaddr_t>(sa.l2cap.l2_bdaddr);
      connection->port = btohs(sa.l2cap.l2_psm);
      break;
    default:
      LOG(ERROR) << "Unknown socket type " << sock_wrapper->sock_type;
      return;
  }

  sock_wrapper->remote->OnAccepted(std::move(connection));
}

void ArcBluezBridge::OnBluezConnectingSocketReady(
    ArcBluezBridge::BluetoothConnectingSocket* sock_wrapper) {
  // When connect() is ready, we will transfer this fd to Android, and Android
  // is responsible for closing it. The file watcher |controller| needs to be
  // disabled first, and then the fd ownership is transferred.
  sock_wrapper->controller.reset();
  base::ScopedFD fd = std::move(sock_wrapper->file);

  // Checks whether connect() succeeded.
  int err = 0;
  socklen_t len = sizeof(err);
  int ret = getsockopt(fd.get(), SOL_SOCKET, SO_ERROR, &err, &len);
  if (ret != 0 || err != 0) {
    LOG(ERROR) << "Failed to connect. err=" << err;
    sock_wrapper->remote->OnConnectFailed();
  }

  // Gets peer address.
  BluetoothSocketAddress peer_sa;
  socklen_t peer_sa_len = sizeof(peer_sa);
  if (getpeername(fd.get(), &peer_sa.sock, &peer_sa_len) == -1) {
    PLOG(ERROR) << "Failed to getpeername().";
    sock_wrapper->remote->OnConnectFailed();
  }

  // Gets our port.
  BluetoothSocketAddress local_sa;
  socklen_t local_sa_len = sizeof(local_sa);
  if (getsockname(fd.get(), &local_sa.sock, &local_sa_len) == -1) {
    PLOG(ERROR) << "Failed to getsockname()";
    sock_wrapper->remote->OnConnectFailed();
  }

  mojo::ScopedHandle handle =
      mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(fd)));

  // Notifies Android.
  auto connection = mojom::BluetoothSocketConnection::New();
  connection->sock = std::move(handle);
  switch (sock_wrapper->sock_type) {
    case mojom::BluetoothSocketType::TYPE_RFCOMM:
      connection->addr =
          mojom::BluetoothAddress::From<bdaddr_t>(peer_sa.rfcomm.rc_bdaddr);
      connection->port = local_sa.rfcomm.rc_channel;
      break;
    case mojom::BluetoothSocketType::TYPE_L2CAP_LE:
      connection->addr =
          mojom::BluetoothAddress::From<bdaddr_t>(peer_sa.l2cap.l2_bdaddr);
      connection->port = btohs(local_sa.l2cap.l2_psm);
      break;
    default:
      LOG(ERROR) << "Unknown socket type " << sock_wrapper->sock_type;
      return;
  }
  sock_wrapper->remote->OnConnected(std::move(connection));
}

void ArcBluezBridge::CreateBluetoothListenSocket(
    mojom::BluetoothSocketType type,
    mojom::BluetoothSocketFlagsPtr flags,
    int port,
    BluetoothSocketListenCallback callback) {
  std::unique_ptr<ArcBluezBridge::BluetoothListeningSocket> sock_wrapper =
      nullptr;
  int32_t optval = GetSockOptvalFromFlags(type, std::move(flags));
  uint16_t listen_port = static_cast<uint16_t>(port);

  do {
    base::ScopedFD sock = OpenBluetoothSocketImpl(type, optval, listen_port);
    if (!sock.is_valid()) {
      LOG(ERROR) << "Failed to open listen socket.";
      break;
    }

    if (listen(sock.get(), /*backlog=*/1) == -1) {
      PLOG(ERROR) << "Failed to listen()";
      break;
    }

    BluetoothSocketAddress local_addr;
    socklen_t addr_len = sizeof(local_addr);
    if (getsockname(sock.get(), &local_addr.sock, &addr_len) == -1) {
      PLOG(ERROR) << "Failed to getsockname()";
      break;
    }

    sock_wrapper = std::make_unique<BluetoothListeningSocket>();
    sock_wrapper->sock_type = type;
    sock_wrapper->controller = base::FileDescriptorWatcher::WatchReadable(
        sock.get(),
        base::BindRepeating(&ArcBluezBridge::OnBluezListeningSocketReady,
                            weak_factory_.GetWeakPtr(), sock_wrapper.get()));
    sock_wrapper->file = std::move(sock);

    if (type == mojom::BluetoothSocketType::TYPE_RFCOMM) {
      listen_port = local_addr.rfcomm.rc_channel;
    } else if (type == mojom::BluetoothSocketType::TYPE_L2CAP_LE) {
      listen_port = btohs(local_addr.l2cap.l2_psm);
    } else {
      LOG(ERROR) << "Unknown socket type " << type;
      break;
    }
  } while (!sock_wrapper.get());

  if (sock_wrapper) {
    std::move(callback).Run(mojom::BluetoothStatus::SUCCESS, listen_port,
                            sock_wrapper->remote.BindNewPipeAndPassReceiver());
    sock_wrapper->remote.set_disconnect_handler(
        base::BindOnce(&ArcBluezBridge::CloseBluetoothListeningSocket,
                       weak_factory_.GetWeakPtr(), sock_wrapper.get()));
    listening_sockets_.insert(std::move(sock_wrapper));
  } else {
    std::move(callback).Run(
        mojom::BluetoothStatus::FAIL, /*port=*/0,
        mojo::PendingReceiver<mojom::BluetoothListenSocketClient>());
  }
}

void ArcBluezBridge::CreateBluetoothConnectSocket(
    mojom::BluetoothSocketType type,
    mojom::BluetoothSocketFlagsPtr flags,
    mojom::BluetoothAddressPtr addr,
    int port,
    BluetoothSocketConnectCallback callback) {
  std::unique_ptr<ArcBluetoothBridge::BluetoothConnectingSocket> sock_wrapper =
      nullptr;
  int32_t optval = GetSockOptvalFromFlags(type, std::move(flags));
  base::ScopedFD sock = OpenBluetoothSocketImpl(type, optval, kAutoSockPort);
  int ret = 0;

  do {
    if (!sock.is_valid()) {
      LOG(ERROR) << "Failed to open connect socket.";
      break;
    }

    std::string addr_str = addr->To<std::string>();
    BluetoothDevice* device = bluetooth_adapter_->GetDevice(addr_str);
    if (!device) {
      break;
    }

    const auto addr_type = device->GetAddressType();
    if (addr_type == BluetoothDevice::ADDR_TYPE_UNKNOWN) {
      LOG(ERROR) << "Unknown address type.";
      break;
    }

    BluetoothSocketAddress sa = {};
    if (type == mojom::BluetoothSocketType::TYPE_RFCOMM) {
      sa.rfcomm.rc_family = AF_BLUETOOTH;
      sa.rfcomm.rc_bdaddr = addr->To<bdaddr_t>();
      sa.rfcomm.rc_channel = static_cast<uint8_t>(port);
    } else if (type == mojom::BluetoothSocketType::TYPE_L2CAP_LE) {
      sa.l2cap.l2_family = AF_BLUETOOTH;
      sa.l2cap.l2_bdaddr = addr->To<bdaddr_t>();
      sa.l2cap.l2_psm = htobs(port);
      sa.l2cap.l2_bdaddr_type = addr_type == BluetoothDevice::ADDR_TYPE_PUBLIC
                                    ? BDADDR_LE_PUBLIC
                                    : BDADDR_LE_RANDOM;
    } else {
      LOG(ERROR) << "Unknown socket type " << type;
    }

    ret = HANDLE_EINTR(connect(
        sock.get(), reinterpret_cast<const struct sockaddr*>(&sa), sizeof(sa)));

    sock_wrapper =
        std::make_unique<ArcBluetoothBridge::BluetoothConnectingSocket>();
    sock_wrapper->sock_type = type;
  } while (sock_wrapper == nullptr);

  if (sock_wrapper) {
    if (ret == 0) {
      // connect() returns success immediately.
      sock_wrapper->file = std::move(sock);
      // BluetoothSocketConnect() is a blocking mojo call on the ARC side, so
      // the callback needs to be triggered asynchronously and thus we use a
      // PostTask here.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&ArcBluezBridge::OnBluezConnectingSocketReady,
                         weak_factory_.GetWeakPtr(), sock_wrapper.get()));
    } else if (errno == EINPROGRESS) {
      sock_wrapper->controller = base::FileDescriptorWatcher::WatchWritable(
          sock.get(),
          base::BindRepeating(&ArcBluezBridge::OnBluezConnectingSocketReady,
                              weak_factory_.GetWeakPtr(), sock_wrapper.get()));
      sock_wrapper->file = std::move(sock);
    } else {
      PLOG(ERROR) << "Failed to connect.";
      std::move(callback).Run(
          mojom::BluetoothStatus::FAIL,
          mojo::PendingReceiver<arc::mojom::BluetoothConnectSocketClient>());
      return;
    }
  } else {
    std::move(callback).Run(
        mojom::BluetoothStatus::FAIL,
        mojo::PendingReceiver<arc::mojom::BluetoothConnectSocketClient>());
    return;
  }

  std::move(callback).Run(mojom::BluetoothStatus::SUCCESS,
                          sock_wrapper->remote.BindNewPipeAndPassReceiver());
  sock_wrapper->remote.set_disconnect_handler(
      base::BindOnce(&ArcBluezBridge::CloseBluetoothConnectingSocket,
                     weak_factory_.GetWeakPtr(), sock_wrapper.get()));
  connecting_sockets_.insert(std::move(sock_wrapper));
}

}  // namespace arc

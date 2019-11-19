// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/usb/android_usb_device.h"

#include <set>
#include <utility>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/devtools/device/usb/android_rsa.h"
#include "chrome/browser/devtools/device/usb/android_usb_socket.h"
#include "crypto/rsa_private_key.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

using device::mojom::UsbTransferStatus;

namespace {

const size_t kHeaderSize = 24;

const int kUsbTimeout = 0;

const uint32_t kMaxPayload = 4096;
const uint32_t kVersion = 0x01000000;

static const char kHostConnectMessage[] = "host::";

// Stores android wrappers around claimed usb devices on caller thread.
base::LazyInstance<std::vector<AndroidUsbDevice*>>::Leaky g_devices =
    LAZY_INSTANCE_INITIALIZER;

// Stores the GUIDs of devices that are currently opened so that they are not
// re-probed.
base::LazyInstance<std::vector<std::string>>::Leaky g_open_devices =
    LAZY_INSTANCE_INITIALIZER;

uint32_t Checksum(const std::string& data) {
  unsigned char* x = (unsigned char*)data.data();
  int count = data.length();
  uint32_t sum = 0;
  while (count-- > 0)
    sum += *x++;
  return sum;
}

void DumpMessage(bool outgoing, const uint8_t* data, size_t length) {
#if 0
  std::string result = "";
  if (length == kHeaderSize) {
    for (size_t i = 0; i < 24; ++i) {
      result += base::StringPrintf("%02x", data[i]);
      if ((i + 1) % 4 == 0)
        result += " ";
    }
    for (size_t i = 0; i < 24; ++i) {
      if (data[i] >= 0x20 && data[i] <= 0x7E)
        result += data[i];
      else
        result += ".";
    }
  } else {
    result = base::StringPrintf("%d: ", static_cast<int>(length));
    for (size_t i = 0; i < length; ++i) {
      if (data[i] >= 0x20 && data[i] <= 0x7E)
        result += data[i];
      else
        result += ".";
    }
  }
  LOG(ERROR) << (outgoing ? "[out] " : "[ in] ") << result;
#endif  // 0
}

void OnProbeFinished(const AndroidUsbDevicesCallback& callback,
                     AndroidUsbDevices* new_devices) {
  std::unique_ptr<AndroidUsbDevices> devices(new_devices);

  // Add raw pointers to the newly claimed devices.
  for (const scoped_refptr<AndroidUsbDevice>& device : *devices) {
    g_devices.Get().push_back(device.get());
  }

  // Return all claimed devices.
  AndroidUsbDevices result(g_devices.Get().begin(), g_devices.Get().end());
  callback.Run(result);
}

void OnDeviceClosed(const std::string& guid,
                    mojo::Remote<device::mojom::UsbDevice> device) {
  base::Erase(g_open_devices.Get(), guid);
}

void OnDeviceClosedWithBarrier(const std::string& guid,
                               mojo::Remote<device::mojom::UsbDevice> device,
                               const base::RepeatingClosure& barrier) {
  base::Erase(g_open_devices.Get(), guid);
  barrier.Run();
}

void CreateDeviceOnInterfaceClaimed(
    AndroidUsbDevices* devices,
    crypto::RSAPrivateKey* rsa_key,
    AndroidDeviceInfo android_device_info,
    mojo::Remote<device::mojom::UsbDevice> device,
    const base::RepeatingClosure& barrier,
    bool success) {
  if (success) {
    devices->push_back(
        new AndroidUsbDevice(rsa_key, android_device_info, std::move(device)));
    barrier.Run();
  } else {
    auto* device_raw = device.get();
    device_raw->Close(base::BindOnce(&OnDeviceClosedWithBarrier,
                                     android_device_info.guid,
                                     std::move(device), barrier));
  }
}

void OnInterfaceReleased(mojo::Remote<device::mojom::UsbDevice> device,
                         const std::string& guid,
                         bool release_successful) {
  auto* device_raw = device.get();
  device_raw->Close(base::BindOnce(&OnDeviceClosed, guid, std::move(device)));
}

void OnDeviceOpened(AndroidUsbDevices* devices,
                    crypto::RSAPrivateKey* rsa_key,
                    AndroidDeviceInfo android_device_info,
                    mojo::Remote<device::mojom::UsbDevice> device,
                    const base::RepeatingClosure& barrier,
                    device::mojom::UsbOpenDeviceError error) {
  // For UsbOpenDeviceError::OK and UsbOpenDeviceError::ALREADY_OPEN we all try
  // to claim the interface because the device may be opened by other modules or
  // extensions for different interface.
  if (error != device::mojom::UsbOpenDeviceError::ACCESS_DENIED) {
    DCHECK(device);
    auto* device_raw = device.get();
    device_raw->ClaimInterface(
        android_device_info.interface_id,
        base::BindOnce(&CreateDeviceOnInterfaceClaimed, devices, rsa_key,
                       android_device_info, std::move(device), barrier));
  } else {
    base::Erase(g_open_devices.Get(), android_device_info.guid);
    barrier.Run();
  }
}

void OpenAndroidDevices(crypto::RSAPrivateKey* rsa_key,
                        const AndroidUsbDevicesCallback& callback,
                        std::vector<AndroidDeviceInfo> device_info_list) {
  // Add new devices.
  AndroidUsbDevices* devices = new AndroidUsbDevices();
  base::RepeatingClosure barrier =
      base::BarrierClosure(device_info_list.size(),
                           base::BindOnce(&OnProbeFinished, callback, devices));

  for (const auto& device_info : device_info_list) {
    if (base::Contains(g_open_devices.Get(), device_info.guid)) {
      // This device is already open, do not make parallel attempts to connect
      // to it.
      barrier.Run();
      continue;
    }
    g_open_devices.Get().push_back(device_info.guid);

    mojo::Remote<device::mojom::UsbDevice> device;
    UsbDeviceManagerHelper::GetInstance()->GetDevice(
        device_info.guid, device.BindNewPipeAndPassReceiver());
    auto* device_raw = device.get();
    device_raw->Open(base::BindOnce(&OnDeviceOpened, devices, rsa_key,
                                    device_info, std::move(device), barrier));
  }
}

}  // namespace

AdbMessage::AdbMessage(uint32_t command,
                       uint32_t arg0,
                       uint32_t arg1,
                       const std::string& body)
    : command(command), arg0(arg0), arg1(arg1), body(body) {}

AdbMessage::~AdbMessage() {}

// static
void AndroidUsbDevice::Enumerate(crypto::RSAPrivateKey* rsa_key,
                                 const AndroidUsbDevicesCallback& callback) {
  UsbDeviceManagerHelper::GetInstance()->GetAndroidDevices(
      base::BindOnce(&OpenAndroidDevices, rsa_key, callback));
}

AndroidUsbDevice::AndroidUsbDevice(
    crypto::RSAPrivateKey* rsa_key,
    const AndroidDeviceInfo& android_device_info,
    mojo::Remote<device::mojom::UsbDevice> device)
    : rsa_key_(rsa_key->Copy()),
      device_(std::move(device)),
      android_device_info_(android_device_info),
      is_connected_(false),
      signature_sent_(false),
      last_socket_id_(256) {
  DCHECK(device_);
  device_.set_disconnect_handler(
      base::BindOnce(&AndroidUsbDevice::Terminate, weak_factory_.GetWeakPtr()));
}

void AndroidUsbDevice::InitOnCallerThread() {
  if (task_runner_)
    return;
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  Queue(std::make_unique<AdbMessage>(AdbMessage::kCommandCNXN, kVersion,
                                     kMaxPayload, kHostConnectMessage));
  ReadHeader();
}

net::StreamSocket* AndroidUsbDevice::CreateSocket(const std::string& command) {
  if (!device_)
    return nullptr;

  uint32_t socket_id = ++last_socket_id_;
  sockets_[socket_id] = new AndroidUsbSocket(
      this, socket_id, command,
      base::Bind(&AndroidUsbDevice::SocketDeleted, this, socket_id));
  return sockets_[socket_id];
}

void AndroidUsbDevice::Send(uint32_t command,
                            uint32_t arg0,
                            uint32_t arg1,
                            const std::string& body) {
  auto message = std::make_unique<AdbMessage>(command, arg0, arg1, body);
  // Delay open request if not yet connected.
  if (!is_connected_) {
    pending_messages_.push_back(std::move(message));
    return;
  }
  Queue(std::move(message));
}

AndroidUsbDevice::~AndroidUsbDevice() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  Terminate();
}

void AndroidUsbDevice::Queue(std::unique_ptr<AdbMessage> message) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Queue header.
  std::vector<uint32_t> header;
  header.push_back(message->command);
  header.push_back(message->arg0);
  header.push_back(message->arg1);
  bool append_zero = true;
  if (message->body.empty())
    append_zero = false;
  if (message->command == AdbMessage::kCommandAUTH &&
      message->arg0 == AdbMessage::kAuthSignature)
    append_zero = false;
  if (message->command == AdbMessage::kCommandWRTE)
    append_zero = false;

  size_t body_length = message->body.length() + (append_zero ? 1 : 0);
  header.push_back(body_length);
  header.push_back(Checksum(message->body));
  header.push_back(message->command ^ 0xffffffff);
  // TODO(donna.wu@intel.com): eliminate the buffer copy here, needs to change
  // type BulkMessage.
  auto header_buffer = base::MakeRefCounted<base::RefCountedBytes>(
      reinterpret_cast<uint8_t*>(header.data()), kHeaderSize);
  outgoing_queue_.push(header_buffer);

  // Queue body.
  if (!message->body.empty()) {
    auto body_buffer = base::MakeRefCounted<base::RefCountedBytes>(body_length);
    memcpy(body_buffer->front(), message->body.data(), message->body.length());
    if (append_zero)
      body_buffer->data()[body_length - 1] = 0;
    outgoing_queue_.push(body_buffer);
    if (android_device_info_.zero_mask &&
        (body_length & android_device_info_.zero_mask) == 0) {
      // Send a zero length packet.
      outgoing_queue_.push(base::MakeRefCounted<base::RefCountedBytes>(0));
    }
  }
  ProcessOutgoing();
}

void AndroidUsbDevice::ProcessOutgoing() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (outgoing_queue_.empty() || !device_)
    return;

  BulkMessage message = outgoing_queue_.front();
  outgoing_queue_.pop();
  DumpMessage(true, message->front(), message->size());

  device_->GenericTransferOut(android_device_info_.outbound_address,
                              message->data(), kUsbTimeout,
                              base::Bind(&AndroidUsbDevice::OutgoingMessageSent,
                                         weak_factory_.GetWeakPtr()));
}

void AndroidUsbDevice::OutgoingMessageSent(UsbTransferStatus status) {
  if (status != UsbTransferStatus::COMPLETED)
    return;

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AndroidUsbDevice::ProcessOutgoing, this));
}

void AndroidUsbDevice::ReadHeader() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!device_)
    return;

  device_->GenericTransferIn(
      android_device_info_.inbound_address, kHeaderSize, kUsbTimeout,
      base::Bind(&AndroidUsbDevice::ParseHeader, weak_factory_.GetWeakPtr()));
}

void AndroidUsbDevice::ParseHeader(UsbTransferStatus status,
                                   const std::vector<uint8_t>& buffer) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (status == UsbTransferStatus::TIMEOUT) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AndroidUsbDevice::ReadHeader, this));
    return;
  }

  if (status != UsbTransferStatus::COMPLETED || buffer.size() != kHeaderSize) {
    TransferError(status);
    return;
  }

  DumpMessage(false, buffer.data(), buffer.size());
  const auto* header = reinterpret_cast<const uint32_t*>(buffer.data());
  std::unique_ptr<AdbMessage> message(
      new AdbMessage(header[0], header[1], header[2], ""));
  uint32_t data_length = header[3];
  uint32_t data_check = header[4];
  uint32_t magic = header[5];
  if ((message->command ^ 0xffffffff) != magic) {
    TransferError(UsbTransferStatus::TRANSFER_ERROR);
    return;
  }

  if (data_length == 0) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AndroidUsbDevice::HandleIncoming, this,
                                  std::move(message)));
  } else {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AndroidUsbDevice::ReadBody, this,
                                  std::move(message), data_length, data_check));
  }
}

void AndroidUsbDevice::ReadBody(std::unique_ptr<AdbMessage> message,
                                uint32_t data_length,
                                uint32_t data_check) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!device_.get()) {
    return;
  }

  device_->GenericTransferIn(
      android_device_info_.inbound_address, data_length, kUsbTimeout,
      base::Bind(&AndroidUsbDevice::ParseBody, weak_factory_.GetWeakPtr(),
                 base::Passed(&message), data_length, data_check));
}

void AndroidUsbDevice::ParseBody(std::unique_ptr<AdbMessage> message,
                                 uint32_t data_length,
                                 uint32_t data_check,
                                 UsbTransferStatus status,
                                 const std::vector<uint8_t>& buffer) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (status == UsbTransferStatus::TIMEOUT) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AndroidUsbDevice::ReadBody, this,
                                  std::move(message), data_length, data_check));
    return;
  }

  if (status != UsbTransferStatus::COMPLETED ||
      static_cast<uint32_t>(buffer.size()) != data_length) {
    TransferError(status);
    return;
  }

  DumpMessage(false, buffer.data(), data_length);
  message->body =
      std::string(reinterpret_cast<const char*>(buffer.data()), buffer.size());
  if (Checksum(message->body) != data_check) {
    TransferError(UsbTransferStatus::TRANSFER_ERROR);
    return;
  }

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&AndroidUsbDevice::HandleIncoming, this,
                                        std::move(message)));
}

void AndroidUsbDevice::HandleIncoming(std::unique_ptr<AdbMessage> message) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  switch (message->command) {
    case AdbMessage::kCommandAUTH: {
      DCHECK_EQ(message->arg0, static_cast<uint32_t>(AdbMessage::kAuthToken));
      if (signature_sent_) {
        Queue(std::make_unique<AdbMessage>(
            AdbMessage::kCommandAUTH, AdbMessage::kAuthRSAPublicKey, 0,
            AndroidRSAPublicKey(rsa_key_.get())));
      } else {
        signature_sent_ = true;
        std::string signature = AndroidRSASign(rsa_key_.get(), message->body);
        if (!signature.empty()) {
          Queue(std::make_unique<AdbMessage>(AdbMessage::kCommandAUTH,
                                             AdbMessage::kAuthSignature, 0,
                                             signature));
        } else {
          Queue(std::make_unique<AdbMessage>(
              AdbMessage::kCommandAUTH, AdbMessage::kAuthRSAPublicKey, 0,
              AndroidRSAPublicKey(rsa_key_.get())));
        }
      }
    } break;
    case AdbMessage::kCommandCNXN:
      {
        is_connected_ = true;
        PendingMessages pending;
        pending.swap(pending_messages_);
        for (auto& msg : pending)
          Queue(std::move(msg));
      }
      break;
    case AdbMessage::kCommandOKAY:
    case AdbMessage::kCommandWRTE:
    case AdbMessage::kCommandCLSE:
      {
      auto it = sockets_.find(message->arg1);
      if (it != sockets_.end())
        it->second->HandleIncoming(std::move(message));
      }
      break;
    default:
      break;
  }
  ReadHeader();
}

void AndroidUsbDevice::TransferError(UsbTransferStatus status) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  Terminate();
}

void AndroidUsbDevice::Terminate() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Remove this AndroidUsbDevice from |g_devices|.
  auto it = std::find(g_devices.Get().begin(), g_devices.Get().end(), this);
  if (it != g_devices.Get().end())
    g_devices.Get().erase(it);

  // For connection error, remove the guid from recored opening/opened list.
  // For transfer errors, we'll do this after releasing the interface.
  if (!device_) {
    base::Erase(g_open_devices.Get(), android_device_info_.guid);
    return;
  }

  // For Transfer error case.
  // Make sure we zero-out |device_| so that closing connections did not
  // open new socket connections.
  mojo::Remote<device::mojom::UsbDevice> device = std::move(device_);
  device_.reset();

  // Iterate over copy.
  AndroidUsbSockets sockets(sockets_);
  for (auto it = sockets.begin(); it != sockets.end(); ++it) {
    it->second->Terminated(true);
  }
  DCHECK(sockets_.empty());

  auto* device_raw = device.get();
  device_raw->ReleaseInterface(
      android_device_info_.interface_id,
      base::BindOnce(&OnInterfaceReleased, std::move(device),
                     android_device_info_.guid));
}

void AndroidUsbDevice::SocketDeleted(uint32_t socket_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  sockets_.erase(socket_id);
}

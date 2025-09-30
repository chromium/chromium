// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/usb/android_usb_device.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/devtools/device/usb/android_rsa.h"
#include "chrome/browser/devtools/device/usb/android_usb_socket.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

using device::mojom::UsbTransferStatus;

namespace {

constexpr size_t kHeaderSize = 24;

constexpr int kUsbTimeout = 0;

constexpr uint32_t kMaxPayload = 4096;
constexpr uint32_t kVersion = 0x01000000;

constexpr char kHostConnectMessage[] = "host::";

// Stores android wrappers around claimed usb devices on caller thread.
std::vector<AndroidUsbDevice*>& GetDevices() {
  static base::NoDestructor<std::vector<AndroidUsbDevice*>> devices;
  return *devices;
}

// Stores the GUIDs of devices that are currently opened so that they are not
// re-probed.
std::vector<std::string>& GetOpenDevices() {
  static base::NoDestructor<std::vector<std::string>> open_devices;
  return *open_devices;
}

uint32_t Checksum(const std::string& data) {
  uint32_t sum = 0;
  for (char c : data) {
    sum += c;
  }
  return sum;
}

void DumpMessage(bool outgoing, base::span<const uint8_t> data) {
#if 0
  auto is_printable = [](uint8_t c) { return c >= 0x20 && c <= 0x7E; };
  std::string result;
  if (data.size() == kHeaderSize) {
    for (size_t i = 0; i < 24; ++i) {
      result += base::StringPrintf("%02x", data[i]);
      if ((i + 1) % 4 == 0)
        result += " ";
    }
    for (const uint8_t c : data.first<24>()) {
      if (is_printable(c)) {
        result += c;
      } else {
        result += ".";
      }
    }
  } else {
    result = base::StringPrintf("%d: ", static_cast<int>(data.size()));
    for (const uint8_t c : data) {
      if (is_printable(c)) {
        result += c;
      } else {
        result += ".";
      }
    }
  }
  LOG(ERROR) << (outgoing ? "[out] " : "[ in] ") << result;
#endif  // 0
}

void OnProbeFinished(AndroidUsbDevicesCallback callback,
                     AndroidUsbDevices* new_devices) {
  std::unique_ptr<AndroidUsbDevices> devices(new_devices);

  // Add raw pointers to the newly claimed devices.
  for (const scoped_refptr<AndroidUsbDevice>& device : *devices) {
    GetDevices().push_back(device.get());
  }

  // Return all claimed devices.
  AndroidUsbDevices result(GetDevices().begin(), GetDevices().end());
  std::move(callback).Run(result);
}

void OnDeviceClosed(const std::string& guid,
                    mojo::Remote<device::mojom::UsbDevice> device) {
  std::erase(GetOpenDevices(), guid);
}

void OnDeviceClosedWithBarrier(const std::string& guid,
                               mojo::Remote<device::mojom::UsbDevice> device,
                               const base::RepeatingClosure& barrier) {
  std::erase(GetOpenDevices(), guid);
  barrier.Run();
}

void CreateDeviceOnInterfaceClaimed(
    AndroidUsbDevices* devices,
    crypto::keypair::PrivateKey rsa_key,
    AndroidDeviceInfo android_device_info,
    mojo::Remote<device::mojom::UsbDevice> device,
    const base::RepeatingClosure& barrier,
    device::mojom::UsbClaimInterfaceResult result) {
  if (result == device::mojom::UsbClaimInterfaceResult::kSuccess) {
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
                    crypto::keypair::PrivateKey rsa_key,
                    AndroidDeviceInfo android_device_info,
                    mojo::Remote<device::mojom::UsbDevice> device,
                    const base::RepeatingClosure& barrier,
                    device::mojom::UsbOpenDeviceResultPtr result) {
  // If the error is UsbOpenDeviceError::ALREADY_OPEN we all try to claim the
  // interface because the device may be opened by other modules or extensions
  // for different interface.
  if (result->is_success() ||
      result->get_error() == device::mojom::UsbOpenDeviceError::ALREADY_OPEN) {
    DCHECK(device);
    auto* device_raw = device.get();
    device_raw->ClaimInterface(
        android_device_info.interface_id,
        base::BindOnce(&CreateDeviceOnInterfaceClaimed, devices, rsa_key,
                       android_device_info, std::move(device), barrier));
  } else {
    std::erase(GetOpenDevices(), android_device_info.guid);
    barrier.Run();
  }
}

void OpenAndroidDevices(crypto::keypair::PrivateKey rsa_key,
                        AndroidUsbDevicesCallback callback,
                        std::vector<AndroidDeviceInfo> device_info_list) {
  // Add new devices.
  AndroidUsbDevices* devices = new AndroidUsbDevices();
  base::RepeatingClosure barrier = base::BarrierClosure(
      device_info_list.size(),
      base::BindOnce(&OnProbeFinished, std::move(callback), devices));

  for (const auto& device_info : device_info_list) {
    if (base::Contains(GetOpenDevices(), device_info.guid)) {
      // This device is already open, do not make parallel attempts to connect
      // to it.
      barrier.Run();
      continue;
    }
    GetOpenDevices().push_back(device_info.guid);

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

AdbMessage::~AdbMessage() = default;

// static
void AndroidUsbDevice::Enumerate(crypto::keypair::PrivateKey rsa_key,
                                 AndroidUsbDevicesCallback callback) {
  UsbDeviceManagerHelper::GetInstance()->GetAndroidDevices(
      base::BindOnce(&OpenAndroidDevices, rsa_key, std::move(callback)));
}

AndroidUsbDevice::AndroidUsbDevice(
    crypto::keypair::PrivateKey rsa_key,
    const AndroidDeviceInfo& android_device_info,
    mojo::Remote<device::mojom::UsbDevice> device)
    : rsa_key_(rsa_key),
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
  task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
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
      base::BindOnce(&AndroidUsbDevice::SocketDeleted, this, socket_id));
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
  DCHECK_EQ(kHeaderSize, base::as_byte_span(header).size());

  // TODO(donna.wu@intel.com): eliminate the buffer copy here, needs to change
  // type BulkMessage.
  auto header_buffer =
      base::MakeRefCounted<base::RefCountedBytes>(base::as_byte_span(header));
  outgoing_queue_.push(header_buffer);

  // Queue body.
  if (!message->body.empty()) {
    auto body_buffer = base::MakeRefCounted<base::RefCountedBytes>(body_length);
    {
      auto& v = body_buffer->as_vector();
      base::span(v).copy_prefix_from(base::as_byte_span(message->body));
      if (append_zero) {
        v[body_length - 1] = 0;
      }
    }
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
  DumpMessage(true, base::span(*message));

  device_->GenericTransferOut(
      android_device_info_.outbound_address, message->as_vector(), kUsbTimeout,
      base::BindOnce(&AndroidUsbDevice::OutgoingMessageSent,
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

  device_->GenericTransferIn(android_device_info_.inbound_address, kHeaderSize,
                             kUsbTimeout,
                             base::BindOnce(&AndroidUsbDevice::ParseHeader,
                                            weak_factory_.GetWeakPtr()));
}

void AndroidUsbDevice::ParseHeader(UsbTransferStatus status,
                                   base::span<const uint8_t> buffer) {
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

  DumpMessage(false, buffer);
  base::span<const uint8_t> header_span = buffer.first<6 * sizeof(uint32_t)>();
  uint32_t command = base::U32FromLittleEndian(header_span.take_first<4>());
  uint32_t arg0 = base::U32FromLittleEndian(header_span.take_first<4>());
  uint32_t arg1 = base::U32FromLittleEndian(header_span.take_first<4>());
  auto message = std::make_unique<AdbMessage>(command, arg0, arg1, /*body=*/"");
  uint32_t data_length = base::U32FromLittleEndian(header_span.take_first<4>());
  uint32_t data_check = base::U32FromLittleEndian(header_span.take_first<4>());
  uint32_t magic = base::U32FromLittleEndian(header_span.take_first<4>());
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
      base::BindOnce(&AndroidUsbDevice::ParseBody, weak_factory_.GetWeakPtr(),
                     std::move(message), data_length, data_check));
}

void AndroidUsbDevice::ParseBody(std::unique_ptr<AdbMessage> message,
                                 uint32_t data_length,
                                 uint32_t data_check,
                                 UsbTransferStatus status,
                                 base::span<const uint8_t> buffer) {
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

  DumpMessage(false, buffer);
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
      if (message->arg0 != static_cast<uint32_t>(AdbMessage::kAuthToken)) {
        TransferError(UsbTransferStatus::TRANSFER_ERROR);
        return;
      }
      if (signature_sent_) {
        std::optional<std::string> pub = AndroidRSAPublicKey(rsa_key_);
        if (!pub) {
          TransferError(UsbTransferStatus::TRANSFER_ERROR);
          return;
        }
        Queue(std::make_unique<AdbMessage>(
            AdbMessage::kCommandAUTH, AdbMessage::kAuthRSAPublicKey, 0, *pub));
      } else {
        signature_sent_ = true;
        std::string signature = AndroidRSASign(rsa_key_, message->body);
        if (signature.empty()) {
          // This may fail if the device requests to sign a token that is not
          // the same size as a SHA-1 hash. ADB does not use a standard
          // signature scheme and instead treats an arbitrary peer-supplied
          // token as the SHA-1 hash.
          TransferError(UsbTransferStatus::TRANSFER_ERROR);
          return;
        }
        Queue(std::make_unique<AdbMessage>(AdbMessage::kCommandAUTH,
                                           AdbMessage::kAuthSignature, 0,
                                           signature));
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

  // Remove this AndroidUsbDevice from GetDevices().
  auto it = std::ranges::find(GetDevices(), this);
  if (it != GetDevices().end()) {
    GetDevices().erase(it);
  }

  // For connection error, remove the guid from recored opening/opened list.
  // For transfer errors, we'll do this after releasing the interface.
  if (!device_) {
    std::erase(GetOpenDevices(), android_device_info_.guid);
    return;
  }

  // For Transfer error case.
  // Make sure we zero-out |device_| so that closing connections did not
  // open new socket connections.
  mojo::Remote<device::mojom::UsbDevice> device = std::move(device_);
  device_.reset();

  // Iterate over copy.
  AndroidUsbSockets sockets(sockets_);
  for (auto socket_it = sockets.begin(); socket_it != sockets.end();
       ++socket_it) {
    socket_it->second->Terminated(true);
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

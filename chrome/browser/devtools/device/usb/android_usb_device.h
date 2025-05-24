// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_USB_ANDROID_USB_DEVICE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_USB_ANDROID_USB_DEVICE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/devtools/device/usb/usb_device_manager_helper.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_device.mojom-forward.h"

namespace base {
class RefCountedBytes;
class SingleThreadTaskRunner;
}

namespace crypto {
class RSAPrivateKey;
}

namespace net {
class StreamSocket;
}

class AndroidUsbSocket;

class AdbMessage {
 public:
  enum Command {
    kCommandSYNC = 0x434e5953,
    kCommandCNXN = 0x4e584e43,
    kCommandOPEN = 0x4e45504f,
    kCommandOKAY = 0x59414b4f,
    kCommandCLSE = 0x45534c43,
    kCommandWRTE = 0x45545257,
    kCommandAUTH = 0x48545541
  };

  enum Auth {
    kAuthToken = 1,
    kAuthSignature = 2,
    kAuthRSAPublicKey = 3
  };

  AdbMessage(uint32_t command,
             uint32_t arg0,
             uint32_t arg1,
             const std::string& body);

  AdbMessage(const AdbMessage&) = delete;
  AdbMessage& operator=(const AdbMessage&) = delete;

  ~AdbMessage();

  uint32_t command;
  uint32_t arg0;
  uint32_t arg1;
  std::string body;
};

class AndroidUsbDevice;
typedef std::vector<scoped_refptr<AndroidUsbDevice> > AndroidUsbDevices;
typedef base::OnceCallback<void(const AndroidUsbDevices&)>
    AndroidUsbDevicesCallback;

class AndroidUsbDevice : public base::RefCountedThreadSafe<AndroidUsbDevice> {
 public:
  static void Enumerate(crypto::RSAPrivateKey* rsa_key,
                        AndroidUsbDevicesCallback callback);

  AndroidUsbDevice(crypto::RSAPrivateKey* rsa_key,
                   const AndroidDeviceInfo& android_device_info,
                   mojo::Remote<device::mojom::UsbDevice> device);

  AndroidUsbDevice(const AndroidUsbDevice&) = delete;
  AndroidUsbDevice& operator=(const AndroidUsbDevice&) = delete;

  void InitOnCallerThread();

  net::StreamSocket* CreateSocket(const std::string& command);

  void Send(uint32_t command,
            uint32_t arg0,
            uint32_t arg1,
            const std::string& body);

  const std::string& serial() const { return android_device_info_.serial; }

  bool is_connected() const { return is_connected_; }

 private:
  friend class base::RefCountedThreadSafe<AndroidUsbDevice>;

  static void EnsureUsbDeviceManagerConnection();
  static void OnDeviceManagerConnectionError();

  virtual ~AndroidUsbDevice();

  void Queue(std::unique_ptr<AdbMessage> message);
  void ProcessOutgoing();
  void OutgoingMessageSent(device::mojom::UsbTransferStatus status);

  void ReadHeader();
  void ParseHeader(device::mojom::UsbTransferStatus status,
                   base::span<const uint8_t> buffer);

  void ReadBody(std::unique_ptr<AdbMessage> message,
                uint32_t data_length,
                uint32_t data_check);
  void ParseBody(std::unique_ptr<AdbMessage> message,
                 uint32_t data_length,
                 uint32_t data_check,
                 device::mojom::UsbTransferStatus status,
                 base::span<const uint8_t> buffer);

  void HandleIncoming(std::unique_ptr<AdbMessage> message);

  void TransferError(device::mojom::UsbTransferStatus status);

  void Terminate();

  void SocketDeleted(uint32_t socket_id);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::unique_ptr<crypto::RSAPrivateKey> rsa_key_;

  // Device info
  mojo::Remote<device::mojom::UsbDevice> device_;
  AndroidDeviceInfo android_device_info_;

  bool is_connected_;
  bool signature_sent_;

  // Created sockets info
  uint32_t last_socket_id_;
  using AndroidUsbSockets =
      std::map<uint32_t, raw_ptr<AndroidUsbSocket, CtnExperimental>>;
  AndroidUsbSockets sockets_;

  // Outgoing bulk queue
  using BulkMessage = scoped_refptr<base::RefCountedBytes>;
  base::queue<BulkMessage> outgoing_queue_;

  // Outgoing messages pending connect
  using PendingMessages = std::vector<std::unique_ptr<AdbMessage>>;
  PendingMessages pending_messages_;

  base::WeakPtrFactory<AndroidUsbDevice> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_USB_ANDROID_USB_DEVICE_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/usb/usb_device_provider.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/devtools/device/usb/android_rsa.h"
#include "chrome/browser/devtools/device/usb/android_usb_device.h"
#include "crypto/rsa_private_key.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

namespace {

const char kLocalAbstractCommand[] = "localabstract:%s";

const int kBufferSize = 16 * 1024;

void OnOpenSocket(const UsbDeviceProvider::SocketCallback& callback,
                  net::StreamSocket* socket_raw,
                  int result) {
  std::unique_ptr<net::StreamSocket> socket(socket_raw);
  if (result != net::OK)
    socket.reset();
  callback.Run(result, std::move(socket));
}

void OnRead(net::StreamSocket* socket,
            scoped_refptr<net::IOBuffer> buffer,
            const std::string& data,
            const UsbDeviceProvider::CommandCallback& callback,
            int result) {
  if (result <= 0) {
    callback.Run(result, result == 0 ? data : std::string());
    delete socket;
    return;
  }

  std::string new_data = data + std::string(buffer->data(), result);
  result =
      socket->Read(buffer.get(),
                   kBufferSize,
                   base::Bind(&OnRead, socket, buffer, new_data, callback));
  if (result != net::ERR_IO_PENDING)
    OnRead(socket, buffer, new_data, callback, result);
}

void OpenedForCommand(const UsbDeviceProvider::CommandCallback& callback,
                      net::StreamSocket* socket,
                      int result) {
  if (result != net::OK) {
    callback.Run(result, std::string());
    return;
  }
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kBufferSize);
  result = socket->Read(
      buffer.get(),
      kBufferSize,
      base::Bind(&OnRead, socket, buffer, std::string(), callback));
  if (result != net::ERR_IO_PENDING)
    OnRead(socket, buffer, std::string(), callback, result);
}

void RunCommand(scoped_refptr<AndroidUsbDevice> device,
                const std::string& command,
                const UsbDeviceProvider::CommandCallback& callback) {
  net::StreamSocket* socket = device->CreateSocket(command);
  if (!socket) {
    callback.Run(net::ERR_CONNECTION_FAILED, std::string());
    return;
  }
  int result = socket->Connect(
      base::Bind(&OpenedForCommand, callback, socket));
  if (result != net::ERR_IO_PENDING)
    callback.Run(result, std::string());
}

} // namespace

// static
void UsbDeviceProvider::CountDevices(
    const base::Callback<void(int)>& callback) {
  AndroidUsbDevice::CountDevices(callback);
}

UsbDeviceProvider::UsbDeviceProvider(Profile* profile){
  rsa_key_ = AndroidRSAPrivateKey(profile);
}

void UsbDeviceProvider::QueryDevices(const SerialsCallback& callback) {
  AndroidUsbDevice::Enumerate(
      rsa_key_.get(),
      base::Bind(&UsbDeviceProvider::EnumeratedDevices, this, callback));
}

void UsbDeviceProvider::QueryDeviceInfo(const std::string& serial,
                                        const DeviceInfoCallback& callback) {
  auto it = device_map_.find(serial);
  if (it == device_map_.end() || !it->second->is_connected()) {
    AndroidDeviceManager::DeviceInfo offline_info;
    callback.Run(offline_info);
    return;
  }
  AndroidDeviceManager::QueryDeviceInfo(base::Bind(&RunCommand, it->second),
                                        callback);
}

void UsbDeviceProvider::OpenSocket(const std::string& serial,
                                   const std::string& name,
                                   const SocketCallback& callback) {
  auto it = device_map_.find(serial);
  if (it == device_map_.end()) {
    callback.Run(net::ERR_CONNECTION_FAILED,
                 base::WrapUnique<net::StreamSocket>(NULL));
    return;
  }
  std::string socket_name =
      base::StringPrintf(kLocalAbstractCommand, name.c_str());
  net::StreamSocket* socket = it->second->CreateSocket(socket_name);
  if (!socket) {
    callback.Run(net::ERR_CONNECTION_FAILED,
                 base::WrapUnique<net::StreamSocket>(NULL));
    return;
  }
  int result = socket->Connect(base::Bind(&OnOpenSocket, callback, socket));
  if (result != net::ERR_IO_PENDING)
    callback.Run(result, base::WrapUnique<net::StreamSocket>(NULL));
}

void UsbDeviceProvider::ReleaseDevice(const std::string& serial) {
  device_map_.erase(serial);
}

UsbDeviceProvider::~UsbDeviceProvider() {
}

void UsbDeviceProvider::EnumeratedDevices(const SerialsCallback& callback,
                                          const AndroidUsbDevices& devices) {
  std::vector<std::string> result;
  device_map_.clear();
  for (auto it = devices.begin(); it != devices.end(); ++it) {
    result.push_back((*it)->serial());
    device_map_[(*it)->serial()] = *it;
    (*it)->InitOnCallerThread();
  }
  callback.Run(result);
}


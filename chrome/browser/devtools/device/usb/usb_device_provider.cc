// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/usb/usb_device_provider.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/devtools/device/usb/android_rsa.h"
#include "chrome/browser/devtools/device/usb/android_usb_device.h"
#include "crypto/rsa_private_key.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

namespace {

const char kLocalAbstractCommand[] = "localabstract:%s";

const int kBufferSize = 16 * 1024;

void OnOpenSocket(UsbDeviceProvider::SocketCallback callback,
                  net::StreamSocket* socket_raw,
                  int result) {
  std::unique_ptr<net::StreamSocket> socket(socket_raw);
  if (result != net::OK)
    socket.reset();
  std::move(callback).Run(result, std::move(socket));
}

void OnRead(net::StreamSocket* socket,
            scoped_refptr<net::IOBuffer> buffer,
            const std::string& data,
            UsbDeviceProvider::CommandCallback callback,
            int result) {
  if (result <= 0) {
    std::move(callback).Run(result, result == 0 ? data : std::string());
    delete socket;
    return;
  }

  std::string new_data = data + std::string(buffer->data(), result);
  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&OnRead, socket, buffer, new_data, std::move(callback)));
  result =
      socket->Read(buffer.get(), kBufferSize, std::move(split_callback.first));
  if (result != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(result);
  }
}

void OpenedForCommand(UsbDeviceProvider::CommandCallback callback,
                      net::StreamSocket* socket,
                      int result) {
  if (result != net::OK) {
    std::move(callback).Run(result, std::string());
    return;
  }
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);
  auto split_callback = base::SplitOnceCallback(base::BindOnce(
      &OnRead, socket, buffer, std::string(), std::move(callback)));
  result =
      socket->Read(buffer.get(), kBufferSize, std::move(split_callback.first));
  if (result != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(result);
  }
}

void RunCommand(scoped_refptr<AndroidUsbDevice> device,
                const std::string& command,
                UsbDeviceProvider::CommandCallback callback) {
  net::StreamSocket* socket = device->CreateSocket(command);
  if (!socket) {
    std::move(callback).Run(net::ERR_CONNECTION_FAILED, std::string());
    return;
  }
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  int result = socket->Connect(base::BindOnce(
      &OpenedForCommand, std::move(split_callback.first), socket));
  if (result != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(result, std::string());
  }
}

}  // namespace

UsbDeviceProvider::UsbDeviceProvider(Profile* profile) {
  rsa_key_ = AndroidRSAPrivateKey(profile);
}

void UsbDeviceProvider::QueryDevices(SerialsCallback callback) {
  AndroidUsbDevice::Enumerate(
      rsa_key_.get(), base::BindOnce(&UsbDeviceProvider::EnumeratedDevices,
                                     this, std::move(callback)));
}

void UsbDeviceProvider::QueryDeviceInfo(const std::string& serial,
                                        DeviceInfoCallback callback) {
  auto it = device_map_.find(serial);
  if (it == device_map_.end() || !it->second->is_connected()) {
    AndroidDeviceManager::DeviceInfo offline_info;
    std::move(callback).Run(offline_info);
    return;
  }
  AndroidDeviceManager::QueryDeviceInfo(base::BindOnce(&RunCommand, it->second),
                                        std::move(callback));
}

void UsbDeviceProvider::OpenSocket(const std::string& serial,
                                   const std::string& name,
                                   SocketCallback callback) {
  auto it = device_map_.find(serial);
  if (it == device_map_.end()) {
    std::move(callback).Run(net::ERR_CONNECTION_FAILED, nullptr);
    return;
  }
  std::string socket_name =
      base::StringPrintf(kLocalAbstractCommand, name.c_str());
  net::StreamSocket* socket = it->second->CreateSocket(socket_name);
  if (!socket) {
    std::move(callback).Run(net::ERR_CONNECTION_FAILED, nullptr);
    return;
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  int result = socket->Connect(
      base::BindOnce(&OnOpenSocket, std::move(split_callback.first), socket));
  if (result != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(result, nullptr);
  }
}

void UsbDeviceProvider::ReleaseDevice(const std::string& serial) {
  device_map_.erase(serial);
}

UsbDeviceProvider::~UsbDeviceProvider() {
}

void UsbDeviceProvider::EnumeratedDevices(SerialsCallback callback,
                                          const AndroidUsbDevices& devices) {
  std::vector<std::string> result;
  device_map_.clear();
  for (auto it = devices.begin(); it != devices.end(); ++it) {
    result.push_back((*it)->serial());
    device_map_[(*it)->serial()] = *it;
    (*it)->InitOnCallerThread();
  }
  std::move(callback).Run(std::move(result));
}

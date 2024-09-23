// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/adb/adb_device_provider.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/devtools/device/adb/adb_client_socket.h"

namespace {

const char kHostDevicesCommand[] = "host:devices";
const char kHostTransportCommand[] = "host:transport:%s|%s";
const char kLocalAbstractCommand[] = "localabstract:%s";

const int kAdbPort = 5037;

static void RunCommand(const std::string& serial,
                       const std::string& command,
                       AdbDeviceProvider::CommandCallback callback) {
  std::string query = base::StringPrintf(
      kHostTransportCommand, serial.c_str(), command.c_str());
  AdbClientSocket::AdbQuery(kAdbPort, query, std::move(callback));
}

static void ReceivedAdbDevices(AdbDeviceProvider::SerialsCallback callback,
                               int result_code,
                               const std::string& response) {
  std::vector<std::string> result;
  if (result_code < 0) {
    std::move(callback).Run(std::move(result));
    return;
  }
  for (std::string_view line : base::SplitStringPiece(
           response, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    std::vector<std::string_view> tokens = base::SplitStringPiece(
        line, "\t ", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    result.push_back(std::string(tokens[0]));
  }
  std::move(callback).Run(std::move(result));
}

} // namespace

void AdbDeviceProvider::QueryDevices(SerialsCallback callback) {
  AdbClientSocket::AdbQuery(
      kAdbPort, kHostDevicesCommand,
      base::BindOnce(&ReceivedAdbDevices, std::move(callback)));
}

void AdbDeviceProvider::QueryDeviceInfo(const std::string& serial,
                                        DeviceInfoCallback callback) {
  AndroidDeviceManager::QueryDeviceInfo(base::BindOnce(&RunCommand, serial),
                                        std::move(callback));
}

void AdbDeviceProvider::OpenSocket(const std::string& serial,
                                   const std::string& socket_name,
                                   SocketCallback callback) {
  std::string request =
      base::StringPrintf(kLocalAbstractCommand, socket_name.c_str());
  AdbClientSocket::TransportQuery(kAdbPort, serial, request,
                                  std::move(callback));
}

AdbDeviceProvider::~AdbDeviceProvider() {
}

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/smb_fs_service_provider.h"

#include <string>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromeos/components/mojo_bootstrap/pending_connection_manager.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

SmbFsServiceProvider::SmbFsServiceProvider() = default;

SmbFsServiceProvider::~SmbFsServiceProvider() = default;

void SmbFsServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      smbfs::kSmbFsInterface, smbfs::kOpenIpcChannelMethod,
      base::BindRepeating(&SmbFsServiceProvider::HandleOpenIpcChannel,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce([](const std::string& interface_name,
                        const std::string& method_name, bool success) {
        LOG_IF(ERROR, !success)
            << "Failed to export " << interface_name << "." << method_name;
      }));
}

void SmbFsServiceProvider::HandleOpenIpcChannel(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::string id;
  base::ScopedFD fd;
  dbus::MessageReader reader(method_call);
  if (!reader.PopString(&id)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "First argument is not string."));
    return;
  }
  if (!reader.PopFileDescriptor(&fd)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call,
                                                 DBUS_ERROR_INVALID_ARGS,
                                                 "Second argument is not FD."));
    return;
  }
  if (!mojo_bootstrap::PendingConnectionManager::Get().OpenIpcChannel(
          id, std::move(fd))) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                                 "Failed to open IPC"));
    return;
  }
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

}  // namespace ash

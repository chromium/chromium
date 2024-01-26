// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/vm/vm_wl_service_provider.h"

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "chrome/browser/ash/guest_os/public/guest_os_wayland_server.h"
#include "chromeos/ash/components/dbus/vm_wl/wl.pb.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/vm_wl/dbus-constants.h"

namespace ash {

namespace {

void OnExported(const std::string& interface_name,
                const std::string& method_name,
                bool success) {
  LOG_IF(ERROR, !success) << "Failed to export " << interface_name << "."
                          << method_name;
}

void Respond(dbus::MethodCall* method_call,
             dbus::ExportedObject::ResponseSender response_sender,
             std::optional<std::string> maybe_error) {
  if (maybe_error) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                                 maybe_error.value()));
    return;
  }
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

}  // namespace

VmWlServiceProvider::VmWlServiceProvider() = default;

VmWlServiceProvider::~VmWlServiceProvider() = default;

void VmWlServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      vm_tools::wl::kVmWlServiceInterface,
      vm_tools::wl::kVmWlServiveListenOnSocketMethod,
      base::BindRepeating(&VmWlServiceProvider::ListenOnSocket,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&OnExported));

  exported_object->ExportMethod(
      vm_tools::wl::kVmWlServiceInterface,
      vm_tools::wl::kVmWlServiceCloseSocketMethod,
      base::BindRepeating(&VmWlServiceProvider::CloseSocket,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&OnExported));
}

void VmWlServiceProvider::ListenOnSocket(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  vm_tools::wl::ListenOnSocketRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Unable to parse ListenOnSocketRequest from message"));
    return;
  }

  base::ScopedFD socket_fd;
  if (!reader.PopFileDescriptor(&socket_fd)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Unable to parse socket fd from message"));
    return;
  }

  guest_os::GuestOsWaylandServer::ListenOnSocket(
      request, std::move(socket_fd),
      base::BindOnce(&Respond, method_call, std::move(response_sender)));
}

void VmWlServiceProvider::CloseSocket(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  vm_tools::wl::CloseSocketRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Unable to parse CloseSocketRequest from message"));
    return;
  }

  guest_os::GuestOsWaylandServer::CloseSocket(
      request,
      base::BindOnce(&Respond, method_call, std::move(response_sender)));
}

}  // namespace ash

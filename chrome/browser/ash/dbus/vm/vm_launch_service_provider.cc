// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/vm/vm_launch_service_provider.h"

#include <dbus/dbus-protocol.h>
#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_wayland_interface.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/dbus/vm_launch/launch.pb.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/vm_launch/dbus-constants.h"

namespace ash {

namespace {
void OnExported(const std::string& interface_name,
                const std::string& method_name,
                bool success) {
  LOG_IF(ERROR, !success) << "Failed to export " << interface_name << "."
                          << method_name;
}
}  // namespace

VmLaunchServiceProvider::VmLaunchServiceProvider() = default;

VmLaunchServiceProvider::~VmLaunchServiceProvider() = default;

void VmLaunchServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      vm_tools::launch::kVmLaunchServiceInterface,
      vm_tools::launch::kVmLaunchServiceStartWaylandServerMethod,
      base::BindRepeating(&VmLaunchServiceProvider::StartWaylandServer,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&OnExported));

  exported_object->ExportMethod(
      vm_tools::launch::kVmLaunchServiceInterface,
      vm_tools::launch::kVmLaunchServiceStopWaylandServerMethod,
      base::BindRepeating(&VmLaunchServiceProvider::StopWaylandServer,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&OnExported));
}

void VmLaunchServiceProvider::StartWaylandServer(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  vm_tools::launch::StartWaylandServerRequest request;
  if (!dbus::MessageReader(method_call).PopArrayOfBytesAsProto(&request)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Unable to parse StartWaylandServerRequest from message"));
    return;
  }

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile ||
      ProfileHelper::GetUserIdHashFromProfile(profile) != request.owner_id()) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "Invalid owner_id"));
    return;
  }

  switch (request.vm_type()) {
    case vm_tools::launch::VmType::BOREALIS:
      borealis::BorealisService::GetForProfile(profile)
          ->WaylandInterface()
          .GetWaylandServer(
              base::BindOnce(&VmLaunchServiceProvider::OnWaylandServerStarted,
                             weak_ptr_factory_.GetWeakPtr(), method_call,
                             std::move(response_sender)));
      break;
    default:
      LOG(WARNING) << "StartWaylandServer is not implemented for VM type="
                   << request.vm_type() << " owner=" << request.owner_id();
      std::move(response_sender)
          .Run(dbus::ErrorResponse::FromMethodCall(
              method_call, DBUS_ERROR_NOT_SUPPORTED, "Not implemented"));
      break;
  }
}

void VmLaunchServiceProvider::OnWaylandServerStarted(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender,
    borealis::BorealisCapabilities* capabilities,
    const base::FilePath& path) {
  if (!capabilities || path.empty()) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_FAILED, "Wayland server creation failed"));
    return;
  }
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  vm_tools::launch::StartWaylandServerResponse response_pb;
  response_pb.mutable_server()->set_path(path.AsUTF8Unsafe());
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(response_pb);
  std::move(response_sender).Run(std::move(response));
}

void VmLaunchServiceProvider::StopWaylandServer(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  vm_tools::launch::StopWaylandServerRequest request;
  if (!dbus::MessageReader(method_call).PopArrayOfBytesAsProto(&request)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Unable to parse StopWaylandServerRequest from message"));
    return;
  }
  // TODO(b/200896773): Stop is not used currently as the current set of wayland
  // servers all ignore shutdown requests while their lifetimes are tied to
  // chrome itself. Going forward this method will be necessary for servers
  // whose lifetime is not bound to chrome's.
  std::move(response_sender)
      .Run(dbus::ErrorResponse::FromMethodCall(
          method_call, DBUS_ERROR_NOT_SUPPORTED, "Not implemented."));
}

}  // namespace ash

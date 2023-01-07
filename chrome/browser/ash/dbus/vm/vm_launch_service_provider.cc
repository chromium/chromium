// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/vm/vm_launch_service_provider.h"

#include <dbus/dbus-protocol.h>
#include <memory>
#include <sstream>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/guest_os/guest_os_launcher.h"
#include "chrome/browser/ash/guest_os/public/guest_os_wayland_server.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/vm_launch/launch.pb.h"
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

void OnTokenChecked(Profile* profile,
                    dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender,
                    bool launch,
                    borealis::BorealisFeatures::AllowStatus new_allowed) {
  // TODO(b/218403711): Remove these messages. These messages are shown to users
  // of the Borealis Alpha based on the status of their device, however they are
  // not translated, because this API is only a temporary measure put in place
  // until borealis' installer UX is finalized.
  if (new_allowed == borealis::BorealisFeatures::AllowStatus::kAllowed) {
    if (launch) {
      // When requested, setting the correct token should have the effect of
      // running the client app, which will bring up the installer or launch the
      // client as needed.
      borealis::BorealisService::GetForProfile(profile)->AppLauncher().Launch(
          borealis::kClientAppId, base::DoNothing());
    }
    std::unique_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());
    writer.AppendString(borealis::kInsertCoinSuccessMessage);
    std::move(response_sender).Run(std::move(response));
    return;
  }
  std::stringstream ss;
  if (new_allowed == borealis::BorealisFeatures::AllowStatus::kIncorrectToken) {
    ss << borealis::kInsertCoinRejectMessage;
  } else {
    ss << new_allowed;
  }
  std::move(response_sender)
      .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                               ss.str()));
}

template <typename T>
void HandleReturn(dbus::MethodCall* method_call,
                  dbus::ExportedObject::ResponseSender response_sender,
                  borealis::Expected<T, std::string> response) {
  if (!response) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                                 response.Error()));
    return;
  }
  std::unique_ptr<dbus::Response> dbus_response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(dbus_response.get());
  writer.AppendProtoAsArrayOfBytes(response.Value());
  std::move(response_sender).Run(std::move(dbus_response));
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

  exported_object->ExportMethod(
      vm_tools::launch::kVmLaunchServiceInterface,
      vm_tools::launch::kVmLaunchServiceProvideVmTokenMethod,
      base::BindRepeating(&VmLaunchServiceProvider::ProvideVmToken,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&OnExported));

  exported_object->ExportMethod(
      vm_tools::launch::kVmLaunchServiceInterface,
      vm_tools::launch::kVmLaunchServiceEnsureVmLaunchedMethod,
      base::BindRepeating(&VmLaunchServiceProvider::EnsureVmLaunched,
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

  guest_os::GuestOsWaylandServer::StartServer(
      request, base::BindOnce(
                   &HandleReturn<vm_tools::launch::StartWaylandServerResponse>,
                   method_call, std::move(response_sender)));
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

void VmLaunchServiceProvider::ProvideVmToken(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::string token;
  dbus::MessageReader reader(method_call);
  if (!reader.PopString(&token)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "Token not provided"));
    return;
  }

  bool launch;
  if (!reader.PopBool(&launch))
    launch = true;

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_FAILED,
            "Unable to determine primary profile"));
    return;
  }

  borealis::BorealisService::GetForProfile(profile)->Features().SetVmToken(
      token, base::BindOnce(&OnTokenChecked, profile, method_call,
                            std::move(response_sender), launch));
}

void VmLaunchServiceProvider::EnsureVmLaunched(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  vm_tools::launch::EnsureVmLaunchedRequest request;
  if (!dbus::MessageReader(method_call).PopArrayOfBytesAsProto(&request)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Unable to parse EnsureVmLaunchedRequest from message"));
    return;
  }

  guest_os::launcher::EnsureLaunched(
      request,
      base::BindOnce(&HandleReturn<vm_tools::launch::EnsureVmLaunchedResponse>,
                     method_call, std::move(response_sender)));
}

}  // namespace ash

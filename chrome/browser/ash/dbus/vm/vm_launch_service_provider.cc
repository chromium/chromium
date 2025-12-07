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
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
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

std::unique_ptr<dbus::Response> AllowStatusToResponse(
    borealis::BorealisFeatures::AllowStatus status,
    dbus::MethodCall* method_call) {
  if (status != borealis::BorealisFeatures::AllowStatus::kAllowed) {
    return dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                               "");
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendString("");
  return response;
}

void OnAllowChecked(Profile* profile,
                    dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender,
                    bool launch,
                    borealis::BorealisFeatures::AllowStatus new_allowed) {
  if (launch) {
    // When requested, setting the correct token should have the effect of
    // running the client app, which will bring up the installer or launch the
    // client as needed.
    borealis::BorealisServiceFactory::GetForProfile(profile)
        ->AppLauncher()
        .Launch(borealis::kClientAppId,
                borealis::BorealisLaunchSource::kInsertCoin, base::DoNothing());
  }
  std::move(response_sender)
      .Run(AllowStatusToResponse(new_allowed, method_call));
}

template <typename T>
void HandleReturn(dbus::MethodCall* method_call,
                  dbus::ExportedObject::ResponseSender response_sender,
                  base::expected<T, std::string> response) {
  if (!response.has_value()) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                                 response.error()));
    return;
  }
  std::unique_ptr<dbus::Response> dbus_response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(dbus_response.get());
  writer.AppendProtoAsArrayOfBytes(response.value());
  std::move(response_sender).Run(std::move(dbus_response));
}

}  // namespace

VmLaunchServiceProvider::VmLaunchServiceProvider() = default;

VmLaunchServiceProvider::~VmLaunchServiceProvider() = default;

void VmLaunchServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
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
  if (!reader.PopBool(&launch)) {
    launch = true;
  }

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_FAILED,
            "Unable to determine primary profile"));
    return;
  }

  // TODO(b/317157600): Tokens are no longer required so we have the option to
  // remove this dbus method entirely.
  borealis::BorealisServiceFactory::GetForProfile(profile)
      ->Features()
      .IsAllowed(base::BindOnce(&OnAllowChecked, profile, method_call,
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

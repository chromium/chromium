// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/vm/vm_sk_forwarding_service_provider.h"

#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"
#include "chrome/browser/ash/guest_os/vm_sk_forwarding_native_message_host.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/vm_sk_forwarding/sk_forwarding.pb.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

VmSKForwardingServiceProvider::VmSKForwardingServiceProvider() {}

VmSKForwardingServiceProvider::~VmSKForwardingServiceProvider() = default;

void VmSKForwardingServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      vm_tools::sk_forwarding::kVmSKForwardingServiceInterface,
      vm_tools::sk_forwarding::
          kVmSKForwardingServiceForwardSecurityKeyMessageMethod,
      base::BindRepeating(
          &VmSKForwardingServiceProvider::ForwardSecurityKeyMessage,
          weak_ptr_factory_.GetWeakPtr()),

      base::BindOnce(&VmSKForwardingServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VmSKForwardingServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  LOG_IF(ERROR, !success) << "Failed to export " << interface_name << "."
                          << method_name;
}

void VmSKForwardingServiceProvider::ForwardSecurityKeyMessage(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  vm_tools::sk_forwarding::ForwardSecurityKeyMessageRequest request;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse ForwardSecurityKeyMessageRequest from message";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (request.owner_id() != crostini::CryptohomeIdForProfile(profile)) {
    constexpr char error_message[] = "Primary user is not the owner of the VM";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  auto* service = ::guest_os::GuestOsServiceFactory::GetForProfile(profile);
  if (!service) {
    constexpr char error_message[] = "GuestOsService does not exist";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                                 error_message));
    return;
  }

  service->SkForwarder()->DeliverMessageToSKForwardingExtension(
      profile, request.message(),
      base::BindOnce(&VmSKForwardingServiceProvider::OnResponse,
                     weak_ptr_factory_.GetWeakPtr(), method_call,
                     std::move(response_sender)));
}

void VmSKForwardingServiceProvider::OnResponse(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender,
    const std::string& response_message) {
  std::unique_ptr<dbus::Response> dbus_response =
      dbus::Response::FromMethodCall(method_call);

  vm_tools::sk_forwarding::ForwardSecurityKeyMessageResponse response_proto;
  response_proto.set_message(response_message);

  dbus::MessageWriter writer(dbus_response.get());
  writer.AppendProtoAsArrayOfBytes(response_proto);

  std::move(response_sender).Run(std::move(dbus_response));
}

}  // namespace ash

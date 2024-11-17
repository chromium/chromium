// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/arc_crosh_service_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/mojom/crosh.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/ash/components/dbus/arc/arc.pb.h"
#include "components/user_manager/user_manager.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

ArcCroshServiceProvider::ArcCroshServiceProvider() = default;
ArcCroshServiceProvider::~ArcCroshServiceProvider() = default;

void ArcCroshServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      arc::crosh::kArcCroshInterfaceName, arc::crosh::kArcCroshRequest,
      base::BindRepeating(&ArcCroshServiceProvider::Request,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ArcCroshServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcCroshServiceProvider::Request(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  arc::ArcShellExecutionRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR)
        << "Failed to parse incoming message as ArcShellExecutionRequest";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "No ArcShellExecutionRequest in message"));
    return;
  }

  if (!request.has_command()) {
    LOG(WARNING) << "ArcShellExecutionRequest.command is empty";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "command should not be empty"));
    return;
  }

  arc::mojom::ArcShellCommand command;
  switch (request.command()) {
    case arc::ArcShellExecutionRequest_ArcShellCommand::
        ArcShellExecutionRequest_ArcShellCommand_TOP:
      command = arc::mojom::ArcShellCommand::kTop;
      break;
    case arc::ArcShellExecutionRequest_ArcShellCommand::
        ArcShellExecutionRequest_ArcShellCommand_CPUINFO:
      command = arc::mojom::ArcShellCommand::kCpuinfo;
      break;
    case arc::ArcShellExecutionRequest_ArcShellCommand::
        ArcShellExecutionRequest_ArcShellCommand_MEMINFO:
      command = arc::mojom::ArcShellCommand::kMeminfo;
      break;
    default:
      LOG(FATAL) << "Not supported shell command requested: "
                 << request.command();
  }

  arc::mojom::ArcShellExecutionRequestPtr mojo_request =
      arc::mojom::ArcShellExecutionRequest::New(command);

  if (!request.has_user_id()) {
    LOG(WARNING) << "request.user_id is empty";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "user_id should not be empty"));
    return;
  }

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  std::string requesting_user = request.user_id();
  if (requesting_user != user_manager->GetPrimaryUser()->username_hash()) {
    LOG(WARNING) << "Requesting user is not primary user";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_ACCESS_DENIED,
            "Request from not primary user is prohibited"));
    return;
  }

  arc::mojom::ArcShellExecutionInstance* mojo_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc::ArcServiceManager::Get()
                                      ->arc_bridge_service()
                                      ->arc_shell_execution(),
                                  Exec);
  if (!mojo_instance) {
    LOG(ERROR) << "Failed to get connection with ArcCroshService";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_FAILED,
            "Failed to get connection with ArcCroshService. "
            "You can make ARCVM running from starting ARC apps, "
            "e.g. Play Store"));
    return;
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  mojo_instance->Exec(
      std::move(mojo_request),
      base::BindOnce(&ArcCroshServiceProvider::SendResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(response),
                     std::move(response_sender)));
}

void ArcCroshServiceProvider::SendResponse(
    std::unique_ptr<dbus::Response> response,
    dbus::ExportedObject::ResponseSender response_sender,
    arc::mojom::ArcShellExecutionResultPtr got_mojo_result) {
  arc::ArcShellExecutionResult sending_dbus_result;
  // Only `error` field has a value if the execution fails.
  if (got_mojo_result->is_error()) {
    sending_dbus_result.set_error(got_mojo_result->get_error());
  } else {
    sending_dbus_result.set_stdout(got_mojo_result->get_stdout());
  }

  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(sending_dbus_result);
  std::move(response_sender).Run(std::move(response));
}

void ArcCroshServiceProvider::OnExported(const std::string& interface_name,
                                         const std::string& method_name,
                                         bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

}  // namespace ash

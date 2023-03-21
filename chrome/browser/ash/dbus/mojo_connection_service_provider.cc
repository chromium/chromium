// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/mojo_connection_service_provider.h"

#include <memory>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/net/rollback_network_config/rollback_network_config_service.h"
#include "chromeos/ash/services/rollback_network_config/public/mojom/rollback_network_config.mojom.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

MojoConnectionServiceProvider::MojoConnectionServiceProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

MojoConnectionServiceProvider::~MojoConnectionServiceProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MojoConnectionServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  exported_object_ = exported_object;

  exported_object_->ExportMethod(
      ::mojo_connection_service::kMojoConnectionServiceInterface,
      ::mojo_connection_service::
          kBootstrapMojoConnectionForRollbackNetworkConfigMethod,
      base::BindRepeating(&MojoConnectionServiceProvider::
                              BootstrapMojoConnectionForRollbackNetworkConfig,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&MojoConnectionServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MojoConnectionServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  LOG_IF(ERROR, !success) << "Failed to export " << interface_name << "."
                          << method_name;
}

void MojoConnectionServiceProvider::
    BootstrapMojoConnectionForRollbackNetworkConfig(
        dbus::MethodCall* method_call,
        dbus::ExportedObject::ResponseSender response_sender) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::PlatformChannel platform_channel;
  mojo::ScopedMessagePipeHandle pipe;
  SendInvitation(&platform_channel, &pipe);

  rollback_network_config::BindToInProcessInstance(
      mojo::PendingReceiver<
          rollback_network_config::mojom::RollbackNetworkConfig>(
          std::move(pipe)));

  SendResponse(std::move(platform_channel), method_call,
               std::move(response_sender));
}

void MojoConnectionServiceProvider::SendInvitation(
    mojo::PlatformChannel* platform_channel,
    mojo::ScopedMessagePipeHandle* pipe) {
  // Prepare a Mojo invitation to send through |platform_channel|.
  mojo::OutgoingInvitation invitation;
  // Include an initial Mojo pipe in the invitation.
  *pipe = invitation.AttachMessagePipe(0);
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 platform_channel->TakeLocalEndpoint());
}

void MojoConnectionServiceProvider::SendResponse(
    mojo::PlatformChannel platform_channel,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  base::ScopedFD file_handle =
      platform_channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  dbus::MessageWriter writer(response.get());
  writer.AppendFileDescriptor(file_handle.get());

  std::move(response_sender).Run(std::move(response));
}

}  // namespace ash

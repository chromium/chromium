// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_MOJO_HELPER_MOJO_HELPER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_MOJO_HELPER_MOJO_HELPER_H_

#include <memory>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace base {
class CommandLine;
}  // namespace base

namespace mojo {
class PlatformChannelEndpoint;
class IncomingInvitation;
}  // namespace mojo

namespace enterprise_connectors {

// Interface for the object in charge of performing the mojo operations that
// occur in the Management-Service.
class MojoHelper {
 public:
  virtual ~MojoHelper() = default;

  static void SetInstanceForTesting(std::unique_ptr<MojoHelper> helper);

  static std::unique_ptr<MojoHelper> Create();

  // Uses the `commmand_line` to issue a mojo API that creates a platform
  // channel. This is used to bootstrap Mojo IPC between the current process and
  // the one that sent the command.
  virtual mojo::PlatformChannelEndpoint GetEndpointFromCommandLine(
      const base::CommandLine& command_line) = 0;

  // Uses the `channel_endpoint` to issue a mojo API that accepts an incoming
  // invitation.
  virtual mojo::IncomingInvitation AcceptMojoInvitation(
      mojo::PlatformChannelEndpoint channel_endpoint) = 0;

  // Issues a mojo API that uses the `invitation` to extract the attached
  // message pipe by the `pipe_name`.
  virtual mojo::ScopedMessagePipeHandle ExtractMojoMessage(
      mojo::IncomingInvitation invitation,
      uint64_t pipe_name) = 0;

  // Uses the `pipe_handle` to creates a pending remote url loader factory.
  virtual mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreatePendingRemote(mojo::ScopedMessagePipeHandle pipe_handle) = 0;

  // Binds a `remote_url_loader_factory` to a
  // `pending_remote_url_loader_factory`.
  virtual void BindRemote(
      mojo::Remote<network::mojom::URLLoaderFactory>& remote_url_loader_factory,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_remote_url_loader_factory) = 0;

  // Returns the internal state connection of the `remote_url_loader_factory`.
  virtual bool CheckRemoteConnection(
      mojo::Remote<network::mojom::URLLoaderFactory>&
          remote_url_loader_factory) = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_MOJO_HELPER_MOJO_HELPER_H_

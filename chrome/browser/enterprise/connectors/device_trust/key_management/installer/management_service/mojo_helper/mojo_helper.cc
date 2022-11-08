// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/mojo_helper/mojo_helper.h"

#include <utility>

#include "base/command_line.h"
#include "base/no_destructor.h"

#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace enterprise_connectors {

namespace {

std::unique_ptr<MojoHelper>* GetTestInstanceStorage() {
  static base::NoDestructor<std::unique_ptr<MojoHelper>> storage;
  return storage.get();
}

}  // namespace

class MojoHelperImpl : public MojoHelper {
 public:
  MojoHelperImpl();
  ~MojoHelperImpl() override;

  // MojoHelper:
  mojo::PlatformChannelEndpoint GetEndpointFromCommandLine(
      const base::CommandLine& command_line) override;
  mojo::IncomingInvitation AcceptMojoInvitation(
      mojo::PlatformChannelEndpoint channel_endpoint) override;
  mojo::ScopedMessagePipeHandle ExtractMojoMessage(
      mojo::IncomingInvitation invitation,
      uint64_t pipe_name) override;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> CreatePendingRemote(
      mojo::ScopedMessagePipeHandle pipe_handle) override;
  void BindRemote(
      mojo::Remote<network::mojom::URLLoaderFactory>& remote_url_loader_factory,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_remote_url_loader_factory) override;
  bool CheckRemoteConnection(mojo::Remote<network::mojom::URLLoaderFactory>&
                                 remote_url_loader_factory) override;
};

// static
void MojoHelper::SetInstanceForTesting(std::unique_ptr<MojoHelper> helper) {
  DCHECK(helper);
  *GetTestInstanceStorage() = std::move(helper);
}

// static
std::unique_ptr<MojoHelper> MojoHelper::Create() {
  std::unique_ptr<MojoHelper>& test_instance = *GetTestInstanceStorage();
  if (test_instance)
    return std::move(test_instance);
  return std::make_unique<MojoHelperImpl>();
}

MojoHelperImpl::MojoHelperImpl() = default;
MojoHelperImpl::~MojoHelperImpl() = default;

mojo::PlatformChannelEndpoint MojoHelperImpl::GetEndpointFromCommandLine(
    const base::CommandLine& command_line) {
  return mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
      command_line);
}

mojo::IncomingInvitation MojoHelperImpl::AcceptMojoInvitation(
    mojo::PlatformChannelEndpoint channel_endpoint) {
  return mojo::IncomingInvitation::Accept(std::move(channel_endpoint));
}

mojo::ScopedMessagePipeHandle MojoHelperImpl::ExtractMojoMessage(
    mojo::IncomingInvitation invitation,
    uint64_t pipe_name) {
  return invitation.ExtractMessagePipe(pipe_name);
}
mojo::PendingRemote<network::mojom::URLLoaderFactory>
MojoHelperImpl::CreatePendingRemote(mojo::ScopedMessagePipeHandle pipe_handle) {
  return mojo::PendingRemote<network::mojom::URLLoaderFactory>(
      std::move(pipe_handle), 0);
}

void MojoHelperImpl::BindRemote(
    mojo::Remote<network::mojom::URLLoaderFactory>& remote_url_loader_factory,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_remote_url_loader_factory) {
  remote_url_loader_factory.Bind(std::move(pending_remote_url_loader_factory));
}

bool MojoHelperImpl::CheckRemoteConnection(
    mojo::Remote<network::mojom::URLLoaderFactory>& remote_url_loader_factory) {
  return remote_url_loader_factory.is_connected();
}

}  // namespace enterprise_connectors

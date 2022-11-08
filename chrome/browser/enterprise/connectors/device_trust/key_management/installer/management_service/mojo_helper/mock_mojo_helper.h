// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_MOJO_HELPER_MOCK_MOJO_HELPER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_MOJO_HELPER_MOCK_MOJO_HELPER_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/mojo_helper/mojo_helper.h"

#include "base/command_line.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class CommandLine;
}  // namespace base

namespace mojo {
class PlatformChannelEndpoint;
class IncomingInvitation;
}  // namespace mojo

namespace enterprise_connectors::test {

// Mocked implementation of the MojoHelper interface.
class MockMojoHelper : public MojoHelper {
 public:
  MockMojoHelper();
  ~MockMojoHelper() override;

  MOCK_METHOD(mojo::PlatformChannelEndpoint,
              GetEndpointFromCommandLine,
              (const base::CommandLine&),
              (override));
  MOCK_METHOD(mojo::IncomingInvitation,
              AcceptMojoInvitation,
              (mojo::PlatformChannelEndpoint),
              (override));
  MOCK_METHOD(mojo::ScopedMessagePipeHandle,
              ExtractMojoMessage,
              (mojo::IncomingInvitation, uint64_t),
              (override));
  MOCK_METHOD(mojo::PendingRemote<network::mojom::URLLoaderFactory>,
              CreatePendingRemote,
              (mojo::ScopedMessagePipeHandle),
              (override));
  MOCK_METHOD(void,
              BindRemote,
              (mojo::Remote<network::mojom::URLLoaderFactory>&,
               mojo::PendingRemote<network::mojom::URLLoaderFactory>),
              (override));
  MOCK_METHOD(bool,
              CheckRemoteConnection,
              (mojo::Remote<network::mojom::URLLoaderFactory>&),
              (override));
};

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_MOJO_HELPER_MOCK_MOJO_HELPER_H_

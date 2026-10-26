// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/mach_bootstrap_acceptor.h"

#include <bsm/libbsm.h>

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "chrome/app_shim/app_shim_controller.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class MachBootstrapAcceptorTest : public testing::Test {
 public:
  // Friend accessors:
  static mojo::PlatformChannelEndpoint ConnectToBrowser(
      const mojo::NamedPlatformChannel::ServerName& server_name) {
    return AppShimController::ConnectToBrowser(server_name);
  }

  static mach_port_t GetAcceptorPort(MachBootstrapAcceptor* acceptor) {
    return acceptor->port();
  }

  static const mojo::NamedPlatformChannel::ServerName& GetAcceptorServerName(
      MachBootstrapAcceptor* acceptor) {
    return acceptor->server_name_;
  }

  static void StopAcceptorDispatchSource(MachBootstrapAcceptor* acceptor) {
    acceptor->dispatch_source_.reset();
  }

  // Mach bootstrap names live in a host-global namespace, so use a unique name
  // per test run to avoid collisions when repeated test instances run
  // concurrently across parallel worker processes (e.g. `--gtest_repeat` in CQ
  // flakiness checks).
  static std::string CreateUniqueNameFragment() {
    return base::UnguessableToken::Create().ToString();
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
};

class TestMachBootstrapAcceptorDelegate
    : public MachBootstrapAcceptor::Delegate {
 public:
  explicit TestMachBootstrapAcceptorDelegate(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  void OnClientConnected(mojo::PlatformChannelEndpoint endpoint,
                         audit_token_t audit_token) override {
    endpoint_ = std::move(endpoint);
    pid_ = audit_token_to_pid(audit_token);
    std::move(quit_closure_).Run();
  }

  void OnServerChannelCreateError() override {
    error_ = true;
    std::move(quit_closure_).Run();
  }

  base::ProcessId pid() const { return pid_; }
  const mojo::PlatformChannelEndpoint& endpoint() const { return endpoint_; }
  void ResetEndpoint() { endpoint_.reset(); }
  bool error() const { return error_; }

 private:
  base::ProcessId pid_ = base::kNullProcessId;
  mojo::PlatformChannelEndpoint endpoint_;

  bool error_ = false;

  base::OnceClosure quit_closure_;
};

TEST_F(MachBootstrapAcceptorTest, SingleRequest) {
  base::RunLoop run_loop;
  TestMachBootstrapAcceptorDelegate delegate(run_loop.QuitClosure());
  MachBootstrapAcceptor acceptor(CreateUniqueNameFragment(), &delegate);
  acceptor.Start();

  EXPECT_TRUE(GetAcceptorPort(&acceptor) != MACH_PORT_NULL);

  mojo::PlatformChannelEndpoint endpoint =
      ConnectToBrowser(GetAcceptorServerName(&acceptor));

  run_loop.Run();

  EXPECT_FALSE(delegate.error());
  EXPECT_EQ(base::GetCurrentProcId(), delegate.pid());
  // In the same process, the send and receive rights are known by the same
  // Mach port name.
  EXPECT_EQ(endpoint.platform_handle().GetMachReceiveRight().get(),
            delegate.endpoint().platform_handle().GetMachSendRight().get());
}

TEST_F(MachBootstrapAcceptorTest, FailToRegister) {
  base::RunLoop run_loop;
  TestMachBootstrapAcceptorDelegate delegate(run_loop.QuitClosure());
  MachBootstrapAcceptor acceptor(CreateUniqueNameFragment(), &delegate);

  mojo::NamedPlatformChannel::ServerName server_name =
      GetAcceptorServerName(&acceptor);

  // Squat on the server name in the bootstrap server.
  mojo::NamedPlatformChannel::Options server_options;
  server_options.server_name = server_name;
  mojo::NamedPlatformChannel server_endpoint =
      mojo::NamedPlatformChannel(server_options);

  // The acceptor will fail to start and reports an error.
  acceptor.Start();

  run_loop.Run();

  EXPECT_TRUE(delegate.error());
}

// Tests that if the MachBootstrapAcceptor is stopped while a client's
// ConnectToBrowser message is still queued in its port, the client's
// IsolatedConnection observes peer closure.
TEST_F(MachBootstrapAcceptorTest, AcceptorStoppedWithQueuedRequest) {
  TestMachBootstrapAcceptorDelegate delegate{base::DoNothing()};
  MachBootstrapAcceptor acceptor(CreateUniqueNameFragment(), &delegate);
  acceptor.Start();
  ASSERT_FALSE(delegate.error());
  // Stop the dispatch source so the message stays queued in the acceptor's
  // port until `Stop()` destroys it (simulating `AppShimListener` being
  // destroyed during shutdown before `HandleRequest()` runs).
  StopAcceptorDispatchSource(&acceptor);

  mojo::PlatformChannelEndpoint endpoint =
      ConnectToBrowser(GetAcceptorServerName(&acceptor));
  ASSERT_TRUE(endpoint.is_valid());

  mojo::IsolatedConnection connection;
  mojo::Remote<chrome::mojom::AppShimHostBootstrap> host_bootstrap(
      mojo::PendingRemote<chrome::mojom::AppShimHostBootstrap>(
          connection.Connect(std::move(endpoint)), 0));

  base::RunLoop disconnect_loop;
  host_bootstrap.set_disconnect_handler(disconnect_loop.QuitClosure());

  // Stop the acceptor before its dispatch source drains the Mach port,
  // destroying the queued message carrying the peer's send right.
  acceptor.Stop();

  disconnect_loop.Run();
}

// Tests that if the MachBootstrapAcceptor receives the client's endpoint in
// OnClientConnected but drops it without connecting a Mojo channel, the
// client's IsolatedConnection observes peer closure.
TEST_F(MachBootstrapAcceptorTest, EndpointDroppedWithoutConnecting) {
  base::RunLoop connected_loop;
  TestMachBootstrapAcceptorDelegate delegate(connected_loop.QuitClosure());
  MachBootstrapAcceptor acceptor(CreateUniqueNameFragment(), &delegate);
  acceptor.Start();
  ASSERT_FALSE(delegate.error());

  mojo::PlatformChannelEndpoint endpoint =
      ConnectToBrowser(GetAcceptorServerName(&acceptor));
  ASSERT_TRUE(endpoint.is_valid());

  mojo::IsolatedConnection connection;
  mojo::Remote<chrome::mojom::AppShimHostBootstrap> host_bootstrap(
      mojo::PendingRemote<chrome::mojom::AppShimHostBootstrap>(
          connection.Connect(std::move(endpoint)), 0));

  base::RunLoop disconnect_loop;
  host_bootstrap.set_disconnect_handler(disconnect_loop.QuitClosure());

  connected_loop.Run();
  // Drop the server's send right without ever starting a Mojo channel on it.
  delegate.ResetEndpoint();

  disconnect_loop.Run();
}

}  // namespace apps

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/mach_bootstrap_acceptor.h"

#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/app_shim/app_shim_controller.h"
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

 private:
  base::test::TaskEnvironment task_environment_;
};

class TestMachBootstrapAcceptorDelegate
    : public MachBootstrapAcceptor::Delegate {
 public:
  explicit TestMachBootstrapAcceptorDelegate(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  void OnClientConnected(mojo::PlatformChannelEndpoint endpoint,
                         base::ProcessId pid) override {
    endpoint_ = std::move(endpoint);
    pid_ = pid;
    std::move(quit_closure_).Run();
  }

  void OnServerChannelCreateError() override {
    error_ = true;
    std::move(quit_closure_).Run();
  }

  base::ProcessId pid() const { return pid_; }
  const mojo::PlatformChannelEndpoint& endpoint() const { return endpoint_; }
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
  MachBootstrapAcceptor acceptor("simplereq", &delegate);
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
  MachBootstrapAcceptor acceptor("failtoreg", &delegate);

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

}  // namespace apps

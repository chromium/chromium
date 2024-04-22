// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/firewall_hole/nearby_connections_firewall_hole_factory.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/nearby/public/cpp/tcp_server_socket_port.h"
#include "chromeos/dbus/permission_broker/fake_permission_broker_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

uint16_t kPort = ash::nearby::TcpServerSocketPort::kMin + 1;

void MoveFirewallHole(
    base::RunLoop* run_loop,
    mojo::PendingRemote<::sharing::mojom::FirewallHole>* out_hole,
    mojo::PendingRemote<::sharing::mojom::FirewallHole> hole) {
  *out_hole = std::move(hole);
  run_loop->Quit();
}

class NearbyConnectionsFirewallHoleFactoryTest : public testing::Test {
 public:
  NearbyConnectionsFirewallHoleFactoryTest()
      : port_(*ash::nearby::TcpServerSocketPort::FromUInt16(kPort)),
        task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}
  ~NearbyConnectionsFirewallHoleFactoryTest() override = default;

  void SetUp() override { chromeos::PermissionBrokerClient::InitializeFake(); }

  void TearDown() override { chromeos::PermissionBrokerClient::Shutdown(); }

  const ash::nearby::TcpServerSocketPort port_;
  NearbyConnectionsFirewallHoleFactory factory_;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(NearbyConnectionsFirewallHoleFactoryTest, Success) {
  base::RunLoop run_loop;
  mojo::PendingRemote<::sharing::mojom::FirewallHole> hole;
  factory_.OpenFirewallHole(
      port_, base::BindOnce(&MoveFirewallHole, &run_loop, &hole));
  run_loop.Run();

  EXPECT_TRUE(hole);
  EXPECT_TRUE(chromeos::FakePermissionBrokerClient::Get()->HasTcpHole(
      kPort, /*interface=*/std::string()));

  // Destroy the PendingRemote, which will destroy the underlying FirewallHole.
  hole.reset();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(chromeos::FakePermissionBrokerClient::Get()->HasTcpHole(
      kPort, /*interface=*/std::string()));
}

TEST_F(NearbyConnectionsFirewallHoleFactoryTest, Failure) {
  chromeos::FakePermissionBrokerClient::Get()->AddTcpDenyRule(
      kPort,
      /*interface=*/std::string());

  base::RunLoop run_loop;
  mojo::PendingRemote<::sharing::mojom::FirewallHole> hole;
  factory_.OpenFirewallHole(
      port_, base::BindOnce(&MoveFirewallHole, &run_loop, &hole));
  run_loop.Run();
  EXPECT_FALSE(hole);
}

}  // namespace

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/api/socket/udp_socket.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/test_completion_callback.h"
#include "services/network/network_context.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class UDPSocketUnitTest : public extensions::ExtensionServiceTestBase {
 protected:
  // extensions::ExtensionServiceTestBase:
  void SetUp() override { InitializeEmptyExtensionService(); }

  std::unique_ptr<UDPSocket> CreateSocket() {
    network::mojom::NetworkContext* network_context =
        content::BrowserContext::GetDefaultStoragePartition(profile())
            ->GetNetworkContext();
    mojo::PendingRemote<network::mojom::UDPSocket> socket;
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener_remote;
    mojo::PendingReceiver<network::mojom::UDPSocketListener> listener_receiver =
        listener_remote.InitWithNewPipeAndPassReceiver();
    network_context->CreateUDPSocket(socket.InitWithNewPipeAndPassReceiver(),
                                     std::move(listener_remote));
    return std::make_unique<UDPSocket>(std::move(socket),
                                       std::move(listener_receiver),
                                       "abcdefghijklmnopqrst");
  }
};

static void OnConnected(int result) {
  EXPECT_EQ(0, result);
}

static void OnCompleted(int bytes_read,
                        scoped_refptr<net::IOBuffer> io_buffer,
                        bool socket_destroying,
                        const std::string& address,
                        uint16_t port) {
  // Do nothing; don't care.
}

static const char kTestMessage[] = "$$TESTMESSAGETESTMESSAGETESTMESSAGETEST$$";
static const int kTestMessageLength = base::size(kTestMessage);

net::AddressList CreateAddressList(const char* address_string, int port) {
  net::IPAddress ip;
  EXPECT_TRUE(ip.AssignFromIPLiteral(address_string));
  return net::AddressList::CreateFromIPAddress(ip, port);
}

static void OnSendCompleted(int result) {
  EXPECT_EQ(kTestMessageLength, result);
}

TEST_F(UDPSocketUnitTest, TestUDPSocketRecvFrom) {
  std::unique_ptr<UDPSocket> socket = CreateSocket();

  // Confirm that we can call two RecvFroms in quick succession without
  // triggering crbug.com/146606.
  socket->Connect(CreateAddressList("127.0.0.1", 40000),
                  base::BindOnce(&OnConnected));
  socket->RecvFrom(4096, base::BindOnce(&OnCompleted));
  socket->RecvFrom(4096, base::BindOnce(&OnCompleted));
}

TEST_F(UDPSocketUnitTest, TestUDPMulticastJoinGroup) {
  const char kGroup[] = "237.132.100.17";
  std::unique_ptr<UDPSocket> src = CreateSocket();
  std::unique_ptr<UDPSocket> dest = CreateSocket();

  {
    net::TestCompletionCallback callback;
    dest->Bind("0.0.0.0", 13333, callback.callback());
    EXPECT_EQ(net::OK, callback.WaitForResult());
  }
  {
    net::TestCompletionCallback callback;
    dest->JoinGroup(kGroup, callback.callback());
    EXPECT_EQ(net::OK, callback.WaitForResult());
  }
  std::vector<std::string> groups = dest->GetJoinedGroups();
  EXPECT_EQ(static_cast<size_t>(1), groups.size());
  EXPECT_EQ(kGroup, *groups.begin());
  {
    net::TestCompletionCallback callback;
    dest->LeaveGroup("237.132.100.13", callback.callback());
    EXPECT_NE(net::OK, callback.WaitForResult());
  }
  {
    net::TestCompletionCallback callback;
    dest->LeaveGroup(kGroup, callback.callback());
    EXPECT_EQ(net::OK, callback.WaitForResult());
  }
  groups = dest->GetJoinedGroups();
  EXPECT_EQ(static_cast<size_t>(0), groups.size());
}

TEST_F(UDPSocketUnitTest, TestUDPMulticastTimeToLive) {
  const char kGroup[] = "237.132.100.17";
  std::unique_ptr<UDPSocket> socket = CreateSocket();

  EXPECT_NE(0, socket->SetMulticastTimeToLive(-1));  // Negative TTL shall fail.
  EXPECT_EQ(0, socket->SetMulticastTimeToLive(3));
  socket->Connect(CreateAddressList(kGroup, 13333),
                  base::BindOnce(&OnConnected));
}

TEST_F(UDPSocketUnitTest, TestUDPMulticastLoopbackMode) {
  const char kGroup[] = "237.132.100.17";
  std::unique_ptr<UDPSocket> socket = CreateSocket();

  EXPECT_EQ(0, socket->SetMulticastLoopbackMode(false));
  socket->Connect(CreateAddressList(kGroup, 13333),
                  base::BindOnce(&OnConnected));
}

// Send a test multicast packet every second.
// Once the target socket received the packet, the message loop will exit.
static void SendMulticastPacket(const base::Closure& quit_run_loop,
                                UDPSocket* src,
                                int result) {
  if (result == 0) {
    scoped_refptr<net::IOBuffer> data =
        base::MakeRefCounted<net::WrappedIOBuffer>(kTestMessage);
    src->Write(data, kTestMessageLength, base::BindOnce(&OnSendCompleted));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SendMulticastPacket, quit_run_loop, src, result),
        base::TimeDelta::FromSeconds(1));
  } else {
    quit_run_loop.Run();
    FAIL() << "Failed to connect to multicast address. Error code: " << result;
  }
}

static void OnMulticastReadCompleted(const base::Closure& quit_run_loop,
                                     bool* packet_received,
                                     int count,
                                     scoped_refptr<net::IOBuffer> io_buffer,
                                     bool socket_destroying,
                                     const std::string& ip,
                                     uint16_t port) {
  EXPECT_EQ(kTestMessageLength, count);
  EXPECT_EQ(0, strncmp(io_buffer->data(), kTestMessage, kTestMessageLength));
  *packet_received = true;
  quit_run_loop.Run();
}

TEST_F(UDPSocketUnitTest, TestUDPMulticastRecv) {
  const int kPort = 9999;
  const char kGroup[] = "237.132.100.17";
  bool packet_received = false;
  std::unique_ptr<UDPSocket> src = CreateSocket();
  std::unique_ptr<UDPSocket> dest = CreateSocket();

  // Listener
  {
    net::TestCompletionCallback callback;
    dest->Bind("0.0.0.0", kPort, callback.callback());
    EXPECT_EQ(net::OK, callback.WaitForResult());
  }
  {
    net::TestCompletionCallback callback;
    dest->JoinGroup(kGroup, callback.callback());
    EXPECT_EQ(net::OK, callback.WaitForResult());
  }
  base::RunLoop run_loop;
  // |dest| is used with Bind(), so use RecvFrom() instead of Read().
  dest->RecvFrom(
      1024, base::BindOnce(&OnMulticastReadCompleted, run_loop.QuitClosure(),
                           &packet_received));

  // Sender
  EXPECT_EQ(0, src->SetMulticastTimeToLive(0));
  src->Connect(
      CreateAddressList(kGroup, kPort),
      base::BindOnce(&SendMulticastPacket, run_loop.QuitClosure(), src.get()));

  // If not received within the test action timeout, quit the message loop.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());

  run_loop.Run();

  EXPECT_TRUE(packet_received) << "Failed to receive from multicast address";
}

}  // namespace extensions

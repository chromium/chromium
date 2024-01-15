// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_storage_partition.h"
#include "extensions/browser/api/socket/tcp_socket.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_context.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace extensions {

namespace {

const char kTestMsg[] = "abcdefghij";
const int kTestMsgLength = strlen(kTestMsg);

const char FAKE_ID[] = "abcdefghijklmnopqrst";

using AcceptFuture = base::test::TestFuture<
    int32_t,
    mojo::PendingRemote<network::mojom::TCPConnectedSocket>,
    const std::optional<net::IPEndPoint>&,
    mojo::ScopedDataPipeConsumerHandle,
    mojo::ScopedDataPipeProducerHandle>;

using BoolFuture = base::test::TestFuture<bool>;

using ConnectFuture = base::test::TestFuture<int32_t>;

using ListenFuture = base::test::TestFuture<int32_t, const std::string&>;

using ReadFuture =
    base::test::TestFuture<int32_t, scoped_refptr<net::IOBuffer>, bool>;

using WriteFuture = base::test::TestFuture<int32_t>;

class TCPSocketUnitTestBase : public extensions::ExtensionServiceTestBase {
 public:
  TCPSocketUnitTestBase()
      : url_request_context_builder_(
            net::CreateTestURLRequestContextBuilder()) {}
  ~TCPSocketUnitTestBase() override = default;

  std::unique_ptr<TCPSocket> CreateSocket() {
    auto socket = std::make_unique<TCPSocket>(&profile_, FAKE_ID);
    socket->SetStoragePartitionForTest(&partition_);
    return socket;
  }

  std::unique_ptr<TCPSocket> CreateAndConnectSocketWithAddress(
      const net::IPEndPoint& ip_end_point) {
    auto socket = CreateSocket();
    net::AddressList address(ip_end_point);
    ConnectFuture connect_future;
    socket->Connect(address, connect_future.GetCallback());
    EXPECT_EQ(net::OK, connect_future.Get());
    return socket;
  }

  std::unique_ptr<TCPSocket> CreateAndConnectSocket() {
    net::IPEndPoint ip_end_point(net::IPAddress::IPv4Localhost(), 1234);
    return CreateAndConnectSocketWithAddress(ip_end_point);
  }

  // Reads data from |socket| and compares it with |expected_data|.
  void ReadData(Socket* socket, const std::string& expected_data) {
    std::string received_data;
    const int count = 512;
    while (true) {
      ReadFuture read_future;
      socket->Read(count, read_future.GetCallback());
      auto [net_error, io_buffer, socket_destroying] = read_future.Take();
      EXPECT_FALSE(socket_destroying);
      if (net_error > 0) {
        received_data.append(io_buffer->data(), net_error);
      } else {
        break;
      }
    }
    EXPECT_EQ(expected_data, received_data);
  }

 protected:
  // extensions::ExtensionServiceTestBase implementation.
  void SetUp() override { InitializeEmptyExtensionService(); }

  void Initialize() {
    url_request_context_ = url_request_context_builder_->Build();
    network_context_ = std::make_unique<network::NetworkContext>(
        nullptr, network_context_remote_.BindNewPipeAndPassReceiver(),
        url_request_context_.get(),
        /*cors_exempt_header_list=*/std::vector<std::string>());
    partition_.set_network_context(network_context_remote_.get());
  }

  std::unique_ptr<net::URLRequestContextBuilder> url_request_context_builder_;

 private:
  TestingProfile profile_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  content::TestStoragePartition partition_;
  std::unique_ptr<network::NetworkContext> network_context_;
  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;
};

}  // namespace

class TCPSocketUnitTest : public TCPSocketUnitTestBase,
                          public ::testing::WithParamInterface<net::IoMode> {
 public:
  TCPSocketUnitTest() {
    mock_client_socket_factory_.set_enable_read_if_ready(true);
    url_request_context_builder_->set_client_socket_factory_for_testing(
        &mock_client_socket_factory_);
    Initialize();
  }
  ~TCPSocketUnitTest() override = default;

  net::MockClientSocketFactory* mock_client_socket_factory() {
    return &mock_client_socket_factory_;
  }

 private:
  net::MockClientSocketFactory mock_client_socket_factory_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         TCPSocketUnitTest,
                         testing::Values(net::SYNCHRONOUS, net::ASYNC));

TEST_F(TCPSocketUnitTest, SocketConnectError) {
  net::IPEndPoint ip_end_point(net::IPAddress::IPv4Localhost(), 1234);
  net::StaticSocketDataProvider data_provider((base::span<net::MockRead>()),
                                              base::span<net::MockWrite>());
  data_provider.set_connect_data(
      net::MockConnect(net::ASYNC, net::ERR_FAILED, ip_end_point));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  std::unique_ptr<TCPSocket> socket = CreateSocket();

  ConnectFuture connect_future;
  socket->Connect(net::AddressList(std::move(ip_end_point)),
                  connect_future.GetCallback());
  EXPECT_EQ(net::ERR_FAILED, connect_future.Get());
}

TEST_P(TCPSocketUnitTest, SocketConnectAfterDisconnect) {
  net::IoMode io_mode = GetParam();
  const net::MockRead kReads[] = {net::MockRead(io_mode, net::OK)};
  net::StaticSocketDataProvider data_provider1(kReads,
                                               base::span<net::MockWrite>());
  net::StaticSocketDataProvider data_provider2(kReads,
                                               base::span<net::MockWrite>());
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider1);
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider2);
  std::unique_ptr<TCPSocket> socket = CreateAndConnectSocket();
  socket->Disconnect(false /* socket_destroying */);

  ConnectFuture connect_future;
  net::IPEndPoint ip_end_point(net::IPAddress::IPv4Localhost(), 1234);
  socket->Connect(net::AddressList(std::move(ip_end_point)),
                  connect_future.GetCallback());
  EXPECT_EQ(net::OK, connect_future.Get());

  EXPECT_TRUE(data_provider1.AllReadDataConsumed());
  EXPECT_TRUE(data_provider1.AllWriteDataConsumed());
  EXPECT_TRUE(data_provider2.AllReadDataConsumed());
  EXPECT_TRUE(data_provider2.AllWriteDataConsumed());
}

TEST_F(TCPSocketUnitTest, SocketConnectDisconnectRace) {
  // Regression test for https://crbug.com/882585, disconnect while connect
  // is pending.
  net::IPEndPoint ip_end_point(net::IPAddress::IPv4Localhost(), 1234);
  net::StaticSocketDataProvider data_provider((base::span<net::MockRead>()),
                                              base::span<net::MockWrite>());
  data_provider.set_connect_data(
      net::MockConnect(net::SYNCHRONOUS, net::ERR_FAILED, ip_end_point));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  std::unique_ptr<TCPSocket> socket = CreateSocket();

  net::AddressList address(ip_end_point);
  ConnectFuture connect_future;
  socket->Connect(address, connect_future.GetCallback());
  socket->Disconnect(false /* socket_destroying */);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(connect_future.IsReady());
}

TEST_F(TCPSocketUnitTest, DestroyWhileReadPending) {
  const net::MockRead kReads[] = {
      net::MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING)};
  net::StaticSocketDataProvider data_provider(kReads,
                                              base::span<net::MockWrite>());
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  std::unique_ptr<TCPSocket> socket = CreateAndConnectSocket();

  // Read one byte, and it should be pending.
  ReadFuture read_future;
  socket->Read(/*count=*/1, read_future.GetCallback());
  // Destroy socket.
  socket = nullptr;
  // Wait for read callback.
  auto [net_error, io_buffer, socket_destroying] = read_future.Take();
  // |socket_destroying| should correctly denote that this
  // read callback is invoked through the destructor of
  // TCPSocket.
  EXPECT_TRUE(socket_destroying);
  EXPECT_EQ(net::ERR_CONNECTION_CLOSED, net_error);
}

TEST_P(TCPSocketUnitTest, Read) {
  net::IoMode io_mode = GetParam();
  const net::MockRead kReads[] = {
      net::MockRead(io_mode, kTestMsg, kTestMsgLength),
      net::MockRead(io_mode, net::OK)};
  net::StaticSocketDataProvider data_provider(kReads,
                                              base::span<net::MockWrite>());

  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  std::unique_ptr<TCPSocket> socket = CreateAndConnectSocket();

  ReadData(socket.get(), kTestMsg);
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// Tests the case where a message is split over two separate socket reads.
TEST_P(TCPSocketUnitTest, SocketMultipleRead) {
  const char kFirstHalfTestMsg[] = "abcde";
  const char kSecondHalfTestMsg[] = "fghij";
  EXPECT_EQ(kTestMsg, std::string(kFirstHalfTestMsg) + kSecondHalfTestMsg);
  net::IoMode io_mode = GetParam();
  const net::MockRead kReads[] = {
      net::MockRead(io_mode, kFirstHalfTestMsg, strlen(kFirstHalfTestMsg)),
      net::MockRead(io_mode, kSecondHalfTestMsg, strlen(kSecondHalfTestMsg)),
      net::MockRead(io_mode, net::OK)};
  net::StaticSocketDataProvider data_provider(kReads,
                                              base::span<net::MockWrite>());

  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  std::unique_ptr<TCPSocket> socket = CreateAndConnectSocket();

  ReadData(socket.get(), kTestMsg);
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// Tests the case where read size is smaller than the actual message.
TEST_P(TCPSocketUnitTest, SocketPartialRead) {
  net::IoMode io_mode = GetParam();
  const net::MockRead kReads[] = {
      net::MockRead(io_mode, kTestMsg, kTestMsgLength),
      net::MockRead(io_mode, net::OK)};
  net::StaticSocketDataProvider data_provider(kReads,
                                              base::span<net::MockWrite>());
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  std::unique_ptr<TCPSocket> socket = CreateAndConnectSocket();

  int count = 1;
  std::string received_data;
  while (true) {
    ReadFuture read_future;
    socket->Read(count, read_future.GetCallback());
    auto [bytes_read, io_buffer, socket_destroying] = read_future.Take();
    EXPECT_FALSE(socket_destroying);
    if (bytes_read > 0) {
      received_data.append(io_buffer->data(), bytes_read);
    } else {
      break;
    }
    // Double the read size in the next iteration.
    count *= 2;
  }
  EXPECT_EQ(kTestMsg, received_data);
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(TCPSocketUnitTest, ReadError) {
  net::IoMode io_mode = GetParam();
  const net::MockRead kReads[] = {
      net::MockRead(io_mode, kTestMsg, kTestMsgLength),
      net::MockRead(io_mode, net::ERR_INSUFFICIENT_RESOURCES)};
  net::StaticSocketDataProvider data_provider(kReads,
                                              base::span<net::MockWrite>());
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  std::unique_ptr<TCPSocket> socket = CreateAndConnectSocket();

  int net_error_out = net::OK;
  while (true) {
    ReadFuture read_future;
    socket->Read(/*count=*/512, read_future.GetCallback());
    auto [net_error, io_buffer, socket_destroying] = read_future.Take();
    EXPECT_FALSE(socket_destroying);
    if (net_error <= 0) {
      EXPECT_FALSE(socket->IsConnected());
      EXPECT_EQ(nullptr, io_buffer);
      net_error_out = net_error;
      break;
    } else {
      EXPECT_TRUE(socket->IsConnected());
    }
  }
  // Note that TCPSocket only detects that receive pipe is broken and propagates
  // it as 0 byte read. It doesn't know the specific net error code. To know the
  // specific net error code, it needs to register itself as a
  // network::mojom::SocketObserver. However, that gets tricky because of two
  // separate mojo pipes.
  EXPECT_EQ(0, net_error_out);
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(TCPSocketUnitTest, Write) {
  net::IoMode io_mode = GetParam();
  const net::MockRead kReads[] = {
      net::MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(io_mode, kTestMsg, kTestMsgLength)};

  net::StaticSocketDataProvider data_provider(kReads, kWrites);

  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  std::unique_ptr<TCPSocket> socket = CreateAndConnectSocket();

  auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kTestMsg);
  WriteFuture write_future;
  socket->Write(io_buffer.get(), kTestMsgLength, write_future.GetCallback());
  EXPECT_EQ(kTestMsgLength, write_future.Get());
}

// Tests the case where a message is split over two separate socket writes.
TEST_P(TCPSocketUnitTest, MultipleWrite) {
  const char kFirstHalfTestMsg[] = "abcde";
  const char kSecondHalfTestMsg[] = "fghij";
  EXPECT_EQ(kTestMsg, std::string(kFirstHalfTestMsg) + kSecondHalfTestMsg);
  net::IoMode io_mode = GetParam();
  const net::MockRead kReads[] = {
      net::MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(io_mode, kFirstHalfTestMsg, strlen(kFirstHalfTestMsg)),
      net::MockWrite(io_mode, kSecondHalfTestMsg, strlen(kSecondHalfTestMsg))};

  net::StaticSocketDataProvider data_provider(kReads, kWrites);
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  std::unique_ptr<TCPSocket> socket = CreateAndConnectSocket();

  int num_bytes_written = 0;
  auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kTestMsg);
  auto drainable_io_buffer = base::MakeRefCounted<net::DrainableIOBuffer>(
      io_buffer.get(), kTestMsgLength);
  while (num_bytes_written < kTestMsgLength) {
    WriteFuture write_future;
    socket->Write(drainable_io_buffer.get(), kTestMsgLength - num_bytes_written,
                  write_future.GetCallback());
    int result = write_future.Get();
    ASSERT_GT(result, net::OK);
    drainable_io_buffer->DidConsume(result);
    num_bytes_written += result;
    // Flushes the write.
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(TCPSocketUnitTest, PartialWrite) {
  net::IoMode io_mode = GetParam();
  const net::MockRead kReads[] = {
      net::MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(io_mode, "a", 1), net::MockWrite(io_mode, "bc", 2),
      net::MockWrite(io_mode, "defg", 4), net::MockWrite(io_mode, "hij", 3)};

  net::StaticSocketDataProvider data_provider(kReads, kWrites);

  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  std::unique_ptr<TCPSocket> socket = CreateAndConnectSocket();

  // Start with writing one byte, and double that in the next iteration.
  int num_bytes_to_write = 1;
  int num_bytes_written = 0;
  auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kTestMsg);
  auto drainable_io_buffer = base::MakeRefCounted<net::DrainableIOBuffer>(
      io_buffer.get(), kTestMsgLength);
  while (num_bytes_written < kTestMsgLength) {
    WriteFuture write_future;
    socket->Write(
        drainable_io_buffer.get(),
        std::max(kTestMsgLength - num_bytes_written, num_bytes_to_write),
        write_future.GetCallback());
    int bytes_written = write_future.Get();
    ASSERT_GT(bytes_written, 0);
    drainable_io_buffer->DidConsume(bytes_written);
    num_bytes_written += bytes_written;
    num_bytes_to_write *= 2;
    // Flushes the write.
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(TCPSocketUnitTest, WriteError) {
  net::IoMode io_mode = GetParam();
  const net::MockRead kReads[] = {
      net::MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(io_mode, net::ERR_INSUFFICIENT_RESOURCES)};

  net::StaticSocketDataProvider data_provider(kReads, kWrites);

  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  std::unique_ptr<TCPSocket> socket = CreateAndConnectSocket();

  // Mojo data pipe might buffer some write data, so continue writing until the
  // write error is received.
  auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kTestMsg);
  int net_error = net::OK;
  while (true) {
    WriteFuture write_future;
    socket->Write(io_buffer.get(), kTestMsgLength, write_future.GetCallback());
    auto bytes_written = write_future.Get();
    EXPECT_EQ(bytes_written > 0, socket->IsConnected());
    if (bytes_written <= 0) {
      net_error = bytes_written;
      break;
    }
  }
  // Note that TCPSocket only detects that send pipe is broken and propagates
  // it as a net::ERR_FAILED. It doesn't know the specific net error code. To do
  // that, it needs to register itself as a network::mojom::SocketObserver.
  EXPECT_EQ(net::ERR_FAILED, net_error);
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

namespace {

class ExtensionsMockClientSocket : public net::MockTCPClientSocket {
 public:
  ExtensionsMockClientSocket(net::SocketDataProvider* provider, bool success)
      : MockTCPClientSocket(
            net::AddressList(
                net::IPEndPoint(net::IPAddress::IPv4Localhost(), 1234)),
            nullptr /* netlog */,
            provider),
        success_(success) {
    set_enable_read_if_ready(true);
  }

  ExtensionsMockClientSocket(const ExtensionsMockClientSocket&) = delete;
  ExtensionsMockClientSocket& operator=(const ExtensionsMockClientSocket&) =
      delete;

  ~ExtensionsMockClientSocket() override = default;

  bool SetNoDelay(bool no_delay) override { return success_; }
  bool SetKeepAlive(bool enable, int delay) override { return success_; }

 private:
  // Whether to return success for SetNoDelay() and SetKeepAlive().
  const bool success_;
};

static const net::MockRead kMockReads[] = {net::MockRead(net::ASYNC, net::OK)};

// A ClientSocketFactory to create sockets that simulate SetNoDelay and
// SetKeepAlive success and failures.
class TestSocketFactory : public net::ClientSocketFactory {
 public:
  explicit TestSocketFactory(bool success) : success_(success) {}

  TestSocketFactory(const TestSocketFactory&) = delete;
  TestSocketFactory& operator=(const TestSocketFactory&) = delete;

  ~TestSocketFactory() override = default;

  std::unique_ptr<net::DatagramClientSocket> CreateDatagramClientSocket(
      net::DatagramSocket::BindType,
      net::NetLog*,
      const net::NetLogSource&) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  std::unique_ptr<net::TransportClientSocket> CreateTransportClientSocket(
      const net::AddressList&,
      std::unique_ptr<net::SocketPerformanceWatcher>,
      net::NetworkQualityEstimator* network_quality_estimator,
      net::NetLog*,
      const net::NetLogSource&) override {
    providers_.push_back(std::make_unique<net::StaticSocketDataProvider>(
        kMockReads, base::span<net::MockWrite>()));
    return std::make_unique<ExtensionsMockClientSocket>(providers_.back().get(),
                                                        success_);
  }
  std::unique_ptr<net::SSLClientSocket> CreateSSLClientSocket(
      net::SSLClientContext*,
      std::unique_ptr<net::StreamSocket>,
      const net::HostPortPair&,
      const net::SSLConfig&) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

 private:
  std::vector<std::unique_ptr<net::StaticSocketDataProvider>> providers_;
  // Whether to return success for net::TransportClientSocket::SetNoDelay() and
  // SetKeepAlive().
  const bool success_;
};

}  // namespace

class TCPSocketSettingsTest : public TCPSocketUnitTestBase,
                              public ::testing::WithParamInterface<bool> {
 public:
  TCPSocketSettingsTest() : client_socket_factory_(GetParam()) {
    url_request_context_builder_->set_client_socket_factory_for_testing(
        &client_socket_factory_);
    Initialize();
  }
  ~TCPSocketSettingsTest() override = default;

 private:
  TestSocketFactory client_socket_factory_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         TCPSocketSettingsTest,
                         testing::Bool());

TEST_P(TCPSocketSettingsTest, SetNoDelay) {
  std::unique_ptr<TCPSocket> socket = CreateAndConnectSocket();
  bool expected_success = GetParam();
  {
    BoolFuture no_delay_future;
    socket->SetNoDelay(true, no_delay_future.GetCallback());
    EXPECT_EQ(expected_success, no_delay_future.Get());
  }

  {
    BoolFuture no_delay_future;
    socket->SetNoDelay(false, no_delay_future.GetCallback());
    EXPECT_EQ(expected_success, no_delay_future.Get());
  }
}

TEST_P(TCPSocketSettingsTest, SetKeepAlive) {
  std::unique_ptr<TCPSocket> socket = CreateAndConnectSocket();
  bool expected_success = GetParam();
  {
    BoolFuture keep_alive_future;
    socket->SetKeepAlive(/*enable=*/true, /*delay=*/123,
                         keep_alive_future.GetCallback());
    EXPECT_EQ(expected_success, keep_alive_future.Get());
  }

  {
    BoolFuture keep_alive_future;
    socket->SetKeepAlive(/*enable=*/false, /*delay=*/123,
                         keep_alive_future.GetCallback());
    EXPECT_EQ(expected_success, keep_alive_future.Get());
  }
}

class TCPSocketServerTest : public TCPSocketUnitTestBase {
 public:
  TCPSocketServerTest() { Initialize(); }
  ~TCPSocketServerTest() override = default;

 private:
  net::MockClientSocketFactory mock_client_socket_factory_;
};

TEST_F(TCPSocketServerTest, ListenAccept) {
  // Create a server socket.
  std::unique_ptr<TCPSocket> socket = CreateSocket();
  {
    ListenFuture listen_future;
    socket->Listen("127.0.0.1", 0 /* port */, 1 /* backlog */,
                   listen_future.GetCallback());
    EXPECT_EQ(net::OK, listen_future.Get<int32_t>());
  }

  net::IPEndPoint server_addr;
  EXPECT_TRUE(socket->GetLocalAddress(&server_addr));

  AcceptFuture accept_future;
  socket->Accept(accept_future.GetCallback());
  // Create a client socket to talk to the server socket.
  auto client_socket = CreateAndConnectSocketWithAddress(server_addr);
  auto [result, connected_socket, accept_client_addr, receive_handle,
        send_handle] = accept_future.Take();

  EXPECT_EQ(result, net::OK);
  net::IPEndPoint peer_addr;
  EXPECT_TRUE(client_socket->GetPeerAddress(&peer_addr));
  net::IPEndPoint client_addr;
  EXPECT_TRUE(client_socket->GetLocalAddress(&client_addr));
  EXPECT_EQ(server_addr, peer_addr);
  EXPECT_EQ(client_addr, accept_client_addr);
}

TEST_F(TCPSocketServerTest, ListenDisconnectRace) {
  // Create a server socket.
  std::unique_ptr<TCPSocket> socket = CreateSocket();
  bool callback_ran = false;
  socket->Listen(
      "127.0.0.1", 0 /* port */, 1 /* backlog */,
      base::BindLambdaForTesting([&](int result, const std::string& error_msg) {
        callback_ran = true;
      }));
  socket->Disconnect(false /* socket_destroying */);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_ran);
}

TEST_F(TCPSocketServerTest, ReadAndWrite) {
  // Create a server socket.
  std::unique_ptr<TCPSocket> socket = CreateSocket();
  {
    ListenFuture listen_future;
    socket->Listen("127.0.0.1", 0 /* port */, 1 /* backlog */,
                   listen_future.GetCallback());
    EXPECT_EQ(net::OK, listen_future.Get<int32_t>());
  }
  net::IPEndPoint server_addr;
  EXPECT_TRUE(socket->GetLocalAddress(&server_addr));

  AcceptFuture accept_future;
  socket->Accept(accept_future.GetCallback());

  // Create a client socket to talk to the server socket.
  auto client_socket = CreateAndConnectSocketWithAddress(server_addr);

  auto [result, connected_socket, remote_addr, receive_handle, send_handle] =
      accept_future.Take();
  EXPECT_EQ(net::OK, result);
  auto accepted_socket = std::make_unique<TCPSocket>(
      std::move(connected_socket), std::move(receive_handle),
      std::move(send_handle), remote_addr, FAKE_ID);

  // Send data from the client to the server.
  auto io_buffer_write = base::MakeRefCounted<net::StringIOBuffer>(kTestMsg);
  {
    WriteFuture write_future;
    client_socket->Write(io_buffer_write.get(), kTestMsgLength,
                         write_future.GetCallback());
    EXPECT_EQ(kTestMsgLength, write_future.Get());
  }

  std::string received_contents;
  while (received_contents.size() < kTestMsgLength) {
    ReadFuture read_future;
    accepted_socket->Read(kTestMsgLength, read_future.GetCallback());
    auto [bytes_read, io_buffer_read, socket_destroying] = read_future.Take();
    ASSERT_GT(bytes_read, 0);
    EXPECT_FALSE(socket_destroying);
    received_contents.append(std::string(io_buffer_read->data(), bytes_read));
  }
  EXPECT_EQ(kTestMsg, received_contents);

  // Send data from the server to the client.
  {
    WriteFuture write_future;
    accepted_socket->Write(io_buffer_write.get(), kTestMsgLength,
                           write_future.GetCallback());
    EXPECT_EQ(kTestMsgLength, write_future.Get());
  }

  std::string sent_contents;
  while (sent_contents.size() < kTestMsgLength) {
    ReadFuture read_future;
    client_socket->Read(kTestMsgLength, read_future.GetCallback());
    auto [bytes_read, io_buffer_read, socket_destroying] = read_future.Take();
    ASSERT_GT(bytes_read, 0);
    EXPECT_FALSE(socket_destroying);
    sent_contents.append(std::string(io_buffer_read->data(), bytes_read));
  }
  EXPECT_EQ(kTestMsg, sent_contents);
}

}  // namespace extensions

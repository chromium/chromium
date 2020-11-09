// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_storage_partition.h"
#include "extensions/browser/api/socket/tls_socket.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_context.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace extensions {

namespace {

const char kTestMsg[] = "abcdefghij";
const int kTestMsgLength = strlen(kTestMsg);

const char FAKE_ID[] = "abcdefghijklmnopqrst";

class TLSSocketTestBase : public extensions::ExtensionServiceTestBase {
 public:
  TLSSocketTestBase() : url_request_context_(true) {}
  ~TLSSocketTestBase() override {}

  // Creates a TCP socket.
  std::unique_ptr<TCPSocket> CreateTCPSocket() {
    auto socket = std::make_unique<TCPSocket>(&profile_, FAKE_ID);
    socket->SetStoragePartitionForTest(&partition_);
    net::TestCompletionCallback connect_callback;
    net::IPEndPoint ip_end_point(net::IPAddress::IPv4Localhost(), kPort);
    socket->Connect(net::AddressList(ip_end_point),
                    connect_callback.callback());
    if (net::OK != connect_callback.WaitForResult()) {
      return nullptr;
    }
    return socket;
  }

  // Create a TCP socket and upgrade it to TLS.
  std::unique_ptr<TLSSocket> CreateSocket() {
    auto socket = CreateTCPSocket();
    if (!socket)
      return nullptr;
    base::RunLoop run_loop;
    net::HostPortPair host_port_pair("example.com", kPort);
    net::IPEndPoint local_addr;
    net::IPEndPoint peer_addr;
    if (!socket->GetLocalAddress(&local_addr) ||
        !socket->GetPeerAddress(&peer_addr)) {
      return nullptr;
    }
    std::unique_ptr<TLSSocket> tls_socket;
    socket->UpgradeToTLS(
        nullptr /* options */,
        base::BindLambdaForTesting(
            [&](int result,
                mojo::PendingRemote<network::mojom::TLSClientSocket>
                    pending_tls_socket,
                const net::IPEndPoint& local_addr,
                const net::IPEndPoint& peer_addr,
                mojo::ScopedDataPipeConsumerHandle receive_handle,
                mojo::ScopedDataPipeProducerHandle send_handle) {
              if (net::OK == result) {
                tls_socket = std::make_unique<TLSSocket>(
                    std::move(pending_tls_socket), local_addr, peer_addr,
                    std::move(receive_handle), std::move(send_handle), FAKE_ID);
              }
              run_loop.Quit();
            }));
    run_loop.Run();
    return tls_socket;
  }

 protected:
  // extensions::ExtensionServiceTestBase implementation.
  void SetUp() override { InitializeEmptyExtensionService(); }

  void Initialize() {
    url_request_context_.Init();
    network_context_ = std::make_unique<network::NetworkContext>(
        nullptr, network_context_remote_.BindNewPipeAndPassReceiver(),
        &url_request_context_,
        /*cors_exempt_header_list=*/std::vector<std::string>());
    partition_.set_network_context(network_context_remote_.get());
  }

  net::TestURLRequestContext url_request_context_;

 private:
  static const int kPort = 1234;
  TestingProfile profile_;
  content::TestStoragePartition partition_;
  std::unique_ptr<network::NetworkContext> network_context_;
  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;
};

}  // namespace

class TLSSocketTest : public TLSSocketTestBase,
                      public ::testing::WithParamInterface<net::IoMode> {
 public:
  TLSSocketTest() : TLSSocketTestBase() {
    mock_client_socket_factory_.set_enable_read_if_ready(true);
    url_request_context_.set_client_socket_factory(
        &mock_client_socket_factory_);
    Initialize();
  }
  ~TLSSocketTest() override {}

  net::MockClientSocketFactory* mock_client_socket_factory() {
    return &mock_client_socket_factory_;
  }

 private:
  net::MockClientSocketFactory mock_client_socket_factory_;
};

TEST_F(TLSSocketTest, DestroyWhileReadPending) {
  const net::MockRead kReads[] = {net::MockRead(net::SYNCHRONOUS, net::OK, 1)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::ASYNC, kTestMsg, kTestMsgLength, 0)};
  net::StaticSocketDataProvider data_provider(kReads, kWrites);
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  std::unique_ptr<TLSSocket> socket = CreateSocket();

  int net_result = net::ERR_FAILED;
  base::RunLoop run_loop;
  int count = 1;
  // Read one byte, and it should be pending because it is blocked on the mock
  // write.
  socket->Read(count,
               base::BindLambdaForTesting(
                   [&](int result, scoped_refptr<net::IOBuffer> io_buffer,
                       bool socket_destroying) {
                     net_result = result;
                     // |socket_destroying| should correctly denote that this
                     // read callback is invoked through the destructor of
                     // TLSSocket.
                     EXPECT_TRUE(socket_destroying);
                     run_loop.Quit();
                   }));
  // Destroy socket.
  socket = nullptr;
  // Wait for read callback.
  run_loop.Run();
  EXPECT_EQ(net::ERR_CONNECTION_CLOSED, net_result);
}

// UpgradeToTLS() fails when there is a pending read.
TEST_F(TLSSocketTest, UpgradeToTLSWhilePendingRead) {
  const net::MockRead kReads[] = {
      net::MockRead(net::ASYNC, net::ERR_IO_PENDING)};
  net::StaticSocketDataProvider data_provider(kReads,
                                              base::span<net::MockWrite>());
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  auto socket = CreateTCPSocket();
  // This read will be pending when UpgradeToTLS() is called.
  socket->Read(1 /* count */, base::DoNothing());
  base::RunLoop run_loop;
  socket->UpgradeToTLS(
      nullptr /* options */,
      base::BindLambdaForTesting(
          [&](int result,
              mojo::PendingRemote<network::mojom::TLSClientSocket> tls_socket,
              const net::IPEndPoint& local_addr,
              const net::IPEndPoint& peer_addr,
              mojo::ScopedDataPipeConsumerHandle receive_handle,
              mojo::ScopedDataPipeProducerHandle send_handle) {
            EXPECT_EQ(net::ERR_FAILED, result);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(TLSSocketTest, UpgradeToTLSWithCustomOptions) {
  // Mock data are not consumed. These are here so that net::StreamSocket::Read
  // is always pending and blocked on the write. Otherwise, mock socket data
  // will complains that there aren't any data to read.
  const net::MockRead kReads[] = {
      net::MockRead(net::ASYNC, kTestMsg, kTestMsgLength, 1),
      net::MockRead(net::ASYNC, net::OK, 2)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::ASYNC, kTestMsg, kTestMsgLength, 0)};
  net::SequencedSocketData data_provider(kReads, kWrites);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  ssl_socket.expected_ssl_version_min = net::SSL_PROTOCOL_VERSION_TLS1_1;
  ssl_socket.expected_ssl_version_max = net::SSL_PROTOCOL_VERSION_TLS1_2;
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  auto socket = CreateTCPSocket();
  api::socket::SecureOptions options;
  options.tls_version = std::make_unique<api::socket::TLSVersionConstraints>();
  options.tls_version->min = std::make_unique<std::string>("tls1.1");
  options.tls_version->max = std::make_unique<std::string>("tls1.2");
  int net_error = net::ERR_FAILED;
  base::RunLoop run_loop;
  socket->UpgradeToTLS(
      &options,
      base::BindLambdaForTesting(
          [&](int result,
              mojo::PendingRemote<network::mojom::TLSClientSocket> tls_socket,
              const net::IPEndPoint& local_addr,
              const net::IPEndPoint& peer_addr,
              mojo::ScopedDataPipeConsumerHandle receive_handle,
              mojo::ScopedDataPipeProducerHandle send_handle) {
            net_error = result;
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_EQ(net::OK, net_error);
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
}

// Test the API can parse "tls1.3".
TEST_F(TLSSocketTest, UpgradeToTLSWithCustomOptionsTLS13) {
  // Mock data are not consumed. These are here so that net::StreamSocket::Read
  // is always pending and blocked on the write. Otherwise, mock socket data
  // will complains that there aren't any data to read.
  const net::MockRead kReads[] = {
      net::MockRead(net::ASYNC, kTestMsg, kTestMsgLength, 1),
      net::MockRead(net::ASYNC, net::OK, 2)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::ASYNC, kTestMsg, kTestMsgLength, 0)};
  net::SequencedSocketData data_provider(kReads, kWrites);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  ssl_socket.expected_ssl_version_min = net::SSL_PROTOCOL_VERSION_TLS1_3;
  ssl_socket.expected_ssl_version_max = net::SSL_PROTOCOL_VERSION_TLS1_3;
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  auto socket = CreateTCPSocket();
  api::socket::SecureOptions options;
  options.tls_version = std::make_unique<api::socket::TLSVersionConstraints>();
  options.tls_version->min = std::make_unique<std::string>("tls1.3");
  options.tls_version->max = std::make_unique<std::string>("tls1.3");
  int net_error = net::ERR_FAILED;
  base::RunLoop run_loop;
  socket->UpgradeToTLS(
      &options,
      base::BindLambdaForTesting(
          [&](int result,
              mojo::PendingRemote<network::mojom::TLSClientSocket> tls_socket,
              const net::IPEndPoint& local_addr,
              const net::IPEndPoint& peer_addr,
              mojo::ScopedDataPipeConsumerHandle receive_handle,
              mojo::ScopedDataPipeProducerHandle send_handle) {
            net_error = result;
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_EQ(net::OK, net_error);
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
}

INSTANTIATE_TEST_SUITE_P(All,
                         TLSSocketTest,
                         testing::Values(net::SYNCHRONOUS, net::ASYNC));

TEST_P(TLSSocketTest, ReadWrite) {
  net::IoMode io_mode = GetParam();
  const net::MockRead kReads[] = {
      net::MockRead(net::ASYNC, kTestMsg, kTestMsgLength, 1),
      net::MockRead(io_mode, net::OK, 2)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, kTestMsg, kTestMsgLength, 0)};
  net::SequencedSocketData data_provider(kReads, kWrites);
  net::SSLSocketDataProvider ssl_socket(io_mode, net::OK);
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);
  std::unique_ptr<TLSSocket> socket = CreateSocket();

  auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kTestMsg);
  net::TestCompletionCallback write_callback;
  socket->Write(io_buffer.get(), kTestMsgLength, write_callback.callback());
  EXPECT_EQ(kTestMsgLength, write_callback.WaitForResult());

  std::string received_data;
  int count = 512;
  while (true) {
    base::RunLoop run_loop;
    int net_error = net::ERR_FAILED;
    socket->Read(count,
                 base::BindLambdaForTesting(
                     [&](int result, scoped_refptr<net::IOBuffer> io_buffer,
                         bool socket_destroying) {
                       net_error = result;
                       EXPECT_FALSE(socket_destroying);
                       if (result > 0)
                         received_data.append(io_buffer->data(), result);
                       run_loop.Quit();
                     }));
    run_loop.Run();
    if (net_error <= 0)
      break;
  }
  EXPECT_EQ(kTestMsg, received_data);
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
}

// Tests the case where read size is smaller than the actual message.
TEST_P(TLSSocketTest, PartialRead) {
  net::IoMode io_mode = GetParam();
  const net::MockRead kReads[] = {
      net::MockRead(net::ASYNC, kTestMsg, kTestMsgLength, 1),
      net::MockRead(io_mode, net::OK, 2)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, kTestMsg, kTestMsgLength, 0)};
  net::SequencedSocketData data_provider(kReads, kWrites);
  net::SSLSocketDataProvider ssl_socket(io_mode, net::OK);
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);
  std::unique_ptr<TLSSocket> socket = CreateSocket();

  auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kTestMsg);
  net::TestCompletionCallback write_callback;
  socket->Write(io_buffer.get(), kTestMsgLength, write_callback.callback());
  EXPECT_EQ(kTestMsgLength, write_callback.WaitForResult());

  int count = 1;
  std::string received_data;
  while (true) {
    int net_result = net::ERR_FAILED;
    base::RunLoop run_loop;
    socket->Read(count,
                 base::BindLambdaForTesting(
                     [&](int result, scoped_refptr<net::IOBuffer> io_buffer,
                         bool socket_destroying) {
                       net_result = result;
                       EXPECT_FALSE(socket_destroying);
                       if (result > 0)
                         received_data.append(io_buffer->data(), result);
                       run_loop.Quit();
                     }));
    run_loop.Run();
    if (net_result <= 0)
      break;
    // Double the read size in the next iteration.
    count *= 2;
  }
  EXPECT_EQ(kTestMsg, received_data);
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
}

TEST_P(TLSSocketTest, ReadError) {
  net::IoMode io_mode = GetParam();
  const net::MockRead kReads[] = {
      net::MockRead(net::ASYNC, net::ERR_INSUFFICIENT_RESOURCES, 1)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, kTestMsg, kTestMsgLength, 0)};
  net::SequencedSocketData data_provider(kReads, kWrites);
  net::SSLSocketDataProvider ssl_socket(io_mode, net::OK);
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  std::unique_ptr<TLSSocket> socket = CreateSocket();

  auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kTestMsg);
  net::TestCompletionCallback write_callback;
  socket->Write(io_buffer.get(), kTestMsgLength, write_callback.callback());
  EXPECT_EQ(kTestMsgLength, write_callback.WaitForResult());

  const int count = 512;
  int net_error = net::OK;
  while (true) {
    base::RunLoop run_loop;
    socket->Read(count,
                 base::BindLambdaForTesting(
                     [&](int result, scoped_refptr<net::IOBuffer> io_buffer,
                         bool socket_destroying) {
                       net_error = result;
                       EXPECT_FALSE(socket_destroying);
                       if (result <= 0) {
                         EXPECT_FALSE(socket->IsConnected());
                         EXPECT_EQ(nullptr, io_buffer);
                       } else {
                         EXPECT_TRUE(socket->IsConnected());
                       }
                       run_loop.Quit();
                     }));
    run_loop.Run();
    if (net_error <= 0)
      break;
  }
  // Note that TLSSocket only detects that receive pipe is broken and propagates
  // it as 0 byte read. It doesn't know the specific net error code. To know the
  // specific net error code, it needs to register itself as a
  // network::mojom::SocketObserver. However, that gets tricky because of two
  // separate mojo pipes.
  EXPECT_EQ(0, net_error);
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
}

// Tests the case where a message is split over two separate socket writes.
TEST_P(TLSSocketTest, MultipleWrite) {
  const char kFirstHalfTestMsg[] = "abcde";
  const char kSecondHalfTestMsg[] = "fghij";
  EXPECT_EQ(kTestMsg, std::string(kFirstHalfTestMsg) + kSecondHalfTestMsg);
  net::IoMode io_mode = GetParam();
  const net::MockRead kReads[] = {net::MockRead(net::ASYNC, net::OK, 2)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(io_mode, kFirstHalfTestMsg, strlen(kFirstHalfTestMsg), 0),
      net::MockWrite(io_mode, kSecondHalfTestMsg, strlen(kSecondHalfTestMsg),
                     1)};
  net::SequencedSocketData data_provider(kReads, kWrites);
  net::SSLSocketDataProvider ssl_socket(io_mode, net::OK);
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);
  std::unique_ptr<TLSSocket> socket = CreateSocket();

  int num_bytes_written = 0;
  auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kTestMsg);
  auto drainable_io_buffer = base::MakeRefCounted<net::DrainableIOBuffer>(
      io_buffer.get(), kTestMsgLength);
  while (num_bytes_written < kTestMsgLength) {
    net::TestCompletionCallback write_callback;
    socket->Write(drainable_io_buffer.get(), kTestMsgLength - num_bytes_written,
                  write_callback.callback());
    int result = write_callback.WaitForResult();
    ASSERT_GT(result, net::OK);
    drainable_io_buffer->DidConsume(result);
    num_bytes_written += result;
    // Flushes the write.
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
}

TEST_P(TLSSocketTest, PartialWrite) {
  net::IoMode io_mode = GetParam();
  const net::MockRead kReads[] = {net::MockRead(net::ASYNC, net::OK, 4)};
  const net::MockWrite kWrites[] = {net::MockWrite(io_mode, "a", 1, 0),
                                    net::MockWrite(io_mode, "bc", 2, 1),
                                    net::MockWrite(io_mode, "defg", 4, 2),
                                    net::MockWrite(io_mode, "hij", 3, 3)};

  net::SequencedSocketData data_provider(kReads, kWrites);
  net::SSLSocketDataProvider ssl_socket(io_mode, net::OK);
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  std::unique_ptr<TLSSocket> socket = CreateSocket();

  // Start with writing one byte, and double that in the next iteration.
  int num_bytes_to_write = 1;
  int num_bytes_written = 0;
  auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kTestMsg);
  auto drainable_io_buffer = base::MakeRefCounted<net::DrainableIOBuffer>(
      io_buffer.get(), kTestMsgLength);
  while (num_bytes_written < kTestMsgLength) {
    net::TestCompletionCallback write_callback;
    socket->Write(
        drainable_io_buffer.get(),
        std::max(kTestMsgLength - num_bytes_written, num_bytes_to_write),
        write_callback.callback());
    int result = write_callback.WaitForResult();
    ASSERT_GT(result, net::OK);
    drainable_io_buffer->DidConsume(result);
    num_bytes_written += result;
    num_bytes_to_write *= 2;
    // Flushes the write.
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
}

TEST_P(TLSSocketTest, WriteError) {
  net::IoMode io_mode = GetParam();
  const net::MockRead kReads[] = {net::MockRead(net::ASYNC, net::OK, 1)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(io_mode, net::ERR_INSUFFICIENT_RESOURCES, 0)};

  net::SequencedSocketData data_provider(kReads, kWrites);
  net::SSLSocketDataProvider ssl_socket(io_mode, net::OK);
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);
  std::unique_ptr<TLSSocket> socket = CreateSocket();

  // Mojo data pipe might buffer some write data, so continue writing until the
  // write error is received.
  auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kTestMsg);
  int net_error = net::OK;
  while (true) {
    base::RunLoop run_loop;
    socket->Write(io_buffer.get(), kTestMsgLength,
                  base::BindLambdaForTesting([&](int result) {
                    if (result == net::ERR_FAILED)
                      EXPECT_FALSE(socket->IsConnected());
                    net_error = result;
                    run_loop.Quit();
                  }));
    run_loop.Run();
    if (net_error <= 0)
      break;
  }
  // Note that TCPSocket only detects that send pipe is broken and propagates
  // it as a net::ERR_FAILED. It doesn't know the specific net error code. To do
  // that, it needs to register itself as a network::mojom::SocketObserver.
  EXPECT_EQ(net::ERR_FAILED, net_error);
}

}  // namespace extensions

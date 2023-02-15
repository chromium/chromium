// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/test/bind.h"
#include "base/test/test_future.h"
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
#include "net/socket/socket_test_util.h"
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

using ConnectFuture = base::test::TestFuture<int32_t>;

using ReadFuture =
    base::test::TestFuture<int, scoped_refptr<net::IOBuffer>, bool>;

using UpgradeToTLSFuture =
    base::test::TestFuture<int,
                           mojo::PendingRemote<network::mojom::TLSClientSocket>,
                           const net::IPEndPoint&,
                           const net::IPEndPoint&,
                           mojo::ScopedDataPipeConsumerHandle,
                           mojo::ScopedDataPipeProducerHandle>;

using WriteFuture = base::test::TestFuture<int32_t>;

class TLSSocketTestBase : public extensions::ExtensionServiceTestBase {
 public:
  TLSSocketTestBase()
      : url_request_context_builder_(
            net::CreateTestURLRequestContextBuilder()) {}
  ~TLSSocketTestBase() override = default;

  // Creates a TCP socket.
  std::unique_ptr<TCPSocket> CreateTCPSocket() {
    auto socket = std::make_unique<TCPSocket>(&profile_, FAKE_ID);
    socket->SetStoragePartitionForTest(&partition_);
    net::IPEndPoint ip_end_point(net::IPAddress::IPv4Localhost(), kPort);
    ConnectFuture connect_future;
    socket->Connect(net::AddressList(std::move(ip_end_point)),
                    connect_future.GetCallback());
    EXPECT_EQ(connect_future.Get(), net::OK);
    return socket;
  }

  // Create a TCP socket and upgrade it to TLS.
  std::unique_ptr<TLSSocket> CreateSocket() {
    auto socket = CreateTCPSocket();
    {
      net::IPEndPoint local_addr;
      net::IPEndPoint peer_addr;
      if (!socket->GetLocalAddress(&local_addr) ||
          !socket->GetPeerAddress(&peer_addr)) {
        return nullptr;
      }
    }
    UpgradeToTLSFuture upgrade_future;
    socket->UpgradeToTLS(/*options=*/nullptr, upgrade_future.GetCallback());
    auto [net_error, tls_socket, local_addr, peer_addr, receive_handle,
          send_handle] = upgrade_future.Take();
    EXPECT_EQ(net_error, net::OK);
    return std::make_unique<TLSSocket>(std::move(tls_socket), local_addr,
                                       peer_addr, std::move(receive_handle),
                                       std::move(send_handle), FAKE_ID);
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
  static const int kPort = 1234;
  TestingProfile profile_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  content::TestStoragePartition partition_;
  std::unique_ptr<network::NetworkContext> network_context_;
  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;
};

}  // namespace

class TLSSocketTest : public TLSSocketTestBase,
                      public ::testing::WithParamInterface<net::IoMode> {
 public:
  TLSSocketTest() {
    mock_client_socket_factory_.set_enable_read_if_ready(true);
    url_request_context_builder_->set_client_socket_factory_for_testing(
        &mock_client_socket_factory_);
    Initialize();
  }
  ~TLSSocketTest() override = default;

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

  // Read one byte, and it should be pending because it is blocked on the mock
  // write.
  ReadFuture read_future;
  socket->Read(/*count=*/1, read_future.GetCallback());
  // Destroy socket.

  socket = nullptr;
  // Wait for read callback.
  auto [net_error, io_buffer, socket_destroying] = read_future.Take();
  // |socket_destroying| should correctly denote that this
  // read callback is invoked through the destructor of
  // TLSSocket.
  EXPECT_TRUE(socket_destroying);
  EXPECT_EQ(net::ERR_CONNECTION_CLOSED, net_error);
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
  socket->Read(/*count=*/1, base::DoNothing());

  UpgradeToTLSFuture upgrade_future;
  socket->UpgradeToTLS(/*options=*/nullptr, upgrade_future.GetCallback());
  auto [net_error, tls_socket, local_addr, peer_addr, receive_handle,
        send_handle] = upgrade_future.Take();
  EXPECT_EQ(net_error, net::ERR_FAILED);
}

TEST_F(TLSSocketTest, UpgradeToTLSWithCustomOptionsTLS12) {
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
  ssl_socket.expected_ssl_version_min = net::SSL_PROTOCOL_VERSION_TLS1_2;
  ssl_socket.expected_ssl_version_max = net::SSL_PROTOCOL_VERSION_TLS1_2;
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  auto socket = CreateTCPSocket();
  api::socket::SecureOptions options;
  options.tls_version.emplace();
  options.tls_version->min = "tls1.2";
  options.tls_version->max = "tls1.2";

  UpgradeToTLSFuture upgrade_future;
  socket->UpgradeToTLS(&options, upgrade_future.GetCallback());
  auto [net_error, tls_socket, local_addr, peer_addr, receive_handle,
        send_handle] = upgrade_future.Take();
  EXPECT_EQ(net_error, net::OK);
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
  options.tls_version.emplace();
  options.tls_version->min = "tls1.3";
  options.tls_version->max = "tls1.3";

  UpgradeToTLSFuture upgrade_future;
  socket->UpgradeToTLS(&options, upgrade_future.GetCallback());
  auto [net_error, tls_socket, local_addr, peer_addr, receive_handle,
        send_handle] = upgrade_future.Take();
  EXPECT_EQ(net_error, net::OK);
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
}

// Test that the API maps minimum versions of TLS 1.0, a no longer supported
// protocol, to TLS 1.2.
TEST_F(TLSSocketTest, UpgradeToTLSWithCustomOptionsTLS10) {
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
  ssl_socket.expected_ssl_version_min = net::SSL_PROTOCOL_VERSION_TLS1_2;
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  auto socket = CreateTCPSocket();
  api::socket::SecureOptions options;
  options.tls_version.emplace();
  options.tls_version->min = "tls1";

  UpgradeToTLSFuture upgrade_future;
  socket->UpgradeToTLS(&options, upgrade_future.GetCallback());
  int net_error = std::get<0>(upgrade_future.Take());
  EXPECT_EQ(net_error, net::OK);
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
}

// Test that the API maps minimum versions of TLS 1.1, a no longer supported
// protocol, to TLS 1.2.
TEST_F(TLSSocketTest, UpgradeToTLSWithCustomOptionsTLS11) {
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
  ssl_socket.expected_ssl_version_min = net::SSL_PROTOCOL_VERSION_TLS1_2;
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  auto socket = CreateTCPSocket();
  api::socket::SecureOptions options;
  options.tls_version.emplace();
  options.tls_version->min = "tls1.1";

  UpgradeToTLSFuture upgrade_future;
  socket->UpgradeToTLS(&options, upgrade_future.GetCallback());
  int net_error = std::get<0>(upgrade_future.Take());
  EXPECT_EQ(net_error, net::OK);
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

  {
    auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kTestMsg);
    WriteFuture write_future;
    socket->Write(io_buffer.get(), kTestMsgLength, write_future.GetCallback());
    EXPECT_EQ(kTestMsgLength, write_future.Get());
  }

  std::string received_data;
  while (true) {
    ReadFuture read_future;
    socket->Read(/*count=*/512, read_future.GetCallback());
    auto [net_error, io_buffer, socket_destroying] = read_future.Take();
    EXPECT_FALSE(socket_destroying);
    if (net_error > 0) {
      received_data.append(io_buffer->data(), net_error);
    } else {
      break;
    }
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

  {
    auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kTestMsg);
    WriteFuture write_future;
    socket->Write(io_buffer.get(), kTestMsgLength, write_future.GetCallback());
    EXPECT_EQ(kTestMsgLength, write_future.Get());
  }

  int count = 1;
  std::string received_data;
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

  {
    auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kTestMsg);
    WriteFuture write_future;
    socket->Write(io_buffer.get(), kTestMsgLength, write_future.GetCallback());
    EXPECT_EQ(kTestMsgLength, write_future.Get());
  }

  const int count = 512;
  int net_error_out = net::OK;
  while (true) {
    ReadFuture read_future;
    socket->Read(count, read_future.GetCallback());
    auto [net_error, io_buffer, socket_destroying] = read_future.Take();
    EXPECT_FALSE(socket_destroying);
    if (net_error <= 0) {
      net_error_out = net_error;
      EXPECT_FALSE(socket->IsConnected());
      EXPECT_EQ(nullptr, io_buffer);
      break;
    } else {
      EXPECT_TRUE(socket->IsConnected());
    }
  }
  // Note that TLSSocket only detects that receive pipe is broken and propagates
  // it as 0 byte read. It doesn't know the specific net error code. To know the
  // specific net error code, it needs to register itself as a
  // network::mojom::SocketObserver. However, that gets tricky because of two
  // separate mojo pipes.
  EXPECT_EQ(0, net_error_out);
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
    WriteFuture write_future;
    socket->Write(drainable_io_buffer.get(), kTestMsgLength - num_bytes_written,
                  write_future.GetCallback());
    int32_t result = write_future.Get();
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
    WriteFuture write_future;
    socket->Write(
        drainable_io_buffer.get(),
        std::max(kTestMsgLength - num_bytes_written, num_bytes_to_write),
        write_future.GetCallback());
    int32_t bytes_written = write_future.Get();
    ASSERT_GT(bytes_written, 0);
    drainable_io_buffer->DidConsume(bytes_written);
    num_bytes_written += bytes_written;
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
  int32_t net_error = net::OK;
  while (true) {
    WriteFuture write_future;
    socket->Write(io_buffer.get(), kTestMsgLength, write_future.GetCallback());
    net_error = write_future.Get();
    if (net_error <= 0) {
      break;
    }
  }
  // Note that TCPSocket only detects that send pipe is broken and propagates
  // it as a net::ERR_FAILED. It doesn't know the specific net error code. To do
  // that, it needs to register itself as a network::mojom::SocketObserver.
  EXPECT_EQ(net::ERR_FAILED, net_error);
  EXPECT_FALSE(socket->IsConnected());
}

}  // namespace extensions

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/openscreen/network_service_quic_packet_writer.h"
#include "chrome/browser/media/router/providers/openscreen/network_service_async_packet_sender.h"

#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "media/base/fake_single_thread_task_runner.h"
#include "net/base/net_errors.h"

namespace media_router {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace {

const quic::QuicIpAddress kValidIPAddress(quic::QuicIpAddress::Loopback4());
const quic::QuicSocketAddress kValidSocketAddress(kValidIPAddress, 80);

const net::Error kFailureErrorCode = net::Error::ERR_CONNECTION_CLOSED;

const std::size_t kValidBufferSize = 10;
const char kValidBuffer[kValidBufferSize] = "123456789";

class MockAsyncPacketSender : public AsyncPacketSender {
 public:
  ~MockAsyncPacketSender() override = default;
  MOCK_METHOD3(SendTo,
               net::Error(const net::IPEndPoint&,
                          base::span<const uint8_t>,
                          base::OnceCallback<void(int32_t)>));
};

class MockDelegate : public NetworkServiceQuicPacketWriter::Delegate {
 public:
  ~MockDelegate() override = default;

  MOCK_METHOD1(OnWriteError, void(net::Error));
  MOCK_METHOD0(OnWriteUnblocked, void());
};

// Useful struct that creates a packet writer, and exposes the private
// delegate and AsyncPacketSender fields for mocking purposes.
struct TestPacketWriter {
  TestPacketWriter() {
    task_runner = new media::FakeSingleThreadTaskRunner(&sender_clock);
    transport = new StrictMock<MockAsyncPacketSender>();
    writer = std::make_unique<NetworkServiceQuicPacketWriter>(
        std::unique_ptr<AsyncPacketSender>(transport), &delegate, task_runner);
  }

  void SetSendToCallbackError(int error_code) {
    EXPECT_CALL(*transport, SendTo(_, _, _))
        .WillOnce(WithArg<2>(
            Invoke([error_code](base::OnceCallback<void(int32_t)> callback) {
              std::move(callback).Run(error_code);
              return net::Error::OK;
            })));
  }

  StrictMock<MockDelegate> delegate;
  base::SimpleTestTickClock sender_clock;
  StrictMock<MockAsyncPacketSender>* transport;
  scoped_refptr<media::FakeSingleThreadTaskRunner> task_runner;
  std::unique_ptr<NetworkServiceQuicPacketWriter> writer;
};

}  // namespace

TEST(NetworkServiceQuicPacketWriterTest, BatchModeShouldAlwaysBeFalse) {
  TestPacketWriter test_writer;
  ASSERT_FALSE(test_writer.writer->IsBatchMode());
}

TEST(NetworkServiceQuicPacketWriterTest, ShouldNotSupportReleaseTime) {
  TestPacketWriter test_writer;
  ASSERT_FALSE(test_writer.writer->SupportsReleaseTime());
}

TEST(NetworkServiceQuicPacketWriterTest, FlushDoesNotReturnError) {
  TestPacketWriter test_writer;
  auto write_result = test_writer.writer->Flush();

  ASSERT_EQ(write_result, quic::WriteResult(quic::WRITE_STATUS_OK, 0));
}

TEST(NetworkServiceQuicPacketWriterTest, CallsSendToWithCorrectIPAddress) {
  TestPacketWriter test_writer;

  // Based on kValidSocketAddress from above.
  const net::IPAddress kExpectedIPAddress(127, 0, 0, 1);
  const net::IPEndPoint kExpectedEndpoint(kExpectedIPAddress, 80);

  EXPECT_CALL(*test_writer.transport, SendTo(kExpectedEndpoint, _, _));

  const auto result = test_writer.writer->WritePacket(
      kValidBuffer, kValidBufferSize, kValidIPAddress, kValidSocketAddress,
      nullptr);
  ASSERT_EQ(result, quic::WriteResult(quic::WRITE_STATUS_OK, kValidBufferSize));
  test_writer.task_runner->RunTasks();
}

TEST(NetworkServiceQuicPacketWriter, SocketNotReadyErrorNotifiesDelegate) {
  TestPacketWriter test_writer;

  ASSERT_FALSE(test_writer.writer->IsWriteBlocked());

  EXPECT_CALL(*test_writer.transport, SendTo(_, _, _))
      .WillOnce(Return(net::Error::ERR_SOCKET_NOT_CONNECTED));
  EXPECT_CALL(test_writer.delegate,
              OnWriteError(net::Error::ERR_SOCKET_NOT_CONNECTED));

  test_writer.writer->WritePacket(kValidBuffer, kValidBufferSize,
                                  kValidIPAddress, kValidSocketAddress,
                                  nullptr);
  test_writer.task_runner->RunTasks();
}

TEST(NetworkServiceQuicPacketWriterTest, WriteErrorNotifiesDelegate) {
  TestPacketWriter test_writer;

  ASSERT_FALSE(test_writer.writer->IsWriteBlocked());

  test_writer.SetSendToCallbackError(kFailureErrorCode);
  EXPECT_CALL(test_writer.delegate, OnWriteError(kFailureErrorCode));

  test_writer.writer->WritePacket(kValidBuffer, kValidBufferSize,
                                  kValidIPAddress, kValidSocketAddress,
                                  nullptr);
  test_writer.task_runner->RunTasks();
}

TEST(NetworkServiceQuicPacketWriterTest, ErrorCausesWriteableSetToFalse) {
  TestPacketWriter test_writer;

  ASSERT_FALSE(test_writer.writer->IsWriteBlocked());

  test_writer.SetSendToCallbackError(kFailureErrorCode);
  EXPECT_CALL(test_writer.delegate, OnWriteError(kFailureErrorCode));
  ASSERT_FALSE(test_writer.writer->IsWriteBlocked());

  const auto result = test_writer.writer->WritePacket(
      kValidBuffer, kValidBufferSize, kValidIPAddress, kValidSocketAddress,
      nullptr);
  test_writer.task_runner->RunTasks();

  ASSERT_EQ(result, quic::WriteResult(quic::WRITE_STATUS_OK, kValidBufferSize));
  ASSERT_TRUE(test_writer.writer->IsWriteBlocked());
}

TEST(NetworkServiceQuicPacketWriterTest, DoesNotCallSendToIfWriteBlocked) {
  TestPacketWriter test_writer;

  ASSERT_FALSE(test_writer.writer->IsWriteBlocked());

  test_writer.SetSendToCallbackError(kFailureErrorCode);
  EXPECT_CALL(test_writer.delegate, OnWriteError(kFailureErrorCode));
  ASSERT_FALSE(test_writer.writer->IsWriteBlocked());

  const auto result = test_writer.writer->WritePacket(
      kValidBuffer, kValidBufferSize, kValidIPAddress, kValidSocketAddress,
      nullptr);
  test_writer.task_runner->RunTasks();

  ASSERT_EQ(result, quic::WriteResult(quic::WRITE_STATUS_OK, kValidBufferSize));
  ASSERT_TRUE(test_writer.writer->IsWriteBlocked());

  const auto second_result = test_writer.writer->WritePacket(
      kValidBuffer, kValidBufferSize, kValidIPAddress, kValidSocketAddress,
      nullptr);

  // We have an implicit assertion here, since SetSendToCallbackError
  // expects only ONE call, if the WritePacket above results in a call to
  // SendTo we will assert.
  test_writer.task_runner->RunTasks();
  ASSERT_EQ(second_result,
            quic::WriteResult(quic::WRITE_STATUS_BLOCKED, EWOULDBLOCK));
}

TEST(NetworkServiceQuicPacketWriterTest, WriteUnblockedNotifiesDelegate) {
  TestPacketWriter test_writer;

  ASSERT_FALSE(test_writer.writer->IsWriteBlocked());

  test_writer.SetSendToCallbackError(kFailureErrorCode);
  EXPECT_CALL(test_writer.delegate, OnWriteError(kFailureErrorCode));
  ASSERT_FALSE(test_writer.writer->IsWriteBlocked());

  const auto result = test_writer.writer->WritePacket(
      kValidBuffer, kValidBufferSize, kValidIPAddress, kValidSocketAddress,
      nullptr);
  test_writer.task_runner->RunTasks();

  ASSERT_EQ(result, quic::WriteResult(quic::WRITE_STATUS_OK, kValidBufferSize));
  ASSERT_TRUE(test_writer.writer->IsWriteBlocked());

  EXPECT_CALL(test_writer.delegate, OnWriteUnblocked());
  test_writer.writer->SetWritable();
  test_writer.task_runner->RunTasks();
}

TEST(NetworkServiceQuicPacketWriterTest, CanResetWriteableWithSetter) {
  TestPacketWriter test_writer;

  ASSERT_FALSE(test_writer.writer->IsWriteBlocked());

  test_writer.SetSendToCallbackError(kFailureErrorCode);
  EXPECT_CALL(test_writer.delegate, OnWriteError(kFailureErrorCode));

  test_writer.writer->WritePacket(kValidBuffer, kValidBufferSize,
                                  kValidIPAddress, kValidSocketAddress,
                                  nullptr);
  test_writer.task_runner->RunTasks();

  ASSERT_TRUE(test_writer.writer->IsWriteBlocked());
  EXPECT_CALL(test_writer.delegate, OnWriteUnblocked());
  test_writer.writer->SetWritable();
  test_writer.task_runner->RunTasks();
  ASSERT_FALSE(test_writer.writer->IsWriteBlocked());
}

TEST(NetworkServiceQuicPacketWriterTest, TooManyPacketsCausesWriteBlockage) {
  TestPacketWriter test_writer;
  ASSERT_FALSE(test_writer.writer->IsWriteBlocked());

  // Store the write complete callbacks for calling later.
  std::vector<base::OnceCallback<void(int32_t)>> callbacks;
  EXPECT_CALL(*test_writer.transport, SendTo(_, _, _))
      .WillRepeatedly(WithArg<2>(
          Invoke([&callbacks](base::OnceCallback<void(int32_t)> callback) {
            callbacks.push_back(std::move(callback));
            return net::Error::OK;
          })));

  EXPECT_CALL(test_writer.delegate, OnWriteUnblocked());

  // We should be able to write max_packets_in_flight amount of packets.
  constexpr std::size_t max_packets_in_flight = 4;
  test_writer.writer->SetMaxPacketsInFlightForTesting(max_packets_in_flight);
  for (std::size_t i = 0; i < max_packets_in_flight; ++i) {
    test_writer.writer->WritePacket(kValidBuffer, kValidBufferSize,
                                    kValidIPAddress, kValidSocketAddress,
                                    nullptr);
  }

  // Writing an additional packet should result in a blocking failure.
  const quic::WriteResult result = test_writer.writer->WritePacket(
      kValidBuffer, kValidBufferSize, kValidIPAddress, kValidSocketAddress,
      nullptr);
  ASSERT_EQ(quic::WriteResult(quic::WRITE_STATUS_BLOCKED, EWOULDBLOCK), result);
  test_writer.task_runner->RunTasks();

  // We should be blocked until we actually run the callbacks.
  ASSERT_TRUE(test_writer.writer->IsWriteBlocked());
  ASSERT_EQ(max_packets_in_flight, callbacks.size());
  for (auto& callback : callbacks) {
    std::move(callback).Run(net::Error::OK);
  }

  ASSERT_FALSE(test_writer.writer->IsWriteBlocked());
}

}  // namespace media_router

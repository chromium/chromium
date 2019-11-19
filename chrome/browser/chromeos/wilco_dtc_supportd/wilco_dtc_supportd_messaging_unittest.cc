// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/mojo_utils.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/testing_wilco_dtc_supportd_bridge_wrapper.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_client.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_messaging.h"
#include "chrome/services/wilco_dtc_supportd/public/mojom/wilco_dtc_supportd.mojom.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "mojo/public/cpp/system/handle.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using testing::WithArg;

namespace chromeos {

namespace {

const char kMessageFromExtension[] = "\"test message from extension\"";
const char kMessageFromDaemon[] = "\"test message from daemon\"";

class MockNativeMessageHostClient
    : public extensions::NativeMessageHost::Client {
 public:
  MOCK_METHOD1(PostMessageFromNativeHost, void(const std::string& message));
  MOCK_METHOD1(CloseChannel, void(const std::string& error_message));
};

std::string AssertGetStringFromMojoHandle(mojo::ScopedHandle handle) {
  if (!handle)
    return std::string();
  base::ReadOnlySharedMemoryMapping shared_memory;
  std::string contents =
      GetStringPieceFromMojoHandle(std::move(handle), &shared_memory)
          .as_string();
  CHECK(!contents.empty());
  return contents;
}

mojo::ScopedHandle AssertCreateReadOnlySharedMemoryMojoHandle(
    const std::string& content) {
  if (content.empty())
    return mojo::ScopedHandle();
  mojo::ScopedHandle shared_memory_handle =
      CreateReadOnlySharedMemoryMojoHandle(content);
  CHECK(shared_memory_handle);
  return shared_memory_handle;
}

using SendUiMessageToWilcoDtcImplCallback =
    base::RepeatingCallback<void(const std::string& response_json_message)>;

class MockMojoWilcoDtcSupportdService
    : public wilco_dtc_supportd::mojom::WilcoDtcSupportdService {
 public:
  void SendUiMessageToWilcoDtc(
      mojo::ScopedHandle json_message,
      SendUiMessageToWilcoDtcCallback callback) override {
    // Redirect the call to the Impl method to workaround GMock's issues with
    // move-only types and to make setting test expectations easier (by using
    // std::string's rather than memory handles).
    SendUiMessageToWilcoDtcImpl(
        AssertGetStringFromMojoHandle(std::move(json_message)),
        base::BindRepeating(
            [](SendUiMessageToWilcoDtcCallback original_callback,
               const std::string& response_json_message) {
              std::move(original_callback)
                  .Run(AssertCreateReadOnlySharedMemoryMojoHandle(
                      response_json_message));
            },
            base::Passed(&callback)));
  }

  MOCK_METHOD2(SendUiMessageToWilcoDtcImpl,
               void(const std::string& json_message,
                    SendUiMessageToWilcoDtcImplCallback callback));
  MOCK_METHOD0(NotifyConfigurationDataChanged, void());
};

}  // namespace

// Test that the message channel gets closed if the WilcoDtcSupportdBridge
// instance isn't created.
TEST(WilcoDtcSupportdMessagingOpenedByExtensionNoBridgeTest, Test) {
  base::test::TaskEnvironment task_environment;

  // Create the message host.
  std::unique_ptr<extensions::NativeMessageHost> message_host =
      CreateExtensionOwnedWilcoDtcSupportdMessageHost(nullptr);
  StrictMock<MockNativeMessageHostClient> message_host_client;

  // The message host will close the channel during the OnMessage() call at the
  // latest.
  EXPECT_CALL(message_host_client,
              CloseChannel(extensions::NativeMessageHost::kNotFoundError));
  message_host->Start(&message_host_client);
  message_host->OnMessage(kMessageFromExtension);
}

namespace {

// Test fixture that spins up a testing WilcoDtcSupportdBridge instance.
class WilcoDtcSupportdMessagingOpenedByExtensionTest : public testing::Test {
 protected:
  WilcoDtcSupportdMessagingOpenedByExtensionTest() {
    WilcoDtcSupportdClient::InitializeFake();
    testing_wilco_dtc_supportd_bridge_wrapper_ =
        TestingWilcoDtcSupportdBridgeWrapper::Create(
            &mojo_wilco_dtc_supportd_service_,
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            &wilco_dtc_supportd_bridge_);
  }

  ~WilcoDtcSupportdMessagingOpenedByExtensionTest() override {
    // Make sure |wilco_dtc_supportd_bridge_| is destroyed before
    // DBusThreadManager is shut down, since the WilcoDtcSupportdBridge class
    // uses the latter.
    wilco_dtc_supportd_bridge_.reset();
    WilcoDtcSupportdClient::Shutdown();
  }

  MockMojoWilcoDtcSupportdService* mojo_wilco_dtc_supportd_service() {
    return &mojo_wilco_dtc_supportd_service_;
  }

  TestingWilcoDtcSupportdBridgeWrapper* wilco_dtc_supportd_bridge_wrapper() {
    return testing_wilco_dtc_supportd_bridge_wrapper_.get();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
  StrictMock<MockMojoWilcoDtcSupportdService> mojo_wilco_dtc_supportd_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingWilcoDtcSupportdBridgeWrapper>
      testing_wilco_dtc_supportd_bridge_wrapper_;
  std::unique_ptr<WilcoDtcSupportdBridge> wilco_dtc_supportd_bridge_;
};

}  // namespace

// Test that the message channel gets closed if there's no Mojo connection to
// the wilco_dtc_supportd daemon.
TEST_F(WilcoDtcSupportdMessagingOpenedByExtensionTest, NoMojoConnection) {
  // Create the message host.
  std::unique_ptr<extensions::NativeMessageHost> message_host =
      CreateExtensionOwnedWilcoDtcSupportdMessageHost(nullptr);
  StrictMock<MockNativeMessageHostClient> message_host_client;
  message_host->Start(&message_host_client);

  // The message host will close the channel during the OnMessage() call.
  EXPECT_CALL(message_host_client,
              CloseChannel(extensions::NativeMessageHost::kNotFoundError));
  message_host->OnMessage(kMessageFromExtension);
}

namespace {

// Test fixture that spins up a testing WilcoDtcSupportdBridge instance and
// creates and owns a single message host, to simplify testing of basic
// scenarios.
class WilcoDtcSupportdMessagingOpenedByExtensionSingleHostTest
    : public WilcoDtcSupportdMessagingOpenedByExtensionTest {
 protected:
  WilcoDtcSupportdMessagingOpenedByExtensionSingleHostTest() {
    wilco_dtc_supportd_bridge_wrapper()->EstablishFakeMojoConnection();
    message_host_ = CreateExtensionOwnedWilcoDtcSupportdMessageHost(nullptr);
    message_host_->Start(&message_host_client_);
  }

  extensions::NativeMessageHost* message_host() {
    DCHECK(message_host_);
    return message_host_.get();
  }

  void DestroyMessageHost() { message_host_.reset(); }

  void ExpectMojoSendMessageCall(
      const std::string& expected_message,
      SendUiMessageToWilcoDtcImplCallback* captured_callback,
      base::RunLoop* run_loop) {
    EXPECT_CALL(*mojo_wilco_dtc_supportd_service(),
                SendUiMessageToWilcoDtcImpl(expected_message, _))
        .WillOnce(DoAll(SaveArg<1>(captured_callback),
                        InvokeWithoutArgs(run_loop, &base::RunLoop::Quit)));
  }

  void ExpectMojoSendMessageCallAndRespond(
      const std::string& expected_message,
      const std::string& response_message_to_pass) {
    EXPECT_CALL(*mojo_wilco_dtc_supportd_service(),
                SendUiMessageToWilcoDtcImpl(expected_message, _))
        .WillOnce(WithArg<1>(
            Invoke([=](SendUiMessageToWilcoDtcImplCallback callback) {
              std::move(callback).Run(response_message_to_pass);
            })));
  }

  void ExpectMessageArrivalToExtensionAndChannelClosing(
      const std::string& expected_message,
      base::RunLoop* run_loop) {
    InSequence s;
    EXPECT_CALL(message_host_client_,
                PostMessageFromNativeHost(expected_message));
    EXPECT_CALL(message_host_client_,
                CloseChannel(std::string() /* error_message */))
        .WillOnce(InvokeWithoutArgs(run_loop, &base::RunLoop::Quit));
  }

  void ExpectChannelClosingWithError(const std::string& expected_error_message,
                                     base::RunLoop* run_loop) {
    EXPECT_CALL(message_host_client_, CloseChannel(expected_error_message))
        .WillOnce(InvokeWithoutArgs(run_loop, &base::RunLoop::Quit));
  }

 private:
  StrictMock<MockNativeMessageHostClient> message_host_client_;
  std::unique_ptr<extensions::NativeMessageHost> message_host_;
};

}  // namespace

// Test the basic successful scenario when the message is successfully delivered
// from an extension to the daemon and the response is delivered back.
TEST_F(WilcoDtcSupportdMessagingOpenedByExtensionSingleHostTest,
       SingleRequestResponse) {
  // Set up the daemon's Mojo service to expect the message from the extension.
  SendUiMessageToWilcoDtcImplCallback mojo_method_callback;
  base::RunLoop mojo_method_run_loop;
  ExpectMojoSendMessageCall(kMessageFromExtension /* expected_message */,
                            &mojo_method_callback, &mojo_method_run_loop);

  // Send the message from the extension and wait till it arrives to the daemon.
  message_host()->OnMessage(kMessageFromExtension);
  mojo_method_run_loop.Run();
  ASSERT_TRUE(mojo_method_callback);

  // Set up the expectation that the response message arrives to the extension
  // and the message channel is closed afterwards.
  base::RunLoop channel_close_run_loop;
  ExpectMessageArrivalToExtensionAndChannelClosing(
      kMessageFromDaemon /* expected_message */, &channel_close_run_loop);

  // Respond from the daemon and wait till the message channel gets closed.
  std::move(mojo_method_callback).Run(kMessageFromDaemon);
  channel_close_run_loop.Run();
}

// Test that when the daemon responds without any message, no message is sent to
// the extension.
TEST_F(WilcoDtcSupportdMessagingOpenedByExtensionSingleHostTest,
       EmptyResponse) {
  // Set up the daemon's Mojo service to expect the message from the extension
  // and to respond with an empty message.
  ExpectMojoSendMessageCallAndRespond(
      kMessageFromExtension /* expected_message */,
      std::string() /* response_message_to_pass */);

  // Set up the expectation that the message host closes the channel with an
  // empty error message.
  base::RunLoop channel_close_run_loop;
  ExpectChannelClosingWithError(std::string() /* expected_error_message */,
                                &channel_close_run_loop);

  // Send the message from the extension and wait till the channel gets closed.
  message_host()->OnMessage(kMessageFromExtension);
  channel_close_run_loop.Run();
}

// Test the case when both the extension and the daemon send heavy messages, but
// which are nevertheless within the acceptable bounds.
TEST_F(WilcoDtcSupportdMessagingOpenedByExtensionSingleHostTest,
       HeavyMessages) {
  const std::string kHeavyMessageFromExtension(
      kWilcoDtcSupportdUiMessageMaxSize, '\1');
  const std::string kHeavyMessageFromDaemon(kWilcoDtcSupportdUiMessageMaxSize,
                                            '\2');

  // Set up the daemon's Mojo service to expect the message from the extension
  // and to respond with another message.
  ExpectMojoSendMessageCallAndRespond(
      kHeavyMessageFromExtension /* expected_message */,
      kHeavyMessageFromDaemon /* response_message_to_pass */);

  // Set up the expectation that the response message arrives to the extension
  // and the message channel is closed afterwards.
  base::RunLoop channel_close_run_loop;
  ExpectMessageArrivalToExtensionAndChannelClosing(
      kHeavyMessageFromDaemon /* expected_message */, &channel_close_run_loop);

  // Send the message from the extension and wait till the response from the
  // daemon arrives.
  message_host()->OnMessage(kHeavyMessageFromExtension);
  channel_close_run_loop.Run();
}

// Test that when the extension sends a too heavy message, it is discarded and
// the message channel is closed.
TEST_F(WilcoDtcSupportdMessagingOpenedByExtensionSingleHostTest,
       ExcessivelyBigRequest) {
  const std::string kExcessivelyBigMessage(
      kWilcoDtcSupportdUiMessageMaxSize + 1, '\1');

  base::RunLoop channel_close_run_loop;
  ExpectChannelClosingWithError(kWilcoDtcSupportdUiMessageTooBigExtensionsError
                                /* expected_error_message */,
                                &channel_close_run_loop);

  message_host()->OnMessage(kExcessivelyBigMessage);

  channel_close_run_loop.Run();
}

// Test that when the daemon sends a too heavy message, it is discarded and the
// message channel is closed.
TEST_F(WilcoDtcSupportdMessagingOpenedByExtensionSingleHostTest,
       ExcessivelyBigResponse) {
  const std::string kExcessivelyBigMessage(
      kWilcoDtcSupportdUiMessageMaxSize + 1, '\1');

  // Set up the daemon's Mojo service to expect the message from the extension
  // and to respond with a heavy message.
  ExpectMojoSendMessageCallAndRespond(
      kMessageFromExtension /* expected_message */,
      kExcessivelyBigMessage /* response_message_to_pass */);

  // Set up the expectation that the message host closes the channel with an
  // empty error message.
  base::RunLoop channel_close_run_loop;
  ExpectChannelClosingWithError(kWilcoDtcSupportdUiMessageTooBigExtensionsError
                                /* expected_error_message */,
                                &channel_close_run_loop);

  // Send the message from the extension and wait till the channel gets closed.
  message_host()->OnMessage(kMessageFromExtension);
  channel_close_run_loop.Run();
}

// Test that extra messages sent by the extension before the daemon's response
// arrives result in the channel being closed with an error.
TEST_F(WilcoDtcSupportdMessagingOpenedByExtensionSingleHostTest,
       ExtraRequestsBeforeResponse) {
  // Set up the daemon's Mojo service to expect the message from the extension.
  SendUiMessageToWilcoDtcImplCallback mojo_method_callback;
  base::RunLoop mojo_method_run_loop;
  ExpectMojoSendMessageCall(kMessageFromExtension /* expected_message */,
                            &mojo_method_callback, &mojo_method_run_loop);

  // Send the first message from the extension.
  message_host()->OnMessage(kMessageFromExtension);

  // Send the second message from the extension and wait till the message host
  // closes the channel.
  base::RunLoop channel_close_run_loop;
  ExpectChannelClosingWithError(kWilcoDtcSupportdUiExtraMessagesExtensionsError
                                /* expected_error_message */,
                                &channel_close_run_loop);
  message_host()->OnMessage(kMessageFromExtension);
  channel_close_run_loop.Run();

  // Send a third message from the extension. No more CloseChannel() calls
  // should be made.
  message_host()->OnMessage(kMessageFromExtension);

  // Wait till the message arrives to the daemon and reply from the daemon.
  mojo_method_run_loop.Run();
  ASSERT_TRUE(mojo_method_callback);
  std::move(mojo_method_callback).Run(kMessageFromDaemon);
  // No messages should arrive to the extension at this point. There's no
  // reliable way to wait till the wrong call, if the tested code is buggy,
  // could have been made. RunUntilIdle() is used to make the test failing at
  // least with some probability in case of such a bug.
  RunUntilIdle();
}

// Test that extra messages sent by the extension after the daemon's response is
// delivered are ignored (since the message channel is in the middle of being
// closed at this point).
TEST_F(WilcoDtcSupportdMessagingOpenedByExtensionSingleHostTest,
       ExtraRequestsAfterResponse) {
  // Set up the daemon's Mojo service to expect the message from the extension
  // and to respond with another message.
  ExpectMojoSendMessageCallAndRespond(
      kMessageFromExtension /* expected_message */,
      kMessageFromDaemon /* response_message_to_pass */);

  // Set up the expectation that the response message arrives to the extension
  // and the message channel is closed afterwards.
  base::RunLoop channel_close_run_loop;
  ExpectMessageArrivalToExtensionAndChannelClosing(
      kMessageFromDaemon /* expected_message */, &channel_close_run_loop);

  // Send the message from the extension and wait till the channel gets closed.
  message_host()->OnMessage(kMessageFromExtension);
  channel_close_run_loop.Run();

  // Send the second message from the extension.
  message_host()->OnMessage(kMessageFromExtension);
  // No more messages should arrive to the daemon at this point, neither should
  // CloseChannel() be called. There's no reliable way to wait till the wrong
  // call, if the tested code is buggy, could have been made. RunUntilIdle() is
  // used to make the test failing at least with some probability in case of
  // such a bug.
  RunUntilIdle();
}

// Test the scenario when the message host is destroyed before the response from
// the daemon arrives.
TEST_F(WilcoDtcSupportdMessagingOpenedByExtensionSingleHostTest,
       DestroyBeforeResponse) {
  // Set up the daemon's Mojo service to expect the message from the extension.
  SendUiMessageToWilcoDtcImplCallback mojo_method_callback;
  base::RunLoop mojo_method_run_loop;
  ExpectMojoSendMessageCall(kMessageFromExtension /* expected_message */,
                            &mojo_method_callback, &mojo_method_run_loop);

  // Send a message from the extension and wait until the Mojo call gets
  // captured by |mojo_method_callback|.
  message_host()->OnMessage(kMessageFromExtension);
  mojo_method_run_loop.Run();
  ASSERT_TRUE(mojo_method_callback);

  DestroyMessageHost();

  // Respond from the daemon.
  std::move(mojo_method_callback).Run(kMessageFromDaemon);
  // No calls should be made on the destroyed message host instance at this
  // point. There's no reliable way to wait till the wrong call, if the tested
  // code is buggy, could have been made. RunUntilIdle() is used to make the
  // test failing at least with some probability in case of such a bug.
  RunUntilIdle();
}

}  // namespace chromeos

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/webrtc_signaling_messenger.h"

#include <string>
#include <vector>

#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/instantmessaging/constants.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kSelfId[] = "self_id";
const char kOAuthToken[] = "oauth_token";
const char kTestAccount[] = "test@test.test";

chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse
CreateReceiveMessagesResponse(const std::string& msg) {
  chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse
      response;
  response.mutable_inbox_message()->set_message(msg);
  return response;
}

chrome_browser_nearby_sharing_instantmessaging::StreamBody BuildResponseProto(
    const std::vector<std::string>& messages) {
  chrome_browser_nearby_sharing_instantmessaging::StreamBody stream_body;
  for (const auto& msg : messages) {
    stream_body.add_messages(
        CreateReceiveMessagesResponse(msg).SerializeAsString());
  }
  return stream_body;
}

class FakeIncomingMessagesListener
    : public sharing::mojom::IncomingMessagesListener {
 public:
  ~FakeIncomingMessagesListener() override = default;

  void OnMessage(const std::string& message) override {
    messages_received_.push_back(message);
  }

  const std::vector<std::string>& messages_received() {
    return messages_received_;
  }

 private:
  std::vector<std::string> messages_received_;
};

class WebRtcSignalingMessengerTest : public testing::Test {
 public:
  WebRtcSignalingMessengerTest()
      : webrtc_signaling_messenger_(
            identity_test_environment_.identity_manager(),
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}
  ~WebRtcSignalingMessengerTest() override = default;

  void SetUp() override {
    identity_test_environment_.MakeUnconsentedPrimaryAccountAvailable(
        kTestAccount);
  }

  void SetOAuthTokenSuccessful(bool success) {
    identity_test_environment_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            success ? kOAuthToken : "", base::Time::Now());
  }

  WebRtcSignalingMessenger& GetMessenger() {
    return webrtc_signaling_messenger_;
  }

  network::TestURLLoaderFactory& GetTestUrlLoaderFactory() {
    return test_url_loader_factory_;
  }

  // Required to ensure that the listener has received all messages before we
  // can continue with our tests.
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  WebRtcSignalingMessenger webrtc_signaling_messenger_;
};

TEST_F(WebRtcSignalingMessengerTest, UnsuccessfulSendMessage_EmptyToken) {
  base::RunLoop loop;
  GetMessenger().SendMessage(kSelfId, "peer_id", "message",
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_FALSE(success);
                               loop.Quit();
                             }));
  SetOAuthTokenSuccessful(/*success=*/false);
  loop.Run();
}

TEST_F(WebRtcSignalingMessengerTest, UnsuccessfulSendMessage_HttpError) {
  base::RunLoop loop;
  GetMessenger().SendMessage(kSelfId, "peer_id", "message",
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_FALSE(success);
                               loop.Quit();
                             }));
  SetOAuthTokenSuccessful(/*success=*/true);

  ASSERT_TRUE(
      GetTestUrlLoaderFactory().IsPending(kInstantMessagingSendMessageAPI));
  GetTestUrlLoaderFactory().AddResponse(kInstantMessagingSendMessageAPI,
                                        "response", net::HTTP_FORBIDDEN);

  loop.Run();
}

TEST_F(WebRtcSignalingMessengerTest, SuccessfulSendMessage) {
  base::RunLoop loop;
  GetMessenger().SendMessage(kSelfId, "peer_id", "message",
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_TRUE(success);
                               loop.Quit();
                             }));
  SetOAuthTokenSuccessful(/*success=*/true);

  ASSERT_TRUE(
      GetTestUrlLoaderFactory().IsPending(kInstantMessagingSendMessageAPI));
  GetTestUrlLoaderFactory().AddResponse(kInstantMessagingSendMessageAPI,
                                        "response", net::HTTP_OK);

  loop.Run();
}

TEST_F(WebRtcSignalingMessengerTest, UnsuccessfulReceiveMessages_EmptyToken) {
  FakeIncomingMessagesListener listener;
  mojo::Receiver<sharing::mojom::IncomingMessagesListener> mojo_receiver{
      &listener};

  base::RunLoop loop;
  GetMessenger().StartReceivingMessages(
      kSelfId, mojo_receiver.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_FALSE(success);
        loop.Quit();
      }));
  SetOAuthTokenSuccessful(/*success=*/false);
  loop.Run();
}

TEST_F(WebRtcSignalingMessengerTest, UnsuccessfulReceiveMessages_HttpError) {
  FakeIncomingMessagesListener listener;
  mojo::Receiver<sharing::mojom::IncomingMessagesListener> mojo_receiver{
      &listener};

  base::RunLoop loop;
  GetMessenger().StartReceivingMessages(
      kSelfId, mojo_receiver.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_FALSE(success);
        loop.Quit();
      }));
  SetOAuthTokenSuccessful(/*success=*/true);

  std::string response = BuildResponseProto({"message"}).SerializeAsString();
  ASSERT_TRUE(
      GetTestUrlLoaderFactory().IsPending(kInstantMessagingReceiveMessageAPI));
  GetTestUrlLoaderFactory().AddResponse(kInstantMessagingReceiveMessageAPI,
                                        response, net::HTTP_FORBIDDEN);

  loop.Run();

  EXPECT_TRUE(listener.messages_received().empty());
}

TEST_F(WebRtcSignalingMessengerTest, SuccessfulReceiveMessages) {
  FakeIncomingMessagesListener listener;
  mojo::Receiver<sharing::mojom::IncomingMessagesListener> mojo_receiver{
      &listener};

  base::RunLoop loop;
  GetMessenger().StartReceivingMessages(
      kSelfId, mojo_receiver.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        loop.Quit();
      }));
  SetOAuthTokenSuccessful(/*success=*/true);

  std::vector<std::string> messages = {"hello", "world"};
  std::string response = BuildResponseProto(messages).SerializeAsString();
  ASSERT_TRUE(
      GetTestUrlLoaderFactory().IsPending(kInstantMessagingReceiveMessageAPI));
  GetTestUrlLoaderFactory().AddResponse(kInstantMessagingReceiveMessageAPI,
                                        response, net::HTTP_OK);

  loop.Run();

  RunUntilIdle();
  EXPECT_EQ(messages, listener.messages_received());
}

TEST_F(WebRtcSignalingMessengerTest,
       StartReceivingMessages_RegisterAgainWithoutStopping) {
  FakeIncomingMessagesListener listener_1, listener_2;
  mojo::Receiver<sharing::mojom::IncomingMessagesListener> mojo_receiver_1{
      &listener_1},
      mojo_receiver_2{&listener_2};

  base::RunLoop loop_1;
  GetMessenger().StartReceivingMessages(
      kSelfId, mojo_receiver_1.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        loop_1.Quit();
      }));
  SetOAuthTokenSuccessful(/*success=*/true);

  std::vector<std::string> messages_1 = {"hello", "world"};
  std::string response_1 = BuildResponseProto(messages_1).SerializeAsString();
  ASSERT_TRUE(
      GetTestUrlLoaderFactory().IsPending(kInstantMessagingReceiveMessageAPI));
  GetTestUrlLoaderFactory().SimulateResponseForPendingRequest(
      kInstantMessagingReceiveMessageAPI, response_1);
  loop_1.Run();

  RunUntilIdle();
  EXPECT_EQ(messages_1, listener_1.messages_received());

  base::RunLoop loop_2;
  GetMessenger().StartReceivingMessages(
      kSelfId, mojo_receiver_2.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        loop_2.Quit();
      }));
  SetOAuthTokenSuccessful(/*success=*/true);

  std::vector<std::string> messages_2 = {"the quick brown", "fox jumps",
                                         "over the lazy dog"};
  std::string response_2 = BuildResponseProto(messages_2).SerializeAsString();
  ASSERT_TRUE(
      GetTestUrlLoaderFactory().IsPending(kInstantMessagingReceiveMessageAPI));
  GetTestUrlLoaderFactory().SimulateResponseForPendingRequest(
      kInstantMessagingReceiveMessageAPI, response_2);
  loop_2.Run();

  RunUntilIdle();
  EXPECT_EQ(messages_1, listener_1.messages_received());
  EXPECT_EQ(messages_2, listener_2.messages_received());
}

TEST_F(WebRtcSignalingMessengerTest, StopReceivingMessages) {
  FakeIncomingMessagesListener listener;
  mojo::Receiver<sharing::mojom::IncomingMessagesListener> mojo_receiver{
      &listener};

  base::RunLoop loop;
  GetMessenger().StartReceivingMessages(
      kSelfId, mojo_receiver.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        loop.Quit();
      }));
  SetOAuthTokenSuccessful(/*success=*/true);

  std::vector<std::string> messages = {"hello", "world"};
  std::string response = BuildResponseProto(messages).SerializeAsString();
  ASSERT_TRUE(
      GetTestUrlLoaderFactory().IsPending(kInstantMessagingReceiveMessageAPI));
  GetTestUrlLoaderFactory().SimulateResponseForPendingRequest(
      kInstantMessagingReceiveMessageAPI, response);
  loop.Run();

  RunUntilIdle();
  EXPECT_EQ(messages, listener.messages_received());

  base::RunLoop disconnect_loop;
  mojo_receiver.set_disconnect_handler(
      base::BindLambdaForTesting([&]() { disconnect_loop.Quit(); }));
  GetMessenger().StopReceivingMessages();
  disconnect_loop.Run();
}

}  // namespace

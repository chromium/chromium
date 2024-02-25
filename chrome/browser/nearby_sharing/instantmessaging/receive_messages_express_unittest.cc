// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/instantmessaging/receive_messages_express.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/nearby_sharing/instantmessaging/constants.h"
#include "chrome/browser/nearby_sharing/instantmessaging/fake_token_fetcher.h"
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
const char kCountryCode[] = "US";

chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse
CreateReceiveMessagesResponse(const std::string& msg) {
  chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse
      response;
  response.mutable_inbox_message()->set_message(msg);
  return response;
}

chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse
CreateFastPathReadyResponse() {
  chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse
      response;
  response.mutable_fast_path_ready();
  return response;
}

chrome_browser_nearby_sharing_instantmessaging::StreamBody BuildResponseProto(
    const std::vector<std::string>& messages,
    bool include_fast_path_ready = true) {
  chrome_browser_nearby_sharing_instantmessaging::StreamBody stream_body;
  if (include_fast_path_ready) {
    stream_body.add_messages(CreateFastPathReadyResponse().SerializeAsString());
  }
  for (const auto& msg : messages) {
    stream_body.add_messages(
        CreateReceiveMessagesResponse(msg).SerializeAsString());
  }
  return stream_body;
}

}  // namespace

class FakeIncomingMessagesListener
    : public sharing::mojom::IncomingMessagesListener {
 public:
  ~FakeIncomingMessagesListener() override = default;

  void OnMessage(const std::string& message) override {
    messages_received_.push_back(message);
  }

  void OnComplete(bool success) override { on_complete_result_ = success; }

  const std::vector<std::string>& messages_received() {
    return messages_received_;
  }

  std::optional<bool> on_complete_result() { return on_complete_result_; }

 private:
  std::vector<std::string> messages_received_;
  std::optional<bool> on_complete_result_;
};

class ReceiveMessagesExpressTest : public testing::Test {
 public:
  ReceiveMessagesExpressTest()
      : test_shared_loader_factory_(
            test_url_loader_factory_.GetSafeWeakWrapper()) {
    identity_test_environment_.MakePrimaryAccountAvailable(
        kTestAccount, signin::ConsentLevel::kSignin);
  }
  ~ReceiveMessagesExpressTest() override = default;

  sharing::mojom::LocationHintPtr CountryCodeLocationHint(
      std::string country_code) {
    sharing::mojom::LocationHintPtr location_hint_ptr =
        sharing::mojom::LocationHint::New();
    location_hint_ptr->location = country_code;
    location_hint_ptr->format =
        sharing::mojom::LocationStandardFormat::ISO_3166_1_ALPHA_2;
    return location_hint_ptr;
  }

  network::TestURLLoaderFactory& GetTestUrlLoaderFactory() {
    return test_url_loader_factory_;
  }

  int NumMessagesReceived() {
    return message_listener_.messages_received().size();
  }

  const std::vector<std::string>& GetMessagesReceived() {
    return message_listener_.messages_received();
  }

  std::optional<bool> OnCompleteResult() {
    return message_listener_.on_complete_result();
  }

  std::string GetFastPathOnlyResponse() {
    return BuildResponseProto({}, /*include_fast_path_ready=*/true)
        .SerializeAsString();
  }

  void StartReceivingMessages(base::RunLoop* run_loop, bool token_successful) {
    ReceiveMessagesExpress::StartReceiveSession(
        kSelfId, CountryCodeLocationHint(kCountryCode),
        listener_receiver_.BindNewPipeAndPassRemote(),
        base::BindOnce(&ReceiveMessagesExpressTest::OnStartReceivingMessages,
                       base::Unretained(this), run_loop),
        identity_test_environment_.identity_manager(),
        test_shared_loader_factory_);

    // This allows the token fetcher to resolve.
    identity_test_environment_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            token_successful ? kOAuthToken : "", base::Time::Now());
  }

  void OnStartReceivingMessages(
      base::RunLoop* run_loop,
      bool success,
      mojo::PendingRemote<sharing::mojom::ReceiveMessagesSession>
          pending_remote) {
    start_receive_success_ = success;
    session_pending_remote_ = std::move(pending_remote);
    if (run_loop) {
      run_loop->Quit();
    }
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  std::optional<bool> start_receive_success_;
  FakeIncomingMessagesListener message_listener_;
  mojo::Receiver<sharing::mojom::IncomingMessagesListener> listener_receiver_{
      &message_listener_};

  mojo::PendingRemote<sharing::mojom::ReceiveMessagesSession>
      session_pending_remote_;
};

TEST_F(ReceiveMessagesExpressTest, OAuthTokenFailed) {
  base::RunLoop run_loop;

  StartReceivingMessages(&run_loop, /*token_success=*/false);
  ASSERT_EQ(0, GetTestUrlLoaderFactory().NumPending());

  run_loop.Run();

  // Token fetch will fail here so we won't receive any messages
  EXPECT_EQ(0, NumMessagesReceived());
  ASSERT_TRUE(start_receive_success_.has_value());
  EXPECT_FALSE(*start_receive_success_);
}

TEST_F(ReceiveMessagesExpressTest, HttpResponseError) {
  base::RunLoop run_loop;

  StartReceivingMessages(&run_loop, /*token_success=*/true);

  ASSERT_TRUE(
      GetTestUrlLoaderFactory().IsPending(kInstantMessagingReceiveMessageAPI));
  std::string response = BuildResponseProto({"message"}).SerializeAsString();

  // Calls OnComplete(false) in ReceiveMessagesExpress. OnDataReceived() is not
  // called.
  GetTestUrlLoaderFactory().AddResponse(kInstantMessagingReceiveMessageAPI,
                                        response, net::HTTP_FORBIDDEN);
  run_loop.Run();

  EXPECT_EQ(0, NumMessagesReceived());
  ASSERT_TRUE(start_receive_success_.has_value());
  EXPECT_FALSE(*start_receive_success_);
}

TEST_F(ReceiveMessagesExpressTest, SuccessfulResponse) {
  base::RunLoop run_loop;

  StartReceivingMessages(&run_loop, /*token_success=*/true);

  ASSERT_EQ(1, GetTestUrlLoaderFactory().NumPending());
  ASSERT_TRUE(
      GetTestUrlLoaderFactory().IsPending(kInstantMessagingReceiveMessageAPI));

  std::vector<std::string> messages = {"quick brown", "fox"};
  std::string response = BuildResponseProto(messages).SerializeAsString();

  // Calls OnDataReceived() in ReceiveMessagesExpress.
  GetTestUrlLoaderFactory().AddResponse(kInstantMessagingReceiveMessageAPI,
                                        response, net::HTTP_OK);
  ASSERT_EQ(0, GetTestUrlLoaderFactory().NumPending());
  run_loop.Run();

  EXPECT_EQ(messages, GetMessagesReceived());
}

TEST_F(ReceiveMessagesExpressTest, SuccessfulPartialResponse) {
  base::RunLoop run_loop;

  StartReceivingMessages(&run_loop, /*token_success=*/true);

  ASSERT_EQ(1, GetTestUrlLoaderFactory().NumPending());
  ASSERT_TRUE(
      GetTestUrlLoaderFactory().IsPending(kInstantMessagingReceiveMessageAPI));

  std::vector<std::string> messages = {"quick brown", "fox"};
  std::string response = BuildResponseProto(messages).SerializeAsString();
  std::string partial_response =
      BuildResponseProto({"partial last message"}).SerializeAsString();
  // Random partial substring.
  response += partial_response.substr(0, 10);

  // Calls OnDataReceived() in ReceiveMessagesExpress.
  GetTestUrlLoaderFactory().AddResponse(kInstantMessagingReceiveMessageAPI,
                                        response, net::HTTP_OK);
  ASSERT_EQ(0, GetTestUrlLoaderFactory().NumPending());
  run_loop.Run();

  EXPECT_EQ(messages, GetMessagesReceived());
}

TEST_F(ReceiveMessagesExpressTest, StopPreventsPendingTransfer) {
  base::RunLoop run_loop;
  StartReceivingMessages(&run_loop, /*token_success=*/true);

  // Calls OnDataReceived() in ReceiveMessagesExpress.
  GetTestUrlLoaderFactory().AddResponse(kInstantMessagingReceiveMessageAPI,
                                        GetFastPathOnlyResponse(),
                                        net::HTTP_OK);
  run_loop.Run();

  EXPECT_TRUE(session_pending_remote_);

  base::RunLoop run_loop_2;
  listener_receiver_.set_disconnect_handler(run_loop_2.QuitClosure());
  {
    mojo::Remote<sharing::mojom::ReceiveMessagesSession> session_remote_(
        std::move(session_pending_remote_));
    session_remote_->StopReceivingMessages();
    // When the remote goes out of scope, the pipe will disconnect and
    // the ReceiveMessageExpress will disconnect and be cleaned up.
  }
  run_loop_2.Run();
}

TEST_F(ReceiveMessagesExpressTest, PendingRemoteCleanupDisconnects) {
  base::RunLoop run_loop;

  StartReceivingMessages(&run_loop, /*token_success=*/true);

  // Calls OnDataReceived() in ReceiveMessagesExpress.
  GetTestUrlLoaderFactory().AddResponse(kInstantMessagingReceiveMessageAPI,
                                        GetFastPathOnlyResponse(),
                                        net::HTTP_OK);
  run_loop.Run();

  // We got our fast path ready and first message, now try stopping the stream.
  EXPECT_TRUE(session_pending_remote_);

  base::RunLoop run_loop_2;
  listener_receiver_.set_disconnect_handler(run_loop_2.QuitClosure());
  // If we reset the session_pending_remote_ before binding everything
  // should disconnect.
  session_pending_remote_.reset();
  run_loop_2.Run();
}

TEST_F(ReceiveMessagesExpressTest, OnCompleteAfterSuccess) {
  base::RunLoop run_loop;
  StartReceivingMessages(&run_loop, /*token_success=*/true);

  std::vector<std::string> messages = {"quick brown", "fox"};
  std::string response = BuildResponseProto(messages).SerializeAsString();
  GetTestUrlLoaderFactory().AddResponse(kInstantMessagingReceiveMessageAPI,
                                        response, net::HTTP_OK);
  run_loop.Run();

  ASSERT_EQ(0, GetTestUrlLoaderFactory().NumPending());
  ASSERT_TRUE(OnCompleteResult().has_value());
  EXPECT_TRUE(OnCompleteResult().value());
}

TEST_F(ReceiveMessagesExpressTest, NoOnCompleteWithoutFastPathReady) {
  base::RunLoop run_loop;
  StartReceivingMessages(&run_loop, /*token_success=*/true);

  std::vector<std::string> messages = {"quick brown", "fox"};
  std::string response =
      BuildResponseProto(messages, /*include_fast_path_ready=*/false)
          .SerializeAsString();
  GetTestUrlLoaderFactory().AddResponse(kInstantMessagingReceiveMessageAPI,
                                        response, net::HTTP_FORBIDDEN);
  run_loop.Run();

  ASSERT_EQ(0, GetTestUrlLoaderFactory().NumPending());
  ASSERT_FALSE(OnCompleteResult().has_value());
}

TEST_F(ReceiveMessagesExpressTest, FastPathTimeout) {
  base::RunLoop run_loop;
  StartReceivingMessages(&run_loop, /*token_success=*/true);
  run_loop.Run();
  ASSERT_TRUE(start_receive_success_.has_value());
  EXPECT_FALSE(start_receive_success_.value());
  ASSERT_FALSE(OnCompleteResult().has_value());
}

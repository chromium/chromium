// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/instantmessaging/receive_messages_express.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/nearby_sharing/instantmessaging/constants.h"
#include "chrome/browser/nearby_sharing/instantmessaging/fake_token_fetcher.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesExpressRequest
CreateRequest() {
  return chrome_browser_nearby_sharing_instantmessaging::
      ReceiveMessagesExpressRequest();
}

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

class ReceiveMessagesExpressTest : public testing::Test {
 public:
  ReceiveMessagesExpressTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        receive_messages_express_(&fake_token_fetcher_,
                                  test_shared_loader_factory_) {}
  ~ReceiveMessagesExpressTest() override = default;

  ReceiveMessagesExpress& GetMessenger() { return receive_messages_express_; }

  FakeTokenFetcher& GetFakeTokenFetcher() { return fake_token_fetcher_; }

  network::TestURLLoaderFactory& GetTestUrlLoaderFactory() {
    return test_url_loader_factory_;
  }

  void OnMessageReceived(const std::string& message) {
    received_messages_.push_back(message);
  }

  int NumMessagesReceived() { return received_messages_.size(); }

  const std::vector<std::string>& GetMessagesReceived() {
    return received_messages_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  FakeTokenFetcher fake_token_fetcher_;
  ReceiveMessagesExpress receive_messages_express_;

  std::vector<std::string> received_messages_;
};

TEST_F(ReceiveMessagesExpressTest, OAuthTokenFailed) {
  base::RunLoop run_loop;
  GetMessenger().StartReceivingMessages(
      CreateRequest(),
      base::BindRepeating(&ReceiveMessagesExpressTest::OnMessageReceived,
                          base::Unretained(this)),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_FALSE(success);
        run_loop.Quit();
      }));
  ASSERT_EQ(0, GetTestUrlLoaderFactory().NumPending());
  run_loop.Run();

  EXPECT_EQ(0, NumMessagesReceived());
}

TEST_F(ReceiveMessagesExpressTest, HttpResponseError) {
  base::RunLoop run_loop;
  GetFakeTokenFetcher().SetAccessToken("token");
  GetMessenger().StartReceivingMessages(
      CreateRequest(),
      base::BindRepeating(&ReceiveMessagesExpressTest::OnMessageReceived,
                          base::Unretained(this)),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_FALSE(success);
        run_loop.Quit();
      }));
  ASSERT_TRUE(
      GetTestUrlLoaderFactory().IsPending(kInstantMessagingReceiveMessageAPI));
  std::string response = BuildResponseProto({"message"}).SerializeAsString();

  // Calls OnComplete(false) in ReceiveMessagesExpress. OnDataReceived() is not
  // called.
  GetTestUrlLoaderFactory().AddResponse(kInstantMessagingReceiveMessageAPI,
                                        response, net::HTTP_FORBIDDEN);
  run_loop.Run();

  EXPECT_EQ(0, NumMessagesReceived());
}

TEST_F(ReceiveMessagesExpressTest, SuccessfulResponse) {
  base::RunLoop run_loop;
  GetFakeTokenFetcher().SetAccessToken("token");
  GetMessenger().StartReceivingMessages(
      CreateRequest(),
      base::BindRepeating(&ReceiveMessagesExpressTest::OnMessageReceived,
                          base::Unretained(this)),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
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

  EXPECT_EQ(2, NumMessagesReceived());
  EXPECT_EQ(messages, GetMessagesReceived());
}

TEST_F(ReceiveMessagesExpressTest, SuccessfulPartialResponse) {
  base::RunLoop run_loop;
  GetFakeTokenFetcher().SetAccessToken("token");
  GetMessenger().StartReceivingMessages(
      CreateRequest(),
      base::BindRepeating(&ReceiveMessagesExpressTest::OnMessageReceived,
                          base::Unretained(this)),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
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

  EXPECT_EQ(2, NumMessagesReceived());
  EXPECT_EQ(messages, GetMessagesReceived());
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sms/sms_fetch_request_handler.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "components/sync/protocol/sharing_message.pb.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::BindLambdaForTesting;
using chrome_browser_sharing::ResponseMessage;
using chrome_browser_sharing::SharingMessage;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace {

class MockSmsFetcher : public content::SmsFetcher {
 public:
  MockSmsFetcher() = default;
  ~MockSmsFetcher() = default;

  MOCK_METHOD2(Subscribe,
               void(const url::Origin& origin, Subscriber* subscriber));
  MOCK_METHOD2(Unsubscribe,
               void(const url::Origin& origin, Subscriber* subscriber));
  MOCK_METHOD0(HasSubscribers, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSmsFetcher);
};

SharingMessage CreateRequest(const std::string& origin) {
  SharingMessage message;
  message.mutable_sms_fetch_request()->set_origin(origin);
  return message;
}

}  // namespace

TEST(SmsFetchRequestHandlerTest, Basic) {
  base::test::SingleThreadTaskEnvironment task_environment;
  StrictMock<MockSmsFetcher> fetcher;
  SmsFetchRequestHandler handler(&fetcher);
  SharingMessage message = CreateRequest("https://a.com");

  base::RunLoop loop;

  content::SmsFetcher::Subscriber* subscriber;
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SaveArg<1>(&subscriber));
  EXPECT_CALL(fetcher, Unsubscribe(_, _));

  handler.OnMessage(
      message,
      BindLambdaForTesting([&loop](std::unique_ptr<ResponseMessage> response) {
        EXPECT_TRUE(response->has_sms_fetch_response());
        EXPECT_EQ("123", response->sms_fetch_response().one_time_code());
        EXPECT_EQ("hello", response->sms_fetch_response().sms());
        loop.Quit();
      }));

  subscriber->OnReceive("123", "hello");

  loop.Run();
}

TEST(SmsFetchRequestHandlerTest, OutOfOrder) {
  base::test::SingleThreadTaskEnvironment task_environment;
  StrictMock<MockSmsFetcher> fetcher;
  SmsFetchRequestHandler handler(&fetcher);
  SharingMessage message = CreateRequest("https://a.com");

  base::RunLoop loop1;

  content::SmsFetcher::Subscriber* request1;
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SaveArg<1>(&request1));
  EXPECT_CALL(fetcher, Unsubscribe(_, _)).Times(2);

  handler.OnMessage(
      message,
      BindLambdaForTesting([&loop1](std::unique_ptr<ResponseMessage> response) {
        EXPECT_TRUE(response->has_sms_fetch_response());
        EXPECT_EQ("first", response->sms_fetch_response().sms());
        loop1.Quit();
      }));

  base::RunLoop loop2;

  content::SmsFetcher::Subscriber* request2;
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SaveArg<1>(&request2));

  handler.OnMessage(
      message,
      BindLambdaForTesting([&loop2](std::unique_ptr<ResponseMessage> response) {
        EXPECT_TRUE(response->has_sms_fetch_response());
        EXPECT_EQ("second", response->sms_fetch_response().sms());
        loop2.Quit();
      }));

  request2->OnReceive("2", "second");

  loop2.Run();

  request1->OnReceive("1", "first");

  loop1.Run();
}

TEST(SmsFetchRequestHandlerTest, HangingRequestUnsubscribedUponDestruction) {
  base::test::SingleThreadTaskEnvironment task_environment;
  StrictMock<MockSmsFetcher> fetcher;

  SmsFetchRequestHandler handler(&fetcher);
  SharingMessage message = CreateRequest("https://a.com");
  content::SmsFetcher::Subscriber* subscriber;
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SaveArg<1>(&subscriber));

  // Expects Unsubscribe to be called when SmsFetchRequestHandler goes out of
  // scope.
  EXPECT_CALL(fetcher, Unsubscribe(_, _));

  // Leaves the request deliberately hanging without a response to assert
  // that it gets cleaned up.
  handler.OnMessage(
      message,
      BindLambdaForTesting([&](std::unique_ptr<ResponseMessage> response) {}));
}

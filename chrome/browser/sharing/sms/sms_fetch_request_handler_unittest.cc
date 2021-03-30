// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sms/sms_fetch_request_handler.h"
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::BindLambdaForTesting;
using chrome_browser_sharing::ResponseMessage;
using chrome_browser_sharing::SharingMessage;
using content::SmsFetcher;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace {

class MockSmsFetcher : public SmsFetcher {
 public:
  MockSmsFetcher() = default;
  ~MockSmsFetcher() = default;

  MOCK_METHOD2(Subscribe,
               void(const content::OriginList& origin_list,
                    Subscriber* subscriber));
  MOCK_METHOD3(Subscribe,
               void(const content::OriginList& origin_list,
                    Subscriber* subscriber,
                    content::RenderFrameHost* rfh));
  MOCK_METHOD2(Unsubscribe,
               void(const content::OriginList& origin_list,
                    Subscriber* subscriber));
  MOCK_METHOD0(HasSubscribers, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSmsFetcher);
};

class MockSmsFetchRequestHandler : public SmsFetchRequestHandler {
 public:
  explicit MockSmsFetchRequestHandler(content::SmsFetcher* fetcher)
      : SmsFetchRequestHandler(fetcher) {}
  ~MockSmsFetchRequestHandler() = default;

  MOCK_METHOD2(AskUserPermission,
               void(const content::OriginList&,
                    const std::string& one_time_code));
  MOCK_METHOD2(OnConfirm, void(JNIEnv*, jstring origin));
  MOCK_METHOD2(OnDismiss, void(JNIEnv*, jstring origin));

  MockSmsFetchRequestHandler(const MockSmsFetchRequestHandler&) = delete;
  MockSmsFetchRequestHandler& operator=(const MockSmsFetchRequestHandler&) =
      delete;
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
  const std::string origin = "https://a.com";
  SharingMessage message = CreateRequest(origin);
  JNIEnv* env = base::android::AttachCurrentThread();
  const std::u16string formatted_origin =
      url_formatter::FormatOriginForSecurityDisplay(
          url::Origin::Create(GURL(origin)),
          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  base::android::ScopedJavaLocalRef<jstring> j_origin =
      base::android::ConvertUTF16ToJavaString(env, formatted_origin);

  base::RunLoop loop;

  SmsFetcher::Subscriber* subscriber;
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SaveArg<1>(&subscriber));
  EXPECT_CALL(fetcher, Unsubscribe(_, _));

  handler.OnMessage(
      message,
      BindLambdaForTesting([&loop](std::unique_ptr<ResponseMessage> response) {
        EXPECT_TRUE(response->has_sms_fetch_response());
        EXPECT_EQ("123", response->sms_fetch_response().one_time_code());
        loop.Quit();
      }));

  subscriber->OnReceive(content::OriginList{url::Origin::Create(GURL(origin))},
                        "123", SmsFetcher::UserConsent::kNotObtained);
  handler.OnConfirm(env, j_origin.obj());
  loop.Run();
}

TEST(SmsFetchRequestHandlerTest, OutOfOrder) {
  base::test::SingleThreadTaskEnvironment task_environment;
  StrictMock<MockSmsFetcher> fetcher;
  SmsFetchRequestHandler handler(&fetcher);
  JNIEnv* env = base::android::AttachCurrentThread();
  const std::string origin1 = "https://a.com";
  SharingMessage message1 = CreateRequest(origin1);
  const std::u16string formatted_origin1 =
      url_formatter::FormatOriginForSecurityDisplay(
          url::Origin::Create(GURL(origin1)),
          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  base::android::ScopedJavaLocalRef<jstring> j_origin1 =
      base::android::ConvertUTF16ToJavaString(env, formatted_origin1);

  const std::string origin2 = "https://b.com";
  SharingMessage message2 = CreateRequest(origin2);
  const std::u16string formatted_origin2 =
      url_formatter::FormatOriginForSecurityDisplay(
          url::Origin::Create(GURL(origin2)),
          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  base::android::ScopedJavaLocalRef<jstring> j_origin2 =
      base::android::ConvertUTF16ToJavaString(env, formatted_origin2);

  base::RunLoop loop1;

  SmsFetcher::Subscriber* request1;
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SaveArg<1>(&request1));
  EXPECT_CALL(fetcher, Unsubscribe(_, _)).Times(2);

  handler.OnMessage(
      message1,
      BindLambdaForTesting([&loop1](std::unique_ptr<ResponseMessage> response) {
        EXPECT_TRUE(response->has_sms_fetch_response());
        EXPECT_EQ("1", response->sms_fetch_response().one_time_code());
        loop1.Quit();
      }));

  base::RunLoop loop2;

  SmsFetcher::Subscriber* request2;
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SaveArg<1>(&request2));

  handler.OnMessage(
      message2,
      BindLambdaForTesting([&loop2](std::unique_ptr<ResponseMessage> response) {
        EXPECT_TRUE(response->has_sms_fetch_response());
        EXPECT_EQ("2", response->sms_fetch_response().one_time_code());
        loop2.Quit();
      }));

  request2->OnReceive(content::OriginList{url::Origin::Create(GURL(origin2))},
                      "2", SmsFetcher::UserConsent::kNotObtained);
  handler.OnConfirm(env, j_origin2.obj());
  loop2.Run();

  request1->OnReceive(content::OriginList{url::Origin::Create(GURL(origin1))},
                      "1", SmsFetcher::UserConsent::kNotObtained);
  handler.OnConfirm(env, j_origin1.obj());
  loop1.Run();
}

TEST(SmsFetchRequestHandlerTest, HangingRequestUnsubscribedUponDestruction) {
  base::test::SingleThreadTaskEnvironment task_environment;
  StrictMock<MockSmsFetcher> fetcher;

  SmsFetchRequestHandler handler(&fetcher);
  SharingMessage message = CreateRequest("https://a.com");
  SmsFetcher::Subscriber* subscriber;
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

TEST(SmsFetchRequestHandlerTest, AskUserPermissionOnReceive) {
  base::test::SingleThreadTaskEnvironment task_environment;
  StrictMock<MockSmsFetcher> fetcher;
  MockSmsFetchRequestHandler handler(&fetcher);
  SharingMessage message = CreateRequest("https://a.com");

  SmsFetcher::Subscriber* subscriber;
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SaveArg<1>(&subscriber));
  EXPECT_CALL(handler, AskUserPermission);
  EXPECT_CALL(fetcher, Unsubscribe);

  handler.OnMessage(message, base::DoNothing());

  subscriber->OnReceive(
      content::OriginList{url::Origin::Create(GURL("https://a.com"))}, "123",
      SmsFetcher::UserConsent::kNotObtained);
}

TEST(SmsFetchRequestHandlerTest, SendSuccessMessageOnConfirm) {
  base::test::SingleThreadTaskEnvironment task_environment;
  StrictMock<MockSmsFetcher> fetcher;
  SmsFetchRequestHandler handler(&fetcher);
  const std::string origin = "https://a.com";
  SharingMessage message = CreateRequest(origin);
  JNIEnv* env = base::android::AttachCurrentThread();
  const std::u16string formatted_origin =
      url_formatter::FormatOriginForSecurityDisplay(
          url::Origin::Create(GURL(origin)),
          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  base::android::ScopedJavaLocalRef<jstring> j_origin =
      base::android::ConvertUTF16ToJavaString(env, formatted_origin);

  base::RunLoop loop;

  SmsFetcher::Subscriber* subscriber;
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SaveArg<1>(&subscriber));
  EXPECT_CALL(fetcher, Unsubscribe);

  handler.OnMessage(
      message,
      BindLambdaForTesting([&loop](std::unique_ptr<ResponseMessage> response) {
        EXPECT_TRUE(response->has_sms_fetch_response());
        EXPECT_EQ("123", response->sms_fetch_response().one_time_code());
        loop.Quit();
      }));

  subscriber->OnReceive(content::OriginList{url::Origin::Create(GURL(origin))},
                        "123", SmsFetcher::UserConsent::kNotObtained);
  handler.OnConfirm(env, j_origin.obj());
  loop.Run();
}

TEST(SmsFetchRequestHandlerTest, SendFailureMessageOnDismiss) {
  base::test::SingleThreadTaskEnvironment task_environment;
  StrictMock<MockSmsFetcher> fetcher;
  SmsFetchRequestHandler handler(&fetcher);
  const std::string origin = "https://a.com";
  SharingMessage message = CreateRequest(origin);
  JNIEnv* env = base::android::AttachCurrentThread();
  const std::u16string formatted_origin =
      url_formatter::FormatOriginForSecurityDisplay(
          url::Origin::Create(GURL(origin)),
          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  base::android::ScopedJavaLocalRef<jstring> j_origin =
      base::android::ConvertUTF16ToJavaString(env, formatted_origin);

  base::RunLoop loop;

  SmsFetcher::Subscriber* subscriber;
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SaveArg<1>(&subscriber));
  EXPECT_CALL(fetcher, Unsubscribe);

  handler.OnMessage(
      message,
      BindLambdaForTesting([&loop](std::unique_ptr<ResponseMessage> response) {
        EXPECT_TRUE(response->has_sms_fetch_response());
        EXPECT_EQ(content::SmsFetchFailureType::kPromptCancelled,
                  static_cast<content::SmsFetchFailureType>(
                      response->sms_fetch_response().failure_type()));
        loop.Quit();
      }));

  subscriber->OnReceive(content::OriginList{url::Origin::Create(GURL(origin))},
                        "123", SmsFetcher::UserConsent::kNotObtained);
  handler.OnDismiss(env, j_origin.obj());
  loop.Run();
}

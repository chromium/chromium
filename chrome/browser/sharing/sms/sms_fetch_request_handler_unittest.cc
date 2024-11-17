// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sms/sms_fetch_request_handler.h"

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/sharing_message/mock_sharing_device_source.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_message_handler.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::BindLambdaForTesting;
using components_sharing_message::ResponseMessage;
using components_sharing_message::SharingMessage;
using content::SmsFetcher;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace {

const char kDefaultDeviceName[] = "Default Device";

class MockSmsFetcher : public SmsFetcher {
 public:
  MockSmsFetcher() = default;

  MockSmsFetcher(const MockSmsFetcher&) = delete;
  MockSmsFetcher& operator=(const MockSmsFetcher&) = delete;

  ~MockSmsFetcher() override = default;

  MOCK_METHOD2(Subscribe,
               void(const content::OriginList& origin_list,
                    Subscriber& subscriber));
  MOCK_METHOD3(Subscribe,
               void(const content::OriginList& origin_list,
                    Subscriber& subscriber,
                    content::RenderFrameHost& rfh));
  MOCK_METHOD2(Unsubscribe,
               void(const content::OriginList& origin_list,
                    Subscriber* subscriber));
  MOCK_METHOD0(HasSubscribers, bool());
};

class MockSmsFetchRequestHandler : public SmsFetchRequestHandler {
 public:
  explicit MockSmsFetchRequestHandler(content::SmsFetcher* fetcher)
      : SmsFetchRequestHandler(&device_source_, fetcher) {}
  ~MockSmsFetchRequestHandler() override = default;

  MOCK_METHOD3(AskUserPermission,
               void(const content::OriginList&,
                    const std::string& one_time_code,
                    const std::string& client_name));

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  MockSmsFetchRequestHandler(const MockSmsFetchRequestHandler&) = delete;
  MockSmsFetchRequestHandler& operator=(const MockSmsFetchRequestHandler&) =
      delete;

 private:
  MockSharingDeviceSource device_source_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

SharingMessage CreateRequest(const std::string& origin) {
  SharingMessage message;
  message.set_sender_device_name(kDefaultDeviceName);
  message.mutable_sms_fetch_request()->add_origins(origin);
  return message;
}

SharingMessage CreateRequestWithMultipleOrigins(
    const std::vector<std::string>& origins) {
  SharingMessage message;
  message.set_sender_device_name(kDefaultDeviceName);
  for (const auto& origin : origins)
    message.mutable_sms_fetch_request()->add_origins(origin);
  return message;
}

// A similar action as testing::SaveArg, but it takes the address of the thing.
template <size_t I = 0, typename T>
auto SavePtrToArg(T* out) {
  return [out](auto&&... args) {
    *out = std::addressof(std::get<I>(std::tie(args...)));
  };
}

}  // namespace

TEST(SmsFetchRequestHandlerTest, Basic) {
  StrictMock<MockSmsFetcher> fetcher;
  MockSmsFetchRequestHandler handler(&fetcher);
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
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SavePtrToArg<1>(&subscriber));
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
  handler.OnConfirm(env, j_origin.obj(), nullptr);
  loop.Run();
}

TEST(SmsFetchRequestHandlerTest, OutOfOrder) {
  StrictMock<MockSmsFetcher> fetcher;
  MockSmsFetchRequestHandler handler(&fetcher);
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
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SavePtrToArg<1>(&request1));
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
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SavePtrToArg<1>(&request2));

  handler.OnMessage(
      message2,
      BindLambdaForTesting([&loop2](std::unique_ptr<ResponseMessage> response) {
        EXPECT_TRUE(response->has_sms_fetch_response());
        EXPECT_EQ("2", response->sms_fetch_response().one_time_code());
        loop2.Quit();
      }));

  request2->OnReceive(content::OriginList{url::Origin::Create(GURL(origin2))},
                      "2", SmsFetcher::UserConsent::kNotObtained);
  handler.OnConfirm(env, j_origin2.obj(), nullptr);
  loop2.Run();

  request1->OnReceive(content::OriginList{url::Origin::Create(GURL(origin1))},
                      "1", SmsFetcher::UserConsent::kNotObtained);
  handler.OnConfirm(env, j_origin1.obj(), nullptr);
  loop1.Run();
}

TEST(SmsFetchRequestHandlerTest, HangingRequestUnsubscribedUponDestruction) {
  StrictMock<MockSmsFetcher> fetcher;

  MockSmsFetchRequestHandler handler(&fetcher);
  SharingMessage message = CreateRequest("https://a.com");
  SmsFetcher::Subscriber* subscriber;
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SavePtrToArg<1>(&subscriber));

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
  StrictMock<MockSmsFetcher> fetcher;
  MockSmsFetchRequestHandler handler(&fetcher);
  SharingMessage message = CreateRequest("https://a.com");

  SmsFetcher::Subscriber* subscriber;
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SavePtrToArg<1>(&subscriber));
  EXPECT_CALL(fetcher, Unsubscribe);

  handler.OnMessage(message, base::DoNothing());

  EXPECT_CALL(handler, AskUserPermission).Times(0);
  subscriber->OnReceive(
      content::OriginList{url::Origin::Create(GURL("https://a.com"))}, "123",
      SmsFetcher::UserConsent::kNotObtained);

  testing::Mock::VerifyAndClear(&handler);
  EXPECT_CALL(handler, AskUserPermission);
  handler.task_environment().FastForwardBy(base::Seconds(1));
}

TEST(SmsFetchRequestHandlerTest, SendSuccessMessageOnConfirm) {
  StrictMock<MockSmsFetcher> fetcher;
  MockSmsFetchRequestHandler handler(&fetcher);
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
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SavePtrToArg<1>(&subscriber));
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
  handler.OnConfirm(env, j_origin.obj(), nullptr);
  loop.Run();
}

TEST(SmsFetchRequestHandlerTest, SendFailureMessageOnDismiss) {
  StrictMock<MockSmsFetcher> fetcher;
  MockSmsFetchRequestHandler handler(&fetcher);
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
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SavePtrToArg<1>(&subscriber));
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
  handler.OnDismiss(env, j_origin.obj(), nullptr);
  loop.Run();
}

TEST(SmsFetchRequestHandlerTest, EmbeddedFrameConfirm) {
  StrictMock<MockSmsFetcher> fetcher;
  MockSmsFetchRequestHandler handler(&fetcher);
  const std::string top_origin = "https://top.com";
  const std::string embedded_origin = "https://embedded.com";
  std::vector<std::string> origins{embedded_origin, top_origin};
  SharingMessage message = CreateRequestWithMultipleOrigins(origins);
  JNIEnv* env = base::android::AttachCurrentThread();
  const std::u16string formatted_top_origin =
      url_formatter::FormatOriginForSecurityDisplay(
          url::Origin::Create(GURL(top_origin)),
          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  base::android::ScopedJavaLocalRef<jstring> j_top_origin =
      base::android::ConvertUTF16ToJavaString(env, formatted_top_origin);

  const std::u16string formatted_embedded_origin =
      url_formatter::FormatOriginForSecurityDisplay(
          url::Origin::Create(GURL(embedded_origin)),
          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  base::android::ScopedJavaLocalRef<jstring> j_embedded_origin =
      base::android::ConvertUTF16ToJavaString(env, formatted_embedded_origin);

  base::RunLoop loop;

  SmsFetcher::Subscriber* subscriber;
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SavePtrToArg<1>(&subscriber));
  EXPECT_CALL(fetcher, Unsubscribe(_, _));

  handler.OnMessage(
      message,
      BindLambdaForTesting([&](std::unique_ptr<ResponseMessage> response) {
        EXPECT_TRUE(response->has_sms_fetch_response());
        EXPECT_EQ("123", response->sms_fetch_response().one_time_code());
        const auto& origin_strings = response->sms_fetch_response().origins();
        EXPECT_EQ(embedded_origin, origin_strings[0]);
        EXPECT_EQ(top_origin, origin_strings[1]);
        loop.Quit();
      }));

  content::OriginList origin_list;
  origin_list.push_back(url::Origin::Create(GURL(embedded_origin)));
  origin_list.push_back(url::Origin::Create(GURL(top_origin)));
  subscriber->OnReceive(origin_list, "123",
                        SmsFetcher::UserConsent::kNotObtained);
  handler.OnConfirm(env, j_top_origin.obj(), j_embedded_origin.obj());
  loop.Run();
}

TEST(SmsFetchRequestHandlerTest, EmbeddedFrameDismiss) {
  StrictMock<MockSmsFetcher> fetcher;
  MockSmsFetchRequestHandler handler(&fetcher);
  const std::string top_origin = "https://top.com";
  const std::string embedded_origin = "https://embedded.com";
  std::vector<std::string> origins{embedded_origin, top_origin};
  SharingMessage message = CreateRequestWithMultipleOrigins(origins);
  JNIEnv* env = base::android::AttachCurrentThread();
  const std::u16string formatted_top_origin =
      url_formatter::FormatOriginForSecurityDisplay(
          url::Origin::Create(GURL(top_origin)),
          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  base::android::ScopedJavaLocalRef<jstring> j_top_origin =
      base::android::ConvertUTF16ToJavaString(env, formatted_top_origin);

  const std::u16string formatted_embedded_origin =
      url_formatter::FormatOriginForSecurityDisplay(
          url::Origin::Create(GURL(embedded_origin)),
          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  base::android::ScopedJavaLocalRef<jstring> j_embedded_origin =
      base::android::ConvertUTF16ToJavaString(env, formatted_embedded_origin);

  base::RunLoop loop;

  SmsFetcher::Subscriber* subscriber;
  EXPECT_CALL(fetcher, Subscribe(_, _)).WillOnce(SavePtrToArg<1>(&subscriber));
  EXPECT_CALL(fetcher, Unsubscribe(_, _));

  handler.OnMessage(
      message,
      BindLambdaForTesting([&](std::unique_ptr<ResponseMessage> response) {
        EXPECT_TRUE(response->has_sms_fetch_response());
        EXPECT_EQ(content::SmsFetchFailureType::kPromptCancelled,
                  static_cast<content::SmsFetchFailureType>(
                      response->sms_fetch_response().failure_type()));
        loop.Quit();
      }));

  content::OriginList origin_list;
  origin_list.push_back(url::Origin::Create(GURL(embedded_origin)));
  origin_list.push_back(url::Origin::Create(GURL(top_origin)));
  subscriber->OnReceive(origin_list, "123",
                        SmsFetcher::UserConsent::kNotObtained);
  handler.OnDismiss(env, j_top_origin.obj(), j_embedded_origin.obj());
  loop.Run();
}

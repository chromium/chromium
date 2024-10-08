// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sms/sms_remote_fetcher.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sms/sms_remote_fetcher_metrics.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sharing_message/mock_sharing_service.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_service.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::BindLambdaForTesting;
using components_sharing_message::ResponseMessage;
using components_sharing_message::SharingMessage;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;

MockSharingService* CreateSharingService(content::BrowserContext* context) {
  return static_cast<MockSharingService*>(
      SharingServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          context, base::BindRepeating([](content::BrowserContext* context) {
            return static_cast<std::unique_ptr<KeyedService>>(
                std::make_unique<MockSharingService>());
          })));
}

url::Origin GetOriginForURL(const std::string& url) {
  return url::Origin::Create(GURL(url));
}

SharingTargetDeviceInfo CreateFakeSharingTargetDeviceInfo(
    const std::string& guid,
    const std::string& client_name) {
  return SharingTargetDeviceInfo(guid, client_name,
                                 SharingDevicePlatform::kUnknown,
                                 /*pulse_interval=*/base::TimeDelta(),
                                 syncer::DeviceInfo::FormFactor::kUnknown,
                                 /*last_updated_timestamp=*/base::Time());
}

TEST(SmsRemoteFetcherTest, NoDevicesAvailable) {
  content::BrowserTaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  TestingProfile profile;
  content::WebContents::CreateParams create_params(&profile);
  auto web_contents = content::WebContents::Create(create_params);

  MockSharingService* service = CreateSharingService(&profile);

  std::vector<SharingTargetDeviceInfo> devices;
  EXPECT_CALL(*service, GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));

  base::RunLoop loop;

  FetchRemoteSms(
      web_contents.get(), std::vector<url::Origin>{GetOriginForURL("a.com")},
      BindLambdaForTesting(
          [&loop](std::optional<std::vector<url::Origin>>,
                  std::optional<std::string> result,
                  std::optional<content::SmsFetchFailureType> failure_type) {
            ASSERT_FALSE(result);
            ASSERT_EQ(failure_type,
                      content::SmsFetchFailureType::kCrossDeviceFailure);
            loop.Quit();
          }));

  loop.Run();
  histogram_tester.ExpectUniqueSample(
      "Blink.Sms.Receive.CrossDeviceFailure",
      static_cast<int>(WebOTPCrossDeviceFailure::kNoRemoteDevice), 1);
}

TEST(SmsRemoteFetcherTest, OneDevice) {
  content::BrowserTaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  TestingProfile profile;
  content::WebContents::CreateParams create_params(&profile);
  auto web_contents = content::WebContents::Create(create_params);

  MockSharingService* service = CreateSharingService(&profile);

  std::vector<SharingTargetDeviceInfo> devices;

  devices.push_back(CreateFakeSharingTargetDeviceInfo("guid", "name"));

  EXPECT_CALL(*service, GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));
  base::RunLoop loop;

  EXPECT_CALL(*service, SendMessageToDevice(_, _, _, _))
      .WillOnce(Invoke([&](const SharingTargetDeviceInfo& device_info,
                           base::TimeDelta response_timeout,
                           components_sharing_message::SharingMessage message,
                           SharingMessageSender::ResponseCallback callback) {
        auto response = std::make_unique<ResponseMessage>();
        response->mutable_sms_fetch_response()->set_one_time_code("ABC");
        std::move(callback).Run(SharingSendMessageResult::kSuccessful,
                                std::move(response));
        return base::DoNothing();
      }));

  FetchRemoteSms(
      web_contents.get(), std::vector<url::Origin>{GetOriginForURL("a.com")},
      BindLambdaForTesting(
          [&loop](std::optional<std::vector<url::Origin>>,
                  std::optional<std::string> result,
                  std::optional<content::SmsFetchFailureType> failure_type) {
            ASSERT_TRUE(result);
            ASSERT_EQ("ABC", result);
            loop.Quit();
          }));

  loop.Run();
  histogram_tester.ExpectUniqueSample(
      "Blink.Sms.Receive.CrossDeviceFailure",
      static_cast<int>(WebOTPCrossDeviceFailure::kNoFailure), 1);
}

TEST(SmsRemoteFetcherTest, OneDeviceTimesOut) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  content::WebContents::CreateParams create_params(&profile);
  auto web_contents = content::WebContents::Create(create_params);

  MockSharingService* service = CreateSharingService(&profile);

  std::vector<SharingTargetDeviceInfo> devices;

  devices.push_back(CreateFakeSharingTargetDeviceInfo("guid", "name"));

  EXPECT_CALL(*service, GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));
  base::RunLoop loop;

  EXPECT_CALL(*service, SendMessageToDevice(_, _, _, _))
      .WillOnce(Invoke([&](const SharingTargetDeviceInfo& device_info,
                           base::TimeDelta response_timeout,
                           components_sharing_message::SharingMessage message,
                           SharingMessageSender::ResponseCallback callback) {
        std::move(callback).Run(SharingSendMessageResult::kAckTimeout,
                                std::make_unique<ResponseMessage>());
        return base::DoNothing();
      }));

  FetchRemoteSms(
      web_contents.get(), std::vector<url::Origin>{GetOriginForURL("a.com")},
      BindLambdaForTesting(
          [&loop](std::optional<std::vector<url::Origin>>,
                  std::optional<std::string> result,
                  std::optional<content::SmsFetchFailureType> failure_type) {
            ASSERT_FALSE(result);
            loop.Quit();
          }));

  loop.Run();
}

TEST(SmsRemoteFetcherTest, RequestCancelled) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  content::WebContents::CreateParams create_params(&profile);
  auto web_contents = content::WebContents::Create(create_params);

  MockSharingService* service = CreateSharingService(&profile);

  std::vector<SharingTargetDeviceInfo> devices;

  devices.push_back(CreateFakeSharingTargetDeviceInfo("guid-abc", "name"));

  EXPECT_CALL(*service, GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));
  base::RunLoop loop;

  base::MockOnceClosure mock_callback;
  EXPECT_CALL(*service, SendMessageToDevice(_, _, _, _))
      .WillOnce(Invoke([&](const SharingTargetDeviceInfo& device_info,
                           base::TimeDelta response_timeout,
                           components_sharing_message::SharingMessage message,
                           SharingMessageSender::ResponseCallback callback) {
        std::move(callback).Run(SharingSendMessageResult::kCancelled,
                                std::make_unique<ResponseMessage>());
        return mock_callback.Get();
      }));

  base::OnceClosure cancel_callback = FetchRemoteSms(
      web_contents.get(), std::vector<url::Origin>{GetOriginForURL("a.com")},
      BindLambdaForTesting(
          [&loop](std::optional<std::vector<url::Origin>>,
                  std::optional<std::string> one_time_code,
                  std::optional<content::SmsFetchFailureType> failure_type) {
            ASSERT_FALSE(one_time_code);
            loop.Quit();
          }));

  EXPECT_CALL(mock_callback, Run);
  std::move(cancel_callback).Run();

  loop.Run();
}

TEST(SmsRemoteFetcherTest, NoSharingService) {
  content::BrowserTaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  TestingProfile profile;
  content::WebContents::CreateParams create_params(&profile);
  auto web_contents = content::WebContents::Create(create_params);

  base::RunLoop loop;

  FetchRemoteSms(
      web_contents.get(), std::vector<url::Origin>{GetOriginForURL("a.com")},
      BindLambdaForTesting(
          [&loop](std::optional<std::vector<url::Origin>>,
                  std::optional<std::string> result,
                  std::optional<content::SmsFetchFailureType> failure_type) {
            ASSERT_EQ(failure_type,
                      content::SmsFetchFailureType::kCrossDeviceFailure);
            loop.Quit();
          }));

  loop.Run();
  histogram_tester.ExpectUniqueSample(
      "Blink.Sms.Receive.CrossDeviceFailure",
      WebOTPCrossDeviceFailure::kNoSharingService, 1);
}

TEST(SmsRemoteFetcherTest, SendSharingMessageFailure) {
  content::BrowserTaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  TestingProfile profile;
  content::WebContents::CreateParams create_params(&profile);
  auto web_contents = content::WebContents::Create(create_params);

  MockSharingService* service = CreateSharingService(&profile);

  std::vector<SharingTargetDeviceInfo> devices;

  devices.push_back(CreateFakeSharingTargetDeviceInfo("guid", "name"));

  EXPECT_CALL(*service, GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));
  base::RunLoop loop;

  EXPECT_CALL(*service, SendMessageToDevice(_, _, _, _))
      .WillOnce(Invoke([&](const SharingTargetDeviceInfo& device_info,
                           base::TimeDelta response_timeout,
                           components_sharing_message::SharingMessage message,
                           SharingMessageSender::ResponseCallback callback) {
        std::move(callback).Run(SharingSendMessageResult::kAckTimeout,
                                std::make_unique<ResponseMessage>());
        return base::DoNothing();
      }));

  FetchRemoteSms(
      web_contents.get(), std::vector<url::Origin>{GetOriginForURL("a.com")},
      BindLambdaForTesting(
          [&loop](std::optional<std::vector<url::Origin>>,
                  std::optional<std::string> result,
                  std::optional<content::SmsFetchFailureType> failure_type) {
            ASSERT_FALSE(result);
            ASSERT_EQ(failure_type,
                      content::SmsFetchFailureType::kCrossDeviceFailure);
            loop.Quit();
          }));

  loop.Run();
  histogram_tester.ExpectUniqueSample(
      "Blink.Sms.Receive.CrossDeviceFailure",
      WebOTPCrossDeviceFailure::kSharingMessageFailure, 1);
}

TEST(SmsRemoteFetcherTest, UserDecline) {
  content::BrowserTaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  TestingProfile profile;
  content::WebContents::CreateParams create_params(&profile);
  auto web_contents = content::WebContents::Create(create_params);

  MockSharingService* service = CreateSharingService(&profile);

  std::vector<SharingTargetDeviceInfo> devices;

  devices.push_back(CreateFakeSharingTargetDeviceInfo("guid", "name"));

  EXPECT_CALL(*service, GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));
  base::RunLoop loop;

  EXPECT_CALL(*service, SendMessageToDevice(_, _, _, _))
      .WillOnce(Invoke([&](const SharingTargetDeviceInfo& device_info,
                           base::TimeDelta response_timeout,
                           components_sharing_message::SharingMessage message,
                           SharingMessageSender::ResponseCallback callback) {
        auto response = std::make_unique<ResponseMessage>();
        response->mutable_sms_fetch_response()->set_one_time_code("ABC");
        response->mutable_sms_fetch_response()->set_failure_type(
            static_cast<
                components_sharing_message::SmsFetchResponse::FailureType>(
                content::SmsFetchFailureType::kPromptCancelled));
        std::move(callback).Run(SharingSendMessageResult::kSuccessful,
                                std::move(response));
        return base::DoNothing();
      }));

  FetchRemoteSms(
      web_contents.get(), std::vector<url::Origin>{GetOriginForURL("a.com")},
      BindLambdaForTesting(
          [&loop](std::optional<std::vector<url::Origin>>,
                  std::optional<std::string> result,
                  std::optional<content::SmsFetchFailureType> failure_type) {
            ASSERT_FALSE(result);
            ASSERT_EQ(failure_type,
                      content::SmsFetchFailureType::kPromptCancelled);
            loop.Quit();
          }));

  loop.Run();
  histogram_tester.ExpectUniqueSample(
      "Blink.Sms.Receive.CrossDeviceFailure",
      WebOTPCrossDeviceFailure::kAPIFailureOnAndroid, 1);
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sms/sms_remote_fetcher.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sharing/fake_device_info.h"
#include "chrome/browser/sharing/mock_sharing_service.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sms/sms_flags.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::BindLambdaForTesting;
using chrome_browser_sharing::ResponseMessage;
using chrome_browser_sharing::SharingMessage;
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

url::Origin GetOriginForURL(const std::string url) {
  return url::Origin::Create(GURL(url));
}

TEST(SmsRemoteFetcherTest, DisabledByDefault) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  content::WebContents::CreateParams create_params(&profile, nullptr);
  auto web_contents = content::WebContents::Create(create_params);

  base::RunLoop loop;

  FetchRemoteSms(
      web_contents.get(), GetOriginForURL("a.com"),
      BindLambdaForTesting(
          [&loop](base::Optional<std::vector<url::Origin>>,
                  base::Optional<std::string> result,
                  base::Optional<content::SmsFetchFailureType> failure_type) {
            ASSERT_FALSE(result);
            loop.Quit();
          }));

  loop.Run();
}

TEST(SmsRemoteFetcherTest, NoDevicesAvailable) {
  // This needs to be done before any tasks running on other threads check if a
  // feature is enabled.
  base::test::ScopedFeatureList flags;
  flags.InitAndEnableFeature(kWebOTPCrossDevice);

  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  content::WebContents::CreateParams create_params(&profile, nullptr);
  auto web_contents = content::WebContents::Create(create_params);

  MockSharingService* service = CreateSharingService(&profile);

  std::vector<std::unique_ptr<syncer::DeviceInfo>> devices;
  EXPECT_CALL(*service, GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));

  base::RunLoop loop;

  FetchRemoteSms(
      web_contents.get(), GetOriginForURL("a.com"),
      BindLambdaForTesting(
          [&loop](base::Optional<std::vector<url::Origin>>,
                  base::Optional<std::string> result,
                  base::Optional<content::SmsFetchFailureType> failure_type) {
            ASSERT_FALSE(result);
            loop.Quit();
          }));

  loop.Run();
}

TEST(SmsRemoteFetcherTest, OneDevice) {
  // This needs to be done before any tasks running on other threads check if a
  // feature is enabled.
  base::test::ScopedFeatureList flags;
  flags.InitAndEnableFeature(kWebOTPCrossDevice);

  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  content::WebContents::CreateParams create_params(&profile, nullptr);
  auto web_contents = content::WebContents::Create(create_params);

  MockSharingService* service = CreateSharingService(&profile);

  std::vector<std::unique_ptr<syncer::DeviceInfo>> devices;

  devices.push_back(CreateFakeDeviceInfo("guid", "name"));

  EXPECT_CALL(*service, GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));
  base::RunLoop loop;

  EXPECT_CALL(*service, SendMessageToDevice(_, _, _, _))
      .WillOnce(Invoke([&](const syncer::DeviceInfo& device_info,
                           base::TimeDelta response_timeout,
                           chrome_browser_sharing::SharingMessage message,
                           SharingMessageSender::ResponseCallback callback) {
        auto response = std::make_unique<ResponseMessage>();
        response->mutable_sms_fetch_response()->set_one_time_code("ABC");
        std::move(callback).Run(SharingSendMessageResult::kSuccessful,
                                std::move(response));
        return base::DoNothing();
      }));

  FetchRemoteSms(
      web_contents.get(), GetOriginForURL("a.com"),
      BindLambdaForTesting(
          [&loop](base::Optional<std::vector<url::Origin>>,
                  base::Optional<std::string> result,
                  base::Optional<content::SmsFetchFailureType> failure_type) {
            ASSERT_TRUE(result);
            ASSERT_EQ("ABC", result);
            loop.Quit();
          }));

  loop.Run();
}

TEST(SmsRemoteFetcherTest, OneDeviceTimesOut) {
  // This needs to be done before any tasks running on other threads check if a
  // feature is enabled.
  base::test::ScopedFeatureList flags;
  flags.InitAndEnableFeature(kWebOTPCrossDevice);

  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  content::WebContents::CreateParams create_params(&profile, nullptr);
  auto web_contents = content::WebContents::Create(create_params);

  MockSharingService* service = CreateSharingService(&profile);

  std::vector<std::unique_ptr<syncer::DeviceInfo>> devices;

  devices.push_back(CreateFakeDeviceInfo("guid", "name"));

  EXPECT_CALL(*service, GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));
  base::RunLoop loop;

  EXPECT_CALL(*service, SendMessageToDevice(_, _, _, _))
      .WillOnce(Invoke([&](const syncer::DeviceInfo& device_info,
                           base::TimeDelta response_timeout,
                           chrome_browser_sharing::SharingMessage message,
                           SharingMessageSender::ResponseCallback callback) {
        std::move(callback).Run(SharingSendMessageResult::kAckTimeout,
                                std::make_unique<ResponseMessage>());
        return base::DoNothing();
      }));

  FetchRemoteSms(
      web_contents.get(), GetOriginForURL("a.com"),
      BindLambdaForTesting(
          [&loop](base::Optional<std::vector<url::Origin>>,
                  base::Optional<std::string> result,
                  base::Optional<content::SmsFetchFailureType> failure_type) {
            ASSERT_FALSE(result);
            loop.Quit();
          }));

  loop.Run();
}

TEST(SmsRemoteFetcherTest, RequestCancelled) {
  // This needs to be done before any tasks running on other threads check if a
  // feature is enabled.
  base::test::ScopedFeatureList flags;
  flags.InitAndEnableFeature(kWebOTPCrossDevice);

  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  content::WebContents::CreateParams create_params(&profile, nullptr);
  auto web_contents = content::WebContents::Create(create_params);

  MockSharingService* service = CreateSharingService(&profile);

  std::vector<std::unique_ptr<syncer::DeviceInfo>> devices;

  devices.push_back(CreateFakeDeviceInfo("guid-abc"));

  EXPECT_CALL(*service, GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));
  base::RunLoop loop;

  base::MockOnceClosure mock_callback;
  EXPECT_CALL(*service, SendMessageToDevice(_, _, _, _))
      .WillOnce(Invoke([&](const syncer::DeviceInfo& device_info,
                           base::TimeDelta response_timeout,
                           chrome_browser_sharing::SharingMessage message,
                           SharingMessageSender::ResponseCallback callback) {
        std::move(callback).Run(SharingSendMessageResult::kCancelled,
                                std::make_unique<ResponseMessage>());
        return mock_callback.Get();
      }));

  base::OnceClosure cancel_callback = FetchRemoteSms(
      web_contents.get(), GetOriginForURL("a.com"),
      BindLambdaForTesting(
          [&loop](base::Optional<std::vector<url::Origin>>,
                  base::Optional<std::string> one_time_code,
                  base::Optional<content::SmsFetchFailureType> failure_type) {
            ASSERT_FALSE(one_time_code);
            loop.Quit();
          }));

  EXPECT_CALL(mock_callback, Run);
  std::move(cancel_callback).Run();

  loop.Run();
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/tips_agent_android.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/notifications/scheduler/test/mock_notification_schedule_service.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tips/core/tips_service.h"
#include "chrome/browser/tips/core/tips_types.h"
#include "chrome/browser/tips/tips_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

class MockTipsService : public tips::TipsService {
 public:
  MockTipsService() : tips::TipsService(nullptr, nullptr) {}
  ~MockTipsService() override = default;

  MOCK_METHOD(void, DetermineBestTip, (OnBestTipChosen), (override));
};

class TipsAgentAndroidTest : public testing::Test {
 protected:
  TipsAgentAndroidTest() = default;
  ~TipsAgentAndroidTest() override = default;

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              segmentation_platform::MockSegmentationPlatformService>();
        }));
    builder.AddTestingFactory(
        tips::TipsServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<MockTipsService>();
        }));
    profile_ = builder.Build();

    mock_segmentation_service_ =
        static_cast<segmentation_platform::MockSegmentationPlatformService*>(
            segmentation_platform::SegmentationPlatformServiceFactory::
                GetForProfile(profile_.get()));
    mock_tips_service_ = static_cast<MockTipsService*>(
        tips::TipsServiceFactory::GetForProfile(profile_.get()));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<segmentation_platform::MockSegmentationPlatformService>
      mock_segmentation_service_ = nullptr;
  raw_ptr<MockTipsService> mock_tips_service_ = nullptr;
  notifications::test::MockNotificationScheduleService mock_service_;
};

TEST_F(TipsAgentAndroidTest, TestScheduleNewNotification) {
  // Verify that GetClassificationResult is called by default.
  EXPECT_CALL(*mock_segmentation_service_, GetClassificationResult(_, _, _, _))
      .Times(1);
  EXPECT_CALL(*mock_tips_service_, DetermineBestTip(_)).Times(0);

  TipsAgentAndroid::ScheduleNewNotification(
      profile_.get(), /*is_bottom_omnibox=*/false, &mock_service_);
}

TEST_F(TipsAgentAndroidTest,
       TestScheduleNewNotification_SelfServiceEnabled_TipSelected) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chrome::android::kTipsSelfService);

  EXPECT_CALL(*mock_segmentation_service_, GetClassificationResult(_, _, _, _))
      .Times(0);
  EXPECT_CALL(*mock_tips_service_, DetermineBestTip(_))
      .WillOnce([](tips::TipsService::OnBestTipChosen callback) {
        std::move(callback).Run(
            tips::TipsNotificationsFeatureType::kEnhancedSafeBrowsing);
      });
  EXPECT_CALL(mock_service_, Schedule(_)).Times(1);

  TipsAgentAndroid::ScheduleNewNotification(
      profile_.get(), /*is_bottom_omnibox=*/false, &mock_service_);
}

TEST_F(TipsAgentAndroidTest,
       TestScheduleNewNotification_SelfServiceEnabled_NoTipSelected) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chrome::android::kTipsSelfService);

  EXPECT_CALL(*mock_segmentation_service_, GetClassificationResult(_, _, _, _))
      .Times(0);
  EXPECT_CALL(*mock_tips_service_, DetermineBestTip(_))
      .WillOnce([](tips::TipsService::OnBestTipChosen callback) {
        std::move(callback).Run(std::nullopt);
      });
  EXPECT_CALL(mock_service_, Schedule(_)).Times(0);

  TipsAgentAndroid::ScheduleNewNotification(
      profile_.get(), /*is_bottom_omnibox=*/false, &mock_service_);
}

TEST_F(TipsAgentAndroidTest, TestOnBestTipChosen) {
  EXPECT_CALL(mock_service_, Schedule(_)).Times(1);
  TipsAgentAndroid::OnBestTipChosen(
      &mock_service_,
      tips::TipsNotificationsFeatureType::kEnhancedSafeBrowsing);

  EXPECT_CALL(mock_service_, Schedule(_)).Times(0);
  TipsAgentAndroid::OnBestTipChosen(&mock_service_, std::nullopt);
}

TEST_F(TipsAgentAndroidTest, TestOnGetClientOverview_Reschedule) {
  notifications::ClientOverview overview;
  notifications::NotificationEntry entry(
      notifications::SchedulerClientType::kTips, "guid");
  overview.scheduled_notifications.push_back(&entry);

  EXPECT_CALL(mock_service_,
              DeleteNotifications(notifications::SchedulerClientType::kTips));
  EXPECT_CALL(mock_service_, Schedule(_));

  TipsAgentAndroid::OnGetClientOverview(profile_.get(),
                                        /*is_bottom_omnibox=*/false,
                                        &mock_service_, std::move(overview));
}

TEST_F(TipsAgentAndroidTest, TestOnGetClientOverview_ScheduleNew) {
  notifications::ClientOverview overview;

  // Should call ScheduleNewNotification, which calls segmentation service.
  EXPECT_CALL(*mock_segmentation_service_, GetClassificationResult(_, _, _, _))
      .Times(1);

  TipsAgentAndroid::OnGetClientOverview(profile_.get(),
                                        /*is_bottom_omnibox=*/false,
                                        &mock_service_, std::move(overview));
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/tips_agent_android.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/notifications/scheduler/test/mock_notification_schedule_service.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

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
    profile_ = builder.Build();

    mock_segmentation_service_ =
        static_cast<segmentation_platform::MockSegmentationPlatformService*>(
            segmentation_platform::SegmentationPlatformServiceFactory::
                GetForProfile(profile_.get()));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<segmentation_platform::MockSegmentationPlatformService>
      mock_segmentation_service_ = nullptr;
  notifications::test::MockNotificationScheduleService mock_service_;
};

TEST_F(TipsAgentAndroidTest, TestScheduleNewNotification) {
  // Verify that GetClassificationResult is called.
  EXPECT_CALL(*mock_segmentation_service_, GetClassificationResult(_, _, _, _))
      .Times(1);

  TipsAgentAndroid::ScheduleNewNotification(
      profile_.get(), /*is_bottom_omnibox=*/false, &mock_service_);
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

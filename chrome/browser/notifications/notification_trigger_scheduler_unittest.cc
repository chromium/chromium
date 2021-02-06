// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/notifications/notification_trigger_scheduler.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace {

std::unique_ptr<TestingProfileManager> CreateTestingProfileManager() {
  std::unique_ptr<TestingProfileManager> profile_manager(
      new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
  EXPECT_TRUE(profile_manager->SetUp());
  return profile_manager;
}

class MockNotificationTriggerScheduler : public NotificationTriggerScheduler {
 public:
  ~MockNotificationTriggerScheduler() override = default;
  MOCK_METHOD1(TriggerNotificationsForStoragePartition,
               void(content::StoragePartition* partition));
};

}  // namespace

class NotificationTriggerSchedulerTest : public testing::Test {
 protected:
  NotificationTriggerSchedulerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  class ProfileTestData {
   public:
    ProfileTestData(TestingProfileManager* profile_manager,
                    const std::string& profile_name)
        : profile_(profile_manager->CreateTestingProfile(profile_name)),
          service_(PlatformNotificationServiceFactory::GetForProfile(profile_)),
          scheduler_(new MockNotificationTriggerScheduler()) {
      service_->trigger_scheduler_ = base::WrapUnique(scheduler_);
    }

    // Owned by TestingProfileManager.
    Profile* profile_;
    // Owned by PlatformNotificationServiceFactory.
    PlatformNotificationServiceImpl* service_;
    // Owned by |service_|.
    MockNotificationTriggerScheduler* scheduler_;
  };

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(NotificationTriggerSchedulerTest,
       TriggerNotificationsCallsAllStoragePartitions) {
  std::unique_ptr<TestingProfileManager> profile_manager =
      CreateTestingProfileManager();
  ProfileTestData data1(profile_manager.get(), "profile1");
  ProfileTestData data2(profile_manager.get(), "profile2");

  EXPECT_CALL(*data1.scheduler_, TriggerNotificationsForStoragePartition(_))
      .Times(0);
  EXPECT_CALL(*data2.scheduler_, TriggerNotificationsForStoragePartition(_))
      .Times(0);

  auto* partition1 = content::BrowserContext::GetStoragePartitionForSite(
      data1.profile_, GURL("http://example.com"));
  auto* partition2 = content::BrowserContext::GetStoragePartitionForSite(
      data2.profile_, GURL("http://example.com"));

  auto now = base::Time::Now();
  auto delta = base::TimeDelta::FromSeconds(3);
  data1.service_->ScheduleTrigger(now + delta);
  data2.service_->ScheduleTrigger(now + delta);
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(data1.scheduler_);
  testing::Mock::VerifyAndClearExpectations(data2.scheduler_);

  EXPECT_CALL(*data1.scheduler_,
              TriggerNotificationsForStoragePartition(partition1));
  EXPECT_CALL(*data2.scheduler_,
              TriggerNotificationsForStoragePartition(partition2));

  task_environment_.FastForwardBy(delta);
  base::RunLoop().RunUntilIdle();
}

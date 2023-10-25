// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
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
      service_->trigger_scheduler_ = base::WrapUnique(scheduler_.get());
    }

    // Owned by TestingProfileManager.
    raw_ptr<Profile> profile_;
    // Owned by PlatformNotificationServiceFactory.
    raw_ptr<PlatformNotificationServiceImpl> service_;
    // Owned by |service_|.
    raw_ptr<MockNotificationTriggerScheduler> scheduler_;
  };

  content::BrowserTaskEnvironment task_environment_;
};

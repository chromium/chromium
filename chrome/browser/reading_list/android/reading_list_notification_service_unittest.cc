// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reading_list/android/reading_list_notification_service.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/test/mock_notification_schedule_service.h"
#include "chrome/browser/reading_list/android/reading_list_notification_delegate.h"
#include "components/reading_list/core/fake_reading_list_model_storage.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/sync/base/storage_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using notifications::ClientOverview;
using notifications::SchedulerClientType;
using notifications::test::MockNotificationScheduleService;
using reading_list::switches::kReadLater;
using reading_list::switches::kReadLaterReminderNotification;
using testing::_;
using testing::Invoke;

namespace {

constexpr notifications::SchedulerClientType kNotificationType =
    notifications::SchedulerClientType::kReadingList;
const char kNow[] = "10 Feb 2020 01:00:00";
const char kShowTime[] = "17 Feb 2020 08:00:00";

MATCHER_P(DeliverOnTime, time, "") {
  return time == arg->schedule_params.deliver_time_start.value();
}

class MockDelegate : public ReadingListNotificationDelegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;
  MOCK_METHOD(std::u16string, getNotificationTitle, (), (override));
  MOCK_METHOD(std::u16string, getNotificationSubTitle, (int), (override));
  MOCK_METHOD(void, OpenReadingListPage, (), (override));
};

class ReadingListNotificationServiceTest : public testing::Test {
 public:
  ReadingListNotificationServiceTest() = default;
  ~ReadingListNotificationServiceTest() override = default;

  void SetUp() override {
    clock_.SetNow(MakeLocalTime(kNow));
    auto storage = std::make_unique<FakeReadingListModelStorage>();
    base::WeakPtr<FakeReadingListModelStorage> storage_ptr =
        storage->AsWeakPtr();
    reading_list_model_ = std::make_unique<ReadingListModelImpl>(
        std::move(storage), syncer::StorageType::kUnspecified, &clock_);
    // Complete the initial model load from storage.
    storage_ptr->TriggerLoadCompletion();

    auto delegate = std::make_unique<MockDelegate>();
    delegate_ = delegate.get();
    auto config = std::make_unique<ReadingListNotificationService::Config>();
    config->notification_show_time = 8;
    service_ = std::make_unique<ReadingListNotificationServiceImpl>(
        reading_list_model_.get(), &mock_schedule_service_, std::move(delegate),
        std::move(config), &clock_);
  }

 protected:
  base::test::TaskEnvironment* task_environment() { return &task_environment_; }
  ReadingListNotificationServiceImpl* service() { return service_.get(); }
  MockDelegate* delegate() { return delegate_; }
  base::SimpleTestClock* clock() { return &clock_; }
  MockNotificationScheduleService* mock_schedule_service() {
    return &mock_schedule_service_;
  }
  ReadingListModelImpl* reading_list_model() {
    return reading_list_model_.get();
  }

  base::Time MakeLocalTime(const char* time_str) {
    base::Time time;
    EXPECT_TRUE(base::Time::FromString(time_str, &time));
    return time;
  }

  void WillInvokeOnClientOverview(size_t scheduled_size) {
    ON_CALL(*mock_schedule_service(), GetClientOverview(_, _))
        .WillByDefault(
            Invoke([scheduled_size](
                       SchedulerClientType type,
                       notifications::ClientOverview::ClientOverviewCallback
                           callback) {
              ClientOverview overview;
              overview.num_scheduled_notifications = scheduled_size;
              std::move(callback).Run(overview);
            }));
  }

  void AddReadingList() {
    reading_list_model()->AddOrReplaceEntry(
        GURL("https://a.example.com"), "title",
        reading_list::ADDED_VIA_CURRENT_APP,
        /*estimated_read_time=*/base::TimeDelta());
  }

 private:
  base::SimpleTestClock clock_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ReadingListModelImpl> reading_list_model_;
  raw_ptr<MockDelegate> delegate_;
  MockNotificationScheduleService mock_schedule_service_;
  std::unique_ptr<ReadingListNotificationServiceImpl> service_;
};

TEST_F(ReadingListNotificationServiceTest, OnClick) {
  EXPECT_CALL(*delegate(), OpenReadingListPage());
  service()->OnClick();
  task_environment()->RunUntilIdle();
}

TEST_F(ReadingListNotificationServiceTest, OnStartWithoutUnreadPages) {
  EXPECT_EQ(0u, reading_list_model()->unread_size());
  EXPECT_CALL(*mock_schedule_service(), DeleteNotifications(kNotificationType));
  service()->OnStart();
  task_environment()->RunUntilIdle();
}

TEST_F(ReadingListNotificationServiceTest, OnStartWithNotificationScheduled) {
  AddReadingList();
  EXPECT_EQ(1u, reading_list_model()->unread_size());
  EXPECT_CALL(*mock_schedule_service(), DeleteNotifications(kNotificationType))
      .Times(0);
  EXPECT_CALL(*mock_schedule_service(), Schedule(_)).Times(0);
  WillInvokeOnClientOverview(/*scheduled_size=*/1u);
  service()->OnStart();
  task_environment()->RunUntilIdle();
}

TEST_F(ReadingListNotificationServiceTest,
       OnStartWithoutNotificationScheduled) {
  AddReadingList();
  EXPECT_EQ(1u, reading_list_model()->unread_size());
  EXPECT_CALL(*mock_schedule_service(), DeleteNotifications(kNotificationType))
      .Times(0);
  EXPECT_CALL(*mock_schedule_service(),
              Schedule(DeliverOnTime(MakeLocalTime(kShowTime))));
  WillInvokeOnClientOverview(/*scheduled_size=*/0u);
  service()->OnStart();
  task_environment()->RunUntilIdle();
}

TEST_F(ReadingListNotificationServiceTest, BeforeShowWithoutUnreadPages) {
  EXPECT_EQ(0u, reading_list_model()->unread_size());
  EXPECT_CALL(*mock_schedule_service(), DeleteNotifications(kNotificationType));
  EXPECT_CALL(*mock_schedule_service(), Schedule(_)).Times(0);
  WillInvokeOnClientOverview(/*scheduled_size=*/1u);
  base::RunLoop loop;
  auto notif_data = std::make_unique<notifications::NotificationData>();
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<notifications::NotificationData> data) {
        EXPECT_EQ(nullptr, data) << "The notification should be canceled.";
        loop.Quit();
      });
  service()->BeforeShowNotification(std::move(notif_data), std::move(callback));
  loop.Run();
  task_environment()->RunUntilIdle();
}

TEST_F(ReadingListNotificationServiceTest,
       BeforeShowWithUnreadPageWithoutScheduled) {
  AddReadingList();
  EXPECT_EQ(1u, reading_list_model()->unread_size());
  EXPECT_CALL(*mock_schedule_service(), DeleteNotifications(kNotificationType))
      .Times(0);
  EXPECT_CALL(*mock_schedule_service(), Schedule(_));
  WillInvokeOnClientOverview(/*scheduled_size=*/0u);
  base::RunLoop loop;
  auto notif_data = std::make_unique<notifications::NotificationData>();
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<notifications::NotificationData> data) {
        EXPECT_NE(nullptr, data) << "The notification should not be canceled.";
        loop.Quit();
      });
  service()->BeforeShowNotification(std::move(notif_data), std::move(callback));
  loop.Run();
  task_environment()->RunUntilIdle();
}

TEST_F(ReadingListNotificationServiceTest,
       BeforeShowWithUnreadPageWithScheduled) {
  AddReadingList();
  EXPECT_EQ(1u, reading_list_model()->unread_size());
  EXPECT_CALL(*mock_schedule_service(), DeleteNotifications(kNotificationType))
      .Times(0);
  EXPECT_CALL(*mock_schedule_service(), Schedule(_)).Times(0);
  WillInvokeOnClientOverview(/*scheduled_size=*/1u);
  base::RunLoop loop;
  auto notif_data = std::make_unique<notifications::NotificationData>();
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<notifications::NotificationData> data) {
        EXPECT_NE(nullptr, data) << "The notification should not be canceled.";
        loop.Quit();
      });
  service()->BeforeShowNotification(std::move(notif_data), std::move(callback));
  loop.Run();
  task_environment()->RunUntilIdle();
}

TEST_F(ReadingListNotificationServiceTest, CacheClosure) {
  auto* cached_closure = service()->GetCachedClosureForTesting();
  base::RunLoop loop;
  auto closure = base::BindLambdaForTesting([&]() { loop.Quit(); });
  cached_closure->emplace(std::move(closure));

  // Flush the cached closures.
  service()->ReadingListModelLoaded(reading_list_model());
  loop.Run();
}

TEST_F(ReadingListNotificationServiceTest, IsEnabled) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({kReadLater},
                                  {kReadLaterReminderNotification});
    EXPECT_FALSE(ReadingListNotificationService::IsEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({},
                                  {kReadLater, kReadLaterReminderNotification});
    EXPECT_FALSE(ReadingListNotificationService::IsEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({kReadLater, kReadLaterReminderNotification},
                                  {});
    EXPECT_TRUE(ReadingListNotificationService::IsEnabled());
  }
}

}  // namespace

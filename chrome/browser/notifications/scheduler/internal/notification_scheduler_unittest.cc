// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/notification_scheduler.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/internal/notification_scheduler_context.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client_registrar.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/browser/notifications/scheduler/test/mock_background_task_coordinator.h"
#include "chrome/browser/notifications/scheduler/test/mock_display_agent.h"
#include "chrome/browser/notifications/scheduler/test/mock_display_decider.h"
#include "chrome/browser/notifications/scheduler/test/mock_impression_history_tracker.h"
#include "chrome/browser/notifications/scheduler/test/mock_notification_background_task_scheduler.h"
#include "chrome/browser/notifications/scheduler/test/mock_notification_scheduler_client.h"
#include "chrome/browser/notifications/scheduler/test/mock_scheduled_notification_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::SetArgPointee;

namespace notifications {
namespace {

const char kGuid[] = "guid";
const char16_t kTitle[] = u"title";

class NotificationSchedulerTest : public testing::Test {
 public:
  NotificationSchedulerTest()
      : registrar_(nullptr),
        impression_tracker_(nullptr),
        notification_manager_(nullptr),
        client_(nullptr),
        task_coordinator_(nullptr),
        display_agent_(nullptr),
        display_decider_(nullptr) {}
  NotificationSchedulerTest(const NotificationSchedulerTest&) = delete;
  NotificationSchedulerTest& operator=(const NotificationSchedulerTest&) =
      delete;
  ~NotificationSchedulerTest() override = default;

  void SetUp() override {
    auto registrar = std::make_unique<NotificationSchedulerClientRegistrar>();
    auto task_coordinator =
        std::make_unique<test::MockBackgroundTaskCoordinator>();
    auto impression_tracker =
        std::make_unique<NiceMock<test::MockImpressionHistoryTracker>>();
    auto notification_manager =
        std::make_unique<NiceMock<test::MockScheduledNotificationManager>>();
    auto display_agent = std::make_unique<test::MockDisplayAgent>();
    auto display_decider = std::make_unique<test::MockDisplayDecider>();
    auto config = SchedulerConfig::Create();

    registrar_ = registrar.get();
    impression_tracker_ = impression_tracker.get();
    notification_manager_ = notification_manager.get();
    task_coordinator_ = task_coordinator.get();
    display_agent_ = display_agent.get();
    display_decider_ = display_decider.get();

    // Register mock clients.
    auto client = std::make_unique<test::MockNotificationSchedulerClient>();
    client_ = client.get();
    registrar_->RegisterClient(SchedulerClientType::kTest1, std::move(client));

    auto context = std::make_unique<NotificationSchedulerContext>(
        std::move(registrar), std::move(task_coordinator),
        std::move(impression_tracker), std::move(notification_manager),
        std::move(display_agent), std::move(display_decider),
        std::move(config));
    notification_scheduler_ = NotificationScheduler::Create(std::move(context));
  }

 protected:
  void Init() {
    EXPECT_CALL(*impression_tracker(), Init(_, _))
        .WillOnce(Invoke([&](ImpressionHistoryTracker::Delegate* delegate,
                             ImpressionHistoryTracker::InitCallback callback) {
          std::move(callback).Run(true);
        }));

    EXPECT_CALL(*notification_manager(), Init(_))
        .WillOnce(
            Invoke([&](ScheduledNotificationManager::InitCallback callback) {
              std::move(callback).Run(true);
            }));

    base::RunLoop run_loop;
    scheduler()->Init(
        base::BindOnce([](bool success) { EXPECT_TRUE(success); }));

    EXPECT_CALL(*client(), OnSchedulerInitialized(true, _))
        .WillOnce(InvokeWithoutArgs([&]() { run_loop.Quit(); }));

    run_loop.Run();
  }

  // Starts the background task and wait for task finished callback to invoke.
  void OnStartTask() {
    base::RunLoop loop;
    auto task_finish_callback =
        base::BindOnce([](base::RepeatingClosure quit_closure,
                          bool needs_reschedule) { quit_closure.Run(); },
                       loop.QuitClosure());
    scheduler()->OnStartTask(std::move(task_finish_callback));
    loop.Run();
  }

  NotificationScheduler* scheduler() { return notification_scheduler_.get(); }

  test::MockImpressionHistoryTracker* impression_tracker() {
    return impression_tracker_;
  }

  test::MockScheduledNotificationManager* notification_manager() {
    return notification_manager_;
  }

  test::MockNotificationSchedulerClient* client() { return client_; }

  test::MockBackgroundTaskCoordinator* task_coordinator() {
    return task_coordinator_;
  }

  test::MockDisplayAgent* display_agent() { return display_agent_; }

  test::MockDisplayDecider* display_decider() { return display_decider_; }

 private:
  base::test::TaskEnvironment task_environment_;
  raw_ptr<NotificationSchedulerClientRegistrar, DanglingUntriaged> registrar_;
  raw_ptr<test::MockImpressionHistoryTracker, DanglingUntriaged>
      impression_tracker_;
  raw_ptr<test::MockScheduledNotificationManager, DanglingUntriaged>
      notification_manager_;
  raw_ptr<test::MockNotificationSchedulerClient, DanglingUntriaged> client_;
  raw_ptr<test::MockBackgroundTaskCoordinator, DanglingUntriaged>
      task_coordinator_;
  raw_ptr<test::MockDisplayAgent, DanglingUntriaged> display_agent_;
  raw_ptr<test::MockDisplayDecider, DanglingUntriaged> display_decider_;

  std::unique_ptr<NotificationScheduler> notification_scheduler_;
};

// Tests successful initialization flow.
TEST_F(NotificationSchedulerTest, InitSuccess) {
  Init();
}

// Tests the case when impression tracker failed to initialize.
TEST_F(NotificationSchedulerTest, InitImpressionTrackerFailed) {
  EXPECT_CALL(*impression_tracker(), Init(_, _))
      .WillOnce(Invoke([](ImpressionHistoryTracker::Delegate* delegate,
                          ImpressionHistoryTracker::InitCallback callback) {
        // Impression tracker failed to load.
        std::move(callback).Run(false);
      }));

  EXPECT_CALL(*notification_manager(), Init(_)).Times(0);

  base::RunLoop run_loop;
  scheduler()->Init(
      base::BindOnce([](bool success) { EXPECT_FALSE(success); }));

  EXPECT_CALL(*client(), OnSchedulerInitialized(false, _))
      .WillOnce(InvokeWithoutArgs([&]() { run_loop.Quit(); }));

  run_loop.Run();
}

// Tests the case when scheduled notification manager failed to initialize.
TEST_F(NotificationSchedulerTest, InitScheduledNotificationManagerFailed) {
  EXPECT_CALL(*impression_tracker(), Init(_, _))
      .WillOnce(Invoke([](ImpressionHistoryTracker::Delegate* delegate,
                          ImpressionHistoryTracker::InitCallback callback) {
        std::move(callback).Run(true);
      }));

  EXPECT_CALL(*notification_manager(), Init(_))
      .WillOnce(Invoke([](ScheduledNotificationManager::InitCallback callback) {
        // Scheduled notification manager failed to load.
        std::move(callback).Run(false);
      }));

  base::RunLoop run_loop;
  scheduler()->Init(
      base::BindOnce([](bool success) { EXPECT_FALSE(success); }));

  EXPECT_CALL(*client(), OnSchedulerInitialized(false, _))
      .WillOnce(InvokeWithoutArgs([&]() { run_loop.Quit(); }));

  run_loop.Run();
}

// Test to schedule a notification.
TEST_F(NotificationSchedulerTest, Schedule) {
  Init();
  auto param = std::unique_ptr<NotificationParams>();
  EXPECT_CALL(*notification_manager(), ScheduleNotification(_, _))
      .WillOnce(
          Invoke([](std::unique_ptr<NotificationParams>,
                    ScheduledNotificationManager::ScheduleCallback callback) {
            std::move(callback).Run(true);
          }));
  EXPECT_CALL(*task_coordinator(), ScheduleBackgroundTask(_, _));
  scheduler()->Schedule(std::move(param));
}

// When failed to add to the scheduled notification manager, no background task
// is triggered.
TEST_F(NotificationSchedulerTest, ScheduleFailed) {
  Init();
  auto param = std::unique_ptr<NotificationParams>();
  EXPECT_CALL(*notification_manager(), ScheduleNotification(_, _))
      .WillOnce(
          Invoke([](std::unique_ptr<NotificationParams>,
                    ScheduledNotificationManager::ScheduleCallback callback) {
            std::move(callback).Run(false);
          }));
  EXPECT_CALL(*task_coordinator(), ScheduleBackgroundTask(_, _)).Times(0);
  scheduler()->Schedule(std::move(param));
}

// Test to delete notifications.
TEST_F(NotificationSchedulerTest, DeleteAllNotifications) {
  Init();

  // Currently we don't reschedule background task even if all the notifications
  // are deleted.
  EXPECT_CALL(*task_coordinator(), ScheduleBackgroundTask(_, _)).Times(0);
  EXPECT_CALL(*notification_manager(),
              DeleteNotifications(SchedulerClientType::kTest1));
  scheduler()->DeleteAllNotifications(SchedulerClientType::kTest1);
}

// Test to get client overview.
TEST_F(NotificationSchedulerTest, GetClientOverview) {
  Init();
  EXPECT_CALL(*impression_tracker(),
              GetImpressionDetail(SchedulerClientType::kTest1, _))
      .WillOnce(Invoke([](SchedulerClientType type,
                          ImpressionDetail::ImpressionDetailCallback callback) {
        std::move(callback).Run(ImpressionDetail());
      }));
  EXPECT_CALL(*notification_manager(), GetNotifications(_, _));
  scheduler()->GetClientOverview(SchedulerClientType::kTest1,
                                 base::DoNothing());
}

// Test to verify user actions are propagated through correctly.
TEST_F(NotificationSchedulerTest, OnUserAction) {
  Init();

  base::RunLoop loop;
  UserActionData action_data(SchedulerClientType::kTest1,
                             UserActionType::kButtonClick, kGuid);
  EXPECT_CALL(*impression_tracker(), OnUserAction(action_data));
  EXPECT_CALL(*task_coordinator(), ScheduleBackgroundTask(_, _)).Times(1);
  EXPECT_CALL(*client(), OnUserAction(_)).WillOnce(InvokeWithoutArgs([&]() {
    loop.Quit();
  }));

  scheduler()->OnUserAction(action_data);
  loop.Run();
}

// Test to simulate a background task flow without any notification shown.
TEST_F(NotificationSchedulerTest, BackgroundTaskStartShowNothing) {
  Init();

  // No notification picked to show.
  DisplayDecider::Results result;
  EXPECT_CALL(*display_decider(), FindNotificationsToShow(_, _, _))
      .WillOnce(SetArgPointee<2>(result));

  EXPECT_CALL(*display_agent(), ShowNotification(_, _)).Times(0);
  EXPECT_CALL(*notification_manager(), DisplayNotification(_, _)).Times(0);
  EXPECT_CALL(*task_coordinator(), ScheduleBackgroundTask(_, _));

  OnStartTask();
}

MATCHER_P(NotificationDataEq, title, "Verify notification data.") {
  EXPECT_EQ(arg->title, title);
  return true;
}

MATCHER_P2(SystemDataEq, type, guid, "Verify system data.") {
  EXPECT_EQ(arg->type, type);
  EXPECT_EQ(arg->guid, guid);
  return true;
}

// Test to simulate a background task flow with some notifications shown.
TEST_F(NotificationSchedulerTest, BackgroundTaskStartShowNotification) {
  Init();

  // Mock the notification to show.
  auto entry =
      std::make_unique<NotificationEntry>(SchedulerClientType::kTest1, kGuid);
  EXPECT_CALL(
      *display_agent(),
      ShowNotification(NotificationDataEq(kTitle),
                       SystemDataEq(SchedulerClientType::kTest1, kGuid)));
  DisplayDecider::Results result({kGuid});
  EXPECT_CALL(*display_decider(), FindNotificationsToShow(_, _, _))
      .WillOnce(SetArgPointee<2>(result));

  EXPECT_CALL(*task_coordinator(), ScheduleBackgroundTask(_, _));
  EXPECT_CALL(*impression_tracker(), AddImpression(_, _, _, _, _));
  EXPECT_CALL(*notification_manager(), DisplayNotification(_, _))
      .WillOnce(
          Invoke([&](const std::string& guid,
                     ScheduledNotificationManager::DisplayCallback callback) {
            std::move(callback).Run(std::move(entry));
          }));

  EXPECT_CALL(*client(), BeforeShowNotification(_, _))
      .WillOnce(Invoke(
          [&](std::unique_ptr<NotificationData> notification_data,
              NotificationSchedulerClient::NotificationDataCallback callback) {
            // The client updates the notification data here.
            notification_data->title = kTitle;
            std::move(callback).Run(std::move(notification_data));
          }));

  OnStartTask();
}

// Verifies if the entry is failed to load, the background task flow can still
// be finished.
TEST_F(NotificationSchedulerTest, BackgroundTaskStartNoEntry) {
  Init();

  DisplayDecider::Results result({kGuid});
  EXPECT_CALL(*display_decider(), FindNotificationsToShow(_, _, _))
      .WillOnce(SetArgPointee<2>(result));

  EXPECT_CALL(*task_coordinator(), ScheduleBackgroundTask(_, _));
  EXPECT_CALL(*impression_tracker(), AddImpression(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*display_agent(), ShowNotification(_, _)).Times(0);
  EXPECT_CALL(*client(), BeforeShowNotification(_, _)).Times(0);
  EXPECT_CALL(*notification_manager(), DisplayNotification(_, _))
      .WillOnce(
          Invoke([&](const std::string& guid,
                     ScheduledNotificationManager::DisplayCallback callback) {
            std::move(callback).Run(nullptr /*entry*/);
          }));

  OnStartTask();
}

// Verifies if the client is not found during display flow, the background task
// flow can still be finished.
TEST_F(NotificationSchedulerTest, BackgroundTaskStartNoClient) {
  Init();

  // Creates entry without corresponding client.
  auto entry_no_client =
      std::make_unique<NotificationEntry>(SchedulerClientType::kTest2, kGuid);

  DisplayDecider::Results result({kGuid});
  EXPECT_CALL(*display_decider(), FindNotificationsToShow(_, _, _))
      .WillOnce(SetArgPointee<2>(result));

  EXPECT_CALL(*task_coordinator(), ScheduleBackgroundTask(_, _));
  EXPECT_CALL(*impression_tracker(), AddImpression(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*display_agent(), ShowNotification(_, _)).Times(0);
  EXPECT_CALL(*client(), BeforeShowNotification(_, _)).Times(0);
  EXPECT_CALL(*notification_manager(), DisplayNotification(_, _))
      .WillOnce(
          Invoke([&](const std::string& guid,
                     ScheduledNotificationManager::DisplayCallback callback) {
            std::move(callback).Run(std::move(entry_no_client));
          }));

  OnStartTask();
}

// Verifies the case that the client dropped the notification data.
TEST_F(NotificationSchedulerTest, ClientDropNotification) {
  Init();

  // Mock the notification to show.
  auto entry =
      std::make_unique<NotificationEntry>(SchedulerClientType::kTest1, kGuid);
  DisplayDecider::Results result({kGuid});
  EXPECT_CALL(*display_decider(), FindNotificationsToShow(_, _, _))
      .WillOnce(SetArgPointee<2>(result));
  EXPECT_CALL(*notification_manager(), DisplayNotification(_, _))
      .WillOnce(
          Invoke([&](const std::string& guid,
                     ScheduledNotificationManager::DisplayCallback callback) {
            std::move(callback).Run(std::move(entry));
          }));

  // The client drops the notification data before showing the notification.
  EXPECT_CALL(*client(), BeforeShowNotification(_, _))
      .WillOnce(Invoke(
          [&](std::unique_ptr<NotificationData> notification_data,
              NotificationSchedulerClient::NotificationDataCallback callback) {
            std::move(callback).Run(nullptr);
          }));

  EXPECT_CALL(*task_coordinator(), ScheduleBackgroundTask(_, _));
  EXPECT_CALL(*impression_tracker(), AddImpression(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*display_agent(), ShowNotification(_, _)).Times(0);

  OnStartTask();
}

// Test to simulate a background task stopped by the OS.
TEST_F(NotificationSchedulerTest, BackgroundTaskStop) {
  Init();
  EXPECT_CALL(*task_coordinator(), ScheduleBackgroundTask(_, _));
  scheduler()->OnStopTask();
}

}  // namespace
}  // namespace notifications

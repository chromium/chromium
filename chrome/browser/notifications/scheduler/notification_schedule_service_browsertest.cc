// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/notifications/scheduler/notification_background_task_scheduler_impl.h"
#include "chrome/browser/notifications/scheduler/public/display_agent.h"
#include "chrome/browser/notifications/scheduler/public/features.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client_registrar.h"
#include "chrome/browser/notifications/scheduler/schedule_service_factory_helper.h"
#include "chrome/browser/notifications/scheduler/test/mock_notification_background_task_scheduler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"

using ::testing::_;

namespace notifications {
namespace {

const base::FilePath::CharType kTestDir[] =
    FILE_PATH_LITERAL("NotificationScheduleServiceTest");

class TestClient : public NotificationSchedulerClient {
 public:
  TestClient() {}
  TestClient(const TestClient&) = delete;
  TestClient& operator=(const TestClient&) = delete;
  ~TestClient() override = default;

  const std::vector<NotificationData>& shown_notification_data() const {
    return shown_notification_data_;
  }

 private:
  // NotificationSchedulerClient implementation.
  void BeforeShowNotification(
      std::unique_ptr<NotificationData> notification_data,
      NotificationDataCallback callback) override {
    if (notification_data)
      shown_notification_data_.emplace_back(*notification_data);
    std::move(callback).Run(std::move(notification_data));
  }

  void OnSchedulerInitialized(bool success,
                              std::set<std::string> guids) override {
    DCHECK(success);
  }

  void OnUserAction(const UserActionData& action_data) override {}

  void GetThrottleConfig(ThrottleConfigCallback callback) override {
    std::move(callback).Run(nullptr);
  }

  // Any NotificationData received before showing the notification.
  std::vector<NotificationData> shown_notification_data_;
};

class TestBackgroundTaskScheduler : public NotificationBackgroundTaskScheduler {
 public:
  TestBackgroundTaskScheduler() = default;
  TestBackgroundTaskScheduler(const TestBackgroundTaskScheduler&) = delete;
  TestBackgroundTaskScheduler& operator=(const TestBackgroundTaskScheduler&) =
      delete;
  ~TestBackgroundTaskScheduler() override = default;

  // Waits until a background task has been updated.
  void WaitForTaskUpdated() {
    DCHECK(!run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  test::MockNotificationBackgroundTaskScheduler* mock_background_task() {
    return &mock_background_task_;
  }

 private:
  void QuitRunLoopIfNeeded() {
    if (run_loop_ && run_loop_->running()) {
      run_loop_->Quit();
    }
  }

  // NotificationBackgroundTaskScheduler implementation.
  void Schedule(base::TimeDelta window_start,
                base::TimeDelta window_end) override {
    QuitRunLoopIfNeeded();
    mock_background_task_.Schedule(window_start, window_end);
  }

  void Cancel() override {
    QuitRunLoopIfNeeded();
    mock_background_task_.Cancel();
  }

  std::unique_ptr<base::RunLoop> run_loop_;

  // Delegates to a mock to setup call expectations.
  test::MockNotificationBackgroundTaskScheduler mock_background_task_;
};

// Browser test for notification scheduling system. Uses real database
// instances. Mainly to cover service initialization flow in chrome layer.
class NotificationScheduleServiceTest : public InProcessBrowserTest {
 public:
  NotificationScheduleServiceTest() : task_scheduler_(nullptr) {
    scoped_feature_list_.InitWithFeatures(
        {features::kNotificationScheduleService}, {});
  }
  NotificationScheduleServiceTest(const NotificationScheduleServiceTest&) =
      delete;
  NotificationScheduleServiceTest& operator=(
      const NotificationScheduleServiceTest&) = delete;

  ~NotificationScheduleServiceTest() override {}

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    ASSERT_TRUE(tmp_dir_.Delete());
  }

  // Initializes |service_|. Injects database test data before this call.
  void Init() {
    auto* profile = browser()->profile();
    auto client = std::make_unique<TestClient>();
    clients_[SchedulerClientType::kTest1] = client.get();
    auto client_registrar =
        std::make_unique<NotificationSchedulerClientRegistrar>();
    client_registrar->RegisterClient(SchedulerClientType::kTest1,
                                     std::move(client));

    auto display_agent = notifications::DisplayAgent::Create();
    auto background_task_scheduler =
        std::make_unique<TestBackgroundTaskScheduler>();
    task_scheduler_ = background_task_scheduler.get();
    auto* db_provider =
        profile->GetDefaultStoragePartition()->GetProtoDatabaseProvider();
    service_ = CreateNotificationScheduleService(
        std::move(client_registrar), std::move(background_task_scheduler),
        std::move(display_agent), db_provider,
        tmp_dir_.GetPath().Append(kTestDir), profile->IsOffTheRecord());
  }

  // Helper function to schedule a notification immediately to show.
  void ScheduleNotification() {
    ScheduleParams schedule_params;
    schedule_params.deliver_time_start = base::Time::Now();
    schedule_params.deliver_time_end = base::Time::Now() + base::Minutes(5);
    NotificationData data;
    data.title = u"title";
    data.message = u"message";
    auto params = std::make_unique<notifications::NotificationParams>(
        notifications::SchedulerClientType::kTest1, std::move(data),
        std::move(schedule_params));
    schedule_service()->Schedule(std::move(params));
  }

  void RunBackgroundTask() {
    base::RunLoop loop;
    using TaskFinishedCallback =
        NotificationBackgroundTaskScheduler::Handler::TaskFinishedCallback;
    TaskFinishedCallback task_finish_callback =
        base::BindOnce([](base::RepeatingClosure quit_closure,
                          bool needs_reschedule) { quit_closure.Run(); },
                       loop.QuitClosure());
    schedule_service()->GetBackgroundTaskSchedulerHandler()->OnStartTask(
        std::move(task_finish_callback));
    loop.Run();
  }

  NotificationScheduleService* schedule_service() {
    return static_cast<NotificationScheduleService*>(service_.get());
  }

  TestBackgroundTaskScheduler* task_scheduler() {
    DCHECK(task_scheduler_);
    return task_scheduler_;
  }

  TestClient* client(SchedulerClientType type) {
    auto it = clients_.find(type);
    return it == clients_.end() ? nullptr : clients_[type];
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir tmp_dir_;
  std::unique_ptr<KeyedService> service_;
  raw_ptr<TestBackgroundTaskScheduler> task_scheduler_;
  std::map<SchedulerClientType, raw_ptr<TestClient, CtnExperimental>> clients_;
};

// Test to schedule a notification.
IN_PROC_BROWSER_TEST_F(NotificationScheduleServiceTest, ScheduleNotification) {
  Init();
  EXPECT_CALL(*task_scheduler()->mock_background_task(), Schedule(_, _))
      .Times(1);
  ScheduleNotification();
  task_scheduler()->WaitForTaskUpdated();
}

// Test to run a background task to show a notification.
IN_PROC_BROWSER_TEST_F(NotificationScheduleServiceTest, ShowNotification) {
  Init();
  EXPECT_CALL(*task_scheduler()->mock_background_task(), Schedule(_, _))
      .Times(1)
      .RetiresOnSaturation();

  // Schedule one notification.
  ScheduleNotification();
  task_scheduler()->WaitForTaskUpdated();

  // Trigger a background task. Expected to show one notification, and no
  // background task will be scheduled since there is no notification entry in
  // the database.
  EXPECT_CALL(*task_scheduler()->mock_background_task(), Cancel())
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*task_scheduler()->mock_background_task(), Schedule(_, _))
      .Times(0);
  RunBackgroundTask();
  EXPECT_EQ(
      1u,
      client(SchedulerClientType::kTest1)->shown_notification_data().size());
}

}  // namespace
}  // namespace notifications

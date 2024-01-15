// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/notification_scheduler.h"

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/notifications/scheduler/internal/background_task_coordinator.h"
#include "chrome/browser/notifications/scheduler/internal/display_decider.h"
#include "chrome/browser/notifications/scheduler/internal/impression_history_tracker.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/internal/notification_scheduler_context.h"
#include "chrome/browser/notifications/scheduler/internal/scheduled_notification_manager.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_utils.h"
#include "chrome/browser/notifications/scheduler/internal/stats.h"
#include "chrome/browser/notifications/scheduler/public/display_agent.h"
#include "chrome/browser/notifications/scheduler/public/notification_background_task_scheduler.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client_registrar.h"
#include "chrome/browser/notifications/scheduler/public/user_action_handler.h"

namespace notifications {
namespace {

class NotificationSchedulerImpl;

// Helper class to do async initialization in parallel for multiple subsystem
// instances.
class InitHelper {
 public:
  using InitCallback = base::OnceCallback<void(bool)>;
  InitHelper() : context_(nullptr) {}
  InitHelper(const InitHelper&) = delete;
  InitHelper& operator=(const InitHelper&) = delete;
  ~InitHelper() = default;

  // Initializes subsystems in notification scheduler, |callback| will be
  // invoked if all initializations finished or anyone of them failed. The
  // object should be destroyed along with the |callback|.
  void Init(NotificationSchedulerContext* context,
            ImpressionHistoryTracker::Delegate* delegate,
            InitCallback callback) {
    // TODO(xingliu): Initialize the databases in parallel, we currently
    // initialize one by one to work around a shared db issue. See
    // https://crbug.com/978680.
    context_ = context;
    callback_ = std::move(callback);

    context_->impression_tracker()->Init(
        delegate, base::BindOnce(&InitHelper::OnImpressionTrackerInitialized,
                                 weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void OnImpressionTrackerInitialized(bool success) {
    if (!success) {
      std::move(callback_).Run(false /*success*/);
      return;
    }

    context_->notification_manager()->Init(
        base::BindOnce(&InitHelper::OnNotificationManagerInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnNotificationManagerInitialized(bool success) {
    std::move(callback_).Run(success);
  }

  raw_ptr<NotificationSchedulerContext> context_;
  InitCallback callback_;

  base::WeakPtrFactory<InitHelper> weak_ptr_factory_{this};
};

// Helper class to display multiple notifications, and invoke a callback when
// finished.
class DisplayHelper {
 public:
  // Invoked with the total number of notification shown when all the display
  // flows are done.
  using FinishCallback = base::OnceCallback<void(int)>;
  DisplayHelper(const std::set<std::string>& guids,
                NotificationSchedulerContext* context,
                FinishCallback finish_callback)
      : guids_(guids),
        context_(context),
        finish_callback_(std::move(finish_callback)),
        shown_count_(0) {
    if (guids_.empty()) {
      std::move(finish_callback_).Run(0);
      return;
    }

    for (const auto& guid : guids) {
      context_->notification_manager()->DisplayNotification(
          guid, base::BindOnce(&DisplayHelper::BeforeDisplay,
                               weak_ptr_factory_.GetWeakPtr(), guid));
    }
  }

  DisplayHelper(const DisplayHelper&) = delete;
  DisplayHelper& operator=(const DisplayHelper&) = delete;
  ~DisplayHelper() = default;

 private:
  void BeforeDisplay(const std::string& guid,
                     std::unique_ptr<NotificationEntry> entry) {
    if (!entry) {
      DLOG(ERROR) << "Notification entry is null";
      MaybeFinish(guid, false /*shown*/);
      return;
    }

    // Inform the client to update notification data.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&DisplayHelper::NotifyClientBeforeDisplay,
                       weak_ptr_factory_.GetWeakPtr(), std::move(entry)));
  }

  void NotifyClientBeforeDisplay(std::unique_ptr<NotificationEntry> entry) {
    auto* client = context_->client_registrar()->GetClient(entry->type);
    if (!client) {
      MaybeFinish(entry->guid, false /*shown*/);
      return;
    }

    // Detach the notification data for client to rewrite.
    auto notification_data =
        std::make_unique<NotificationData>(std::move(entry->notification_data));
    client->BeforeShowNotification(
        std::move(notification_data),
        base::BindOnce(&DisplayHelper::AfterClientUpdateData,
                       weak_ptr_factory_.GetWeakPtr(), std::move(entry)));
  }

  void AfterClientUpdateData(
      std::unique_ptr<NotificationEntry> entry,
      std::unique_ptr<NotificationData> updated_notification_data) {
    if (!updated_notification_data) {
      stats::LogNotificationLifeCycleEvent(
          stats::NotificationLifeCycleEvent::kClientCancel, entry->type);
      MaybeFinish(entry->guid, false /*shown*/);
      return;
    }

    // Tracks user impression on the notification to be shown.
    context_->impression_tracker()->AddImpression(
        entry->type, entry->guid, entry->schedule_params.impression_mapping,
        updated_notification_data->custom_data,
        entry->schedule_params.ignore_timeout_duration);

    stats::LogNotificationShow(*updated_notification_data, entry->type);

    // Show the notification in UI.
    auto system_data = std::make_unique<DisplayAgent::SystemData>();
    system_data->type = entry->type;
    system_data->guid = entry->guid;
    context_->display_agent()->ShowNotification(
        std::move(updated_notification_data), std::move(system_data));

    MaybeFinish(entry->guid, true /*shown*/);
  }

  // Called when notification display flow is finished. Invokes
  // |finish_callback_| when all display flows are done.
  void MaybeFinish(const std::string& guid, bool shown) {
    if (base::Contains(guids_, guid) && shown) {
      shown_count_++;
    }
    guids_.erase(guid);
    if (guids_.empty() && finish_callback_) {
      std::move(finish_callback_).Run(shown_count_);
    }
  }

  std::set<std::string> guids_;
  raw_ptr<NotificationSchedulerContext> context_;
  FinishCallback finish_callback_;
  int shown_count_;
  base::WeakPtrFactory<DisplayHelper> weak_ptr_factory_{this};
};

// Implementation of NotificationScheduler.
class NotificationSchedulerImpl : public NotificationScheduler,
                                  public ImpressionHistoryTracker::Delegate {
 public:
  explicit NotificationSchedulerImpl(
      std::unique_ptr<NotificationSchedulerContext> context)
      : context_(std::move(context)) {}

  NotificationSchedulerImpl(const NotificationSchedulerImpl&) = delete;
  NotificationSchedulerImpl& operator=(const NotificationSchedulerImpl&) =
      delete;
  ~NotificationSchedulerImpl() override = default;

 private:
  // NotificationScheduler implementation.
  void Init(InitCallback init_callback) override {
    init_helper_ = std::make_unique<InitHelper>();
    init_helper_->Init(context_.get(), this,
                       base::BindOnce(&NotificationSchedulerImpl::OnInitialized,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      std::move(init_callback)));
  }

  void Schedule(
      std::unique_ptr<NotificationParams> notification_params) override {
    context_->notification_manager()->ScheduleNotification(
        std::move(notification_params),
        base::BindOnce(&NotificationSchedulerImpl::OnNotificationScheduled,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnNotificationScheduled(bool success) {
    if (success) {
      ScheduleBackgroundTask();
    }
  }

  void DeleteAllNotifications(SchedulerClientType type) override {
    context_->notification_manager()->DeleteNotifications(type);
  }

  void GetClientOverview(
      SchedulerClientType type,
      ClientOverview::ClientOverviewCallback callback) override {
    context_->impression_tracker()->GetImpressionDetail(
        type, base::BindOnce(
                  &NotificationSchedulerImpl::OnImpressionDetailQueryCompleted,
                  weak_ptr_factory_.GetWeakPtr(), type, std::move(callback)));
  }

  void OnImpressionDetailQueryCompleted(
      SchedulerClientType type,
      ClientOverview::ClientOverviewCallback callback,
      ImpressionDetail impression_detail) {
    std::vector<const NotificationEntry*> notifications;
    context_->notification_manager()->GetNotifications(type, &notifications);
    ClientOverview result(std::move(impression_detail), notifications.size());
    std::move(callback).Run(std::move(result));
  }

  void OnInitialized(InitCallback init_callback, bool success) {
    // TODO(xingliu): Tear down internal components if initialization failed.
    init_helper_.reset();
    std::move(init_callback).Run(success);
    NotifyClientsAfterInit(success);
  }

  void NotifyClientsAfterInit(bool success) {
    std::vector<SchedulerClientType> clients;
    context_->client_registrar()->GetRegisteredClients(&clients);
    for (auto type : clients) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&NotificationSchedulerImpl::NotifyClientAfterInit,
                         weak_ptr_factory_.GetWeakPtr(), type, success));
    }
  }

  void NotifyClientAfterInit(SchedulerClientType type, bool success) {
    std::vector<const NotificationEntry*> notifications;
    context_->notification_manager()->GetNotifications(type, &notifications);
    std::set<std::string> guids;
    for (const auto* notification : notifications) {
      DCHECK(notification);
      guids.emplace(notification->guid);
    }

    auto* client = context_->client_registrar()->GetClient(type);
    DCHECK(client);
    client->OnSchedulerInitialized(success, std::move(guids));
  }

  // NotificationBackgroundTaskScheduler::Handler implementation.
  void OnStartTask(TaskFinishedCallback callback) override {
    // Updates the impression data to compute daily notification shown budget.
    context_->impression_tracker()->AnalyzeImpressionHistory();

    // Show notifications.
    FindNotificationToShow(std::move(callback));
  }

  void OnStopTask() override { ScheduleBackgroundTask(); }

  void FindNotificationToShow(TaskFinishedCallback task_finish_callback) {
    DisplayDecider::Results results;
    ScheduledNotificationManager::Notifications notifications;
    context_->notification_manager()->GetAllNotifications(&notifications);

    DisplayDecider::ClientStates client_states;
    context_->impression_tracker()->GetClientStates(&client_states);

    std::vector<SchedulerClientType> clients;
    context_->client_registrar()->GetRegisteredClients(&clients);

    context_->display_decider()->FindNotificationsToShow(
        std::move(notifications), std::move(client_states), &results);

    display_helper_ = std::make_unique<DisplayHelper>(
        results, context_.get(),
        base::BindOnce(&NotificationSchedulerImpl::AfterNotificationsShown,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(task_finish_callback)));
  }

  void AfterNotificationsShown(TaskFinishedCallback task_finish_callback,
                               int shown_count) {
    stats::LogBackgroundTaskNotificationShown(shown_count);

    // Schedule the next background task based on scheduled notifications.
    ScheduleBackgroundTask();

    std::move(task_finish_callback).Run(false /*need_reschedule*/);
  }

  void ScheduleBackgroundTask() {
    BackgroundTaskCoordinator::Notifications notifications;
    context_->notification_manager()->GetAllNotifications(&notifications);
    BackgroundTaskCoordinator::ClientStates client_states;
    context_->impression_tracker()->GetClientStates(&client_states);

    context_->background_task_coordinator()->ScheduleBackgroundTask(
        std::move(notifications), std::move(client_states));
  }

  void OnUserAction(const UserActionData& action_data) override {
    context_->impression_tracker()->OnUserAction(action_data);
    ScheduleBackgroundTask();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&NotificationSchedulerImpl::NotifyClientAfterUserAction,
                       weak_ptr_factory_.GetWeakPtr(), action_data));
  }

  void NotifyClientAfterUserAction(const UserActionData& action_data) {
    auto* client =
        context_->client_registrar()->GetClient(action_data.client_type);
    if (!client)
      return;

    auto client_action_data = action_data;

    // Attach custom data if the impression is not expired.
    const auto* impression = context_->impression_tracker()->GetImpression(
        action_data.client_type, action_data.guid);
    if (impression) {
      client_action_data.custom_data = impression->custom_data;
    }

    client->OnUserAction(client_action_data);
  }

  void GetThrottleConfig(SchedulerClientType type,
                         ThrottleConfigCallback callback) override {
    auto* client = context_->client_registrar()->GetClient(type);
    if (client) {
      client->GetThrottleConfig(std::move(callback));
    } else {
      std::move(callback).Run(nullptr);
    }
  }

  std::unique_ptr<NotificationSchedulerContext> context_;
  std::unique_ptr<InitHelper> init_helper_;
  std::unique_ptr<DisplayHelper> display_helper_;

  base::WeakPtrFactory<NotificationSchedulerImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<NotificationScheduler> NotificationScheduler::Create(
    std::unique_ptr<NotificationSchedulerContext> context) {
  return std::make_unique<NotificationSchedulerImpl>(std::move(context));
}

NotificationScheduler::NotificationScheduler() = default;

NotificationScheduler::~NotificationScheduler() = default;

}  // namespace notifications

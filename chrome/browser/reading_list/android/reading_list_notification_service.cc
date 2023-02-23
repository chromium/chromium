// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reading_list/android/reading_list_notification_service.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/public/client_overview.h"
#include "chrome/browser/notifications/scheduler/public/notification_data.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/browser/notifications/scheduler/public/schedule_params.h"
#include "chrome/browser/reading_list/android/reading_list_notification_delegate.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/features/reading_list_switches.h"

constexpr notifications::SchedulerClientType kNotificationType =
    notifications::SchedulerClientType::kReadingList;
const base::TimeDelta kDeliverStartTimeDelta = base::Days(7);
const base::TimeDelta kDeliverEndTimeDelta = base::Minutes(5);

ReadingListNotificationService::Config::Config() = default;
ReadingListNotificationService::Config::~Config() = default;

// static
bool ReadingListNotificationService::IsEnabled() {
  return base::FeatureList::IsEnabled(
      reading_list::switches::kReadLaterReminderNotification);
}

ReadingListNotificationServiceImpl::ReadingListNotificationServiceImpl(
    ReadingListModel* reading_list_model,
    notifications::NotificationScheduleService* notification_scheduler,
    std::unique_ptr<ReadingListNotificationDelegate> delegate,
    std::unique_ptr<Config> config,
    base::Clock* clock)
    : reading_list_model_(reading_list_model),
      notification_scheduler_(notification_scheduler),
      delegate_(std::move(delegate)),
      config_(std::move(config)),
      clock_(clock) {
  DCHECK(notification_scheduler_);
  DCHECK(reading_list_model_);
  reading_list_model_->AddObserver(this);
}

ReadingListNotificationServiceImpl::~ReadingListNotificationServiceImpl() {
  reading_list_model_->RemoveObserver(this);
}

void ReadingListNotificationServiceImpl::OnStart() {
  if (!reading_list_model_->loaded()) {
    CallWhenModelLoaded(
        base::BindOnce(&ReadingListNotificationServiceImpl::OnStart,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Delete notification if there is no unread pages.
  size_t unread_size = ReadingListUnreadSize();
  if (unread_size == 0u) {
    notification_scheduler_->DeleteNotifications(kNotificationType);
    return;
  }

  MaybeScheduleNotification();
}

void ReadingListNotificationServiceImpl::BeforeShowNotification(
    std::unique_ptr<notifications::NotificationData> notification_data,
    NotificationDataCallback callback) {
  if (!reading_list_model_->loaded()) {
    CallWhenModelLoaded(base::BindOnce(
        &ReadingListNotificationServiceImpl::BeforeShowNotification,
        weak_ptr_factory_.GetWeakPtr(), std::move(notification_data),
        std::move(callback)));
    return;
  }

  // Cancel the notification before rendering in tray if no reading list
  // articles.
  size_t unread_size = ReadingListUnreadSize();
  if (unread_size == 0u) {
    std::move(callback).Run(nullptr);
    notification_scheduler_->DeleteNotifications(kNotificationType);
    return;
  }

  // Update the unread page count text.
  notification_data->message = delegate_->getNotificationSubTitle(unread_size);
  std::move(callback).Run(std::move(notification_data));

  // Schedule another one.
  MaybeScheduleNotification();
}

void ReadingListNotificationServiceImpl::OnClick() {
  delegate_->OpenReadingListPage();
}

void ReadingListNotificationServiceImpl::ReadingListModelLoaded(
    const ReadingListModel* model) {
  // Flush cached closures.
  while (!cached_closures_.empty()) {
    auto closure = std::move(cached_closures_.front());
    cached_closures_.pop();
    std::move(closure).Run();
  }
}

std::queue<base::OnceClosure>*
ReadingListNotificationServiceImpl::GetCachedClosureForTesting() {
  return &cached_closures_;
}

void ReadingListNotificationServiceImpl::CallWhenModelLoaded(
    base::OnceClosure closure) {
  if (reading_list_model_->loaded()) {
    std::move(closure).Run();
    return;
  }

  cached_closures_.emplace(std::move(closure));
}

void ReadingListNotificationServiceImpl::MaybeScheduleNotification() {
  notification_scheduler_->GetClientOverview(
      kNotificationType,
      base::BindOnce(&ReadingListNotificationServiceImpl::OnClientOverview,
                     weak_ptr_factory_.GetWeakPtr(), ReadingListUnreadSize()));
}

void ReadingListNotificationServiceImpl::OnClientOverview(
    size_t unread_size,
    notifications::ClientOverview overview) {
  // Already scheduled a weekly notification, do nothing.
  if (overview.num_scheduled_notifications > 0u)
    return;

  ScheduleNotification(unread_size);
}

void ReadingListNotificationServiceImpl::ScheduleNotification(int unread_size) {
  DCHECK_GT(unread_size, 0);
  notifications::NotificationData data;
  data.title = delegate_->getNotificationTitle();
  data.message = delegate_->getNotificationSubTitle(unread_size);
  notifications::ScheduleParams schedule_params;
  schedule_params.priority =
      notifications::ScheduleParams::Priority::kNoThrottle;

  // Show after a week.
  schedule_params.deliver_time_start = GetShowTime();
  schedule_params.deliver_time_end =
      schedule_params.deliver_time_start.value() + kDeliverEndTimeDelta;

  auto params = std::make_unique<notifications::NotificationParams>(
      kNotificationType, std::move(data), std::move(schedule_params));
  notification_scheduler_->Schedule(std::move(params));
}

size_t ReadingListNotificationServiceImpl::ReadingListUnreadSize() {
  DCHECK(reading_list_model_->loaded());
  return reading_list_model_->unread_size();
}

base::Time ReadingListNotificationServiceImpl::GetShowTime() const {
  base::Time time = clock_->Now() + kDeliverStartTimeDelta;
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);

  exploded.hour = config_->notification_show_time;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  bool success = base::Time::FromLocalExploded(exploded, &time);
  DCHECK(success);
  return time;
}

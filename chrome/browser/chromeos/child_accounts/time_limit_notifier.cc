// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limit_notifier.h"

#include <memory>
#include <string>

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

constexpr base::TimeDelta kWarningNotificationTimeout =
    base::TimeDelta::FromMinutes(5);
constexpr base::TimeDelta kExitNotificationTimeout =
    base::TimeDelta::FromMinutes(1);

// Lock notification id. All the time limit lock notifications share the same id
// so that a subsequent notification can replace the previous one.
constexpr char kTimeLimitLockNotificationId[] = "time-limit-lock-notification";

// Policy update notification id. Each limit has its own id, because we want to
// display all updates, which may happen simultaneously.
constexpr char kTimeLimitBedtimeUpdatedId[] = "time-limit-bedtime-updated";
constexpr char kTimeLimitScreenTimeUpdatedId[] =
    "time-limit-screen-time-updated";
constexpr char kTimeLimitOverrideUpdatedId[] = "time-limit-override-updated";

// The notifier id representing the app.
constexpr char kTimeLimitNotifierId[] = "family-link";

void ShowNotification(base::string16 title,
                      base::string16 message,
                      const std::string& notification_id,
                      content::BrowserContext* context) {
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title,
          message,
          l10n_util::GetStringUTF16(IDS_TIME_LIMIT_NOTIFICATION_DISPLAY_SOURCE),
          GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kTimeLimitNotifierId),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::NotificationDelegate>(),
          ash::kNotificationSupervisedUserIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  NotificationDisplayService::GetForProfile(
      Profile::FromBrowserContext(context))
      ->Display(NotificationHandler::Type::TRANSIENT, *notification,
                /*metadata=*/nullptr);
}

base::string16 RemainingTimeString(base::TimeDelta time_remaining) {
  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                ui::TimeFormat::LENGTH_LONG, time_remaining);
}

}  // namespace

TimeLimitNotifier::TimeLimitNotifier(content::BrowserContext* context)
    : TimeLimitNotifier(context, nullptr /* task_runner */) {}

TimeLimitNotifier::~TimeLimitNotifier() = default;

void TimeLimitNotifier::MaybeScheduleLockNotifications(
    LimitType limit_type,
    base::TimeDelta remaining_time) {
  // Stop any previously set timers.
  UnscheduleNotifications();

  int title_id;
  switch (limit_type) {
    case LimitType::kScreenTime:
      title_id = IDS_SCREEN_TIME_NOTIFICATION_TITLE;
      break;
    case LimitType::kBedTime:
    case LimitType::kOverride:
      title_id = IDS_BED_TIME_NOTIFICATION_TITLE;
      break;
  }

  const base::string16 title = l10n_util::GetStringUTF16(title_id);

  if (remaining_time >= kWarningNotificationTimeout) {
    warning_notification_timer_.Start(
        FROM_HERE, remaining_time - kWarningNotificationTimeout,
        base::BindOnce(&ShowNotification, title,
                       RemainingTimeString(kWarningNotificationTimeout),
                       kTimeLimitLockNotificationId, context_));
  }
  if (remaining_time >= kExitNotificationTimeout) {
    exit_notification_timer_.Start(
        FROM_HERE, remaining_time - kExitNotificationTimeout,
        base::BindOnce(&ShowNotification, title,
                       RemainingTimeString(kExitNotificationTimeout),
                       kTimeLimitLockNotificationId, context_));
  }
}

void TimeLimitNotifier::ShowPolicyUpdateNotification(
    LimitType limit_type,
    base::Optional<base::Time> lock_time) {
  int title_id;
  base::string16 message;
  std::string notification_id;
  switch (limit_type) {
    case LimitType::kScreenTime:
      title_id = IDS_TIME_LIMIT_UPDATED_NOTIFICATION_TITLE;
      message = l10n_util::GetStringUTF16(
          IDS_SCREEN_TIME_UPDATED_NOTIFICATION_MESSAGE);
      notification_id = kTimeLimitScreenTimeUpdatedId;
      break;
    case LimitType::kBedTime:
      title_id = IDS_TIME_LIMIT_UPDATED_NOTIFICATION_TITLE;
      message =
          l10n_util::GetStringUTF16(IDS_BEDTIME_UPDATED_NOTIFICATION_MESSAGE);
      notification_id = kTimeLimitBedtimeUpdatedId;
      break;
    case LimitType::kOverride:
      if (!lock_time)
        return;
      title_id = IDS_OVERRIDE_WITH_DURATION_UPDATED_NOTIFICATION_TITLE;
      message = l10n_util::GetStringFUTF16(
          IDS_OVERRIDE_WITH_DURATION_UPDATED_NOTIFICATION_MESSAGE,
          base::TimeFormatTimeOfDay(lock_time.value()));
      notification_id = kTimeLimitOverrideUpdatedId;
      break;
  }
  ShowNotification(l10n_util::GetStringUTF16(title_id), message,
                   notification_id, context_);
}

void TimeLimitNotifier::UnscheduleNotifications() {
  // TODO(crbug.com/897975): Stop() should be sufficient, but doesn't have the
  // expected effect in tests.
  warning_notification_timer_.AbandonAndStop();
  exit_notification_timer_.AbandonAndStop();
}

TimeLimitNotifier::TimeLimitNotifier(
    content::BrowserContext* context,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : context_(context) {
  if (task_runner.get()) {
    warning_notification_timer_.SetTaskRunner(task_runner);
    exit_notification_timer_.SetTaskRunner(task_runner);
  }
}

}  // namespace chromeos

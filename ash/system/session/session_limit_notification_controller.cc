// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/session/session_limit_notification_controller.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/session_length_limit_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

const char kNotifierSessionLengthTimeout[] = "ash.session-length-timeout";

// A notification is shown to the user only if the remaining session time falls
// under this threshold. e.g. If the user has several days left in their
// session, there is no use displaying a notification right now.
constexpr base::TimeDelta kNotificationThreshold = base::Minutes(60);

}  // namespace

// static
const char SessionLimitNotificationController::kNotificationId[] =
    "chrome://session/timeout";

SessionLimitNotificationController::SessionLimitNotificationController()
    : model_(Shell::Get()->system_tray_model()->session_length_limit()) {
  model_->AddObserver(this);
  OnSessionLengthLimitUpdated();
}

SessionLimitNotificationController::~SessionLimitNotificationController() {
  model_->RemoveObserver(this);
}

void SessionLimitNotificationController::OnSessionLengthLimitUpdated() {
  // Don't show notification until the user is logged in.
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    return;

  UpdateNotification();
  last_limit_state_ = model_->limit_state();
}

void SessionLimitNotificationController::UpdateNotification() {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  // If state hasn't changed and the notification has already been acknowledged,
  // we won't re-create it. We consider a notification to be acknowledged if it
  // was shown before, but is no longer visible.
  if (model_->limit_state() == last_limit_state_ &&
      has_notification_been_shown_ &&
      !message_center->FindVisibleNotificationById(kNotificationId)) {
    return;
  }

  // After state change, any possibly existing notification is removed to make
  // sure it is re-shown even if it had been acknowledged by the user before
  // (and in the rare case of state change towards LIMIT_NONE to make the
  // notification disappear).
  if (model_->limit_state() != last_limit_state_ &&
      message_center->FindVisibleNotificationById(kNotificationId)) {
    message_center::MessageCenter::Get()->RemoveNotification(
        kNotificationId, false /* by_user */);
  }

  // If the session is unlimited or if the remaining time is too far off into
  // the future, there is nothing more to do.
  if (model_->limit_state() == SessionLengthLimitModel::LIMIT_NONE ||
      model_->remaining_session_time() > kNotificationThreshold) {
    return;
  }

  message_center::RichNotificationData data;
  data.should_make_spoken_feedback_for_popup_updates =
      (model_->limit_state() != last_limit_state_);
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
          ComposeNotificationTitle(),
          l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_NOTIFICATION_SESSION_LENGTH_LIMIT_MESSAGE),
          std::u16string() /* display_source */, GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierSessionLengthTimeout,
              NotificationCatalogName::kSessionLengthTimeout),
          data, nullptr /* delegate */, kNotificationTimerIcon,
          message_center::SystemNotificationWarningLevel::WARNING);
  notification->set_pinned(true);
  if (message_center->FindVisibleNotificationById(kNotificationId)) {
    message_center->UpdateNotification(kNotificationId,
                                       std::move(notification));
  } else {
    message_center->AddNotification(std::move(notification));
  }
  has_notification_been_shown_ = true;
}

std::u16string SessionLimitNotificationController::ComposeNotificationTitle()
    const {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NOTIFICATION_SESSION_LENGTH_LIMIT_TITLE,
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_SHORT,
                             model_->remaining_session_time()));
}

}  // namespace ash

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tracing_notification_controller.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {

const char kNotifierId[] = "ash.tracing";

void HandleNotificationClick() {
  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_TRACING_DEFAULT_SELECTED);
  Shell::Get()->system_tray_model()->client()->ShowChromeSlow();
}

}  // namespace

// static
const char TracingNotificationController::kNotificationId[] = "chrome://slow";

TracingNotificationController::TracingNotificationController()
    : model_(Shell::Get()->system_tray_model()->tracing()) {
  model_->AddObserver(this);
  OnTracingModeChanged();
}

TracingNotificationController::~TracingNotificationController() {
  model_->RemoveObserver(this);
}

void TracingNotificationController::OnTracingModeChanged() {
  // Return if the state doesn't change.
  if (was_tracing_ == model_->is_tracing())
    return;

  if (model_->is_tracing())
    CreateNotification();
  else
    RemoveNotification();

  was_tracing_ = model_->is_tracing();
}

void TracingNotificationController::CreateNotification() {
  std::unique_ptr<Notification> notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_TRACING_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_TRACING_NOTIFICATION_MESSAGE),
      base::string16() /* display_source */, GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&HandleNotificationClick)),
      kSystemMenuTracingIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  notification->set_pinned(true);
  MessageCenter::Get()->AddNotification(std::move(notification));
}

void TracingNotificationController::RemoveNotification() {
  message_center::MessageCenter::Get()->RemoveNotification(kNotificationId,
                                                           false /* by_user */);
}

}  // namespace ash

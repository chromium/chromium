// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/gnubby_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/location.h"
#include "base/strings/string16.h"
#include "base/task/post_task.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace {
constexpr base::TimeDelta kNotificationTimeout =
    base::TimeDelta::FromSeconds(2);
}  // namespace

namespace chromeos {

GnubbyNotification::GnubbyNotification()
    : update_dismiss_notification_timer_(new base::OneShotTimer()),
      weak_ptr_factory_(this) {
  DCHECK(DBusThreadManager::Get()->GetGnubbyClient());
  DBusThreadManager::Get()->GetGnubbyClient()->AddObserver(this);
}
GnubbyNotification::~GnubbyNotification() {
  DCHECK(DBusThreadManager::Get()->GetGnubbyClient());
  DBusThreadManager::Get()->GetGnubbyClient()->RemoveObserver(this);
}

void GnubbyNotification::PromptUserAuth() {
  ShowNotification();
}

void GnubbyNotification::CreateNotification() {
  const base::string16 title =
      l10n_util::GetStringUTF16(IDS_GNUBBY_NOTIFICATION_TITLE);
  const base::string16 message =
      l10n_util::GetStringUTF16(IDS_GNUBBY_NOTIFICATION_MESSAGE);
  const message_center::SystemNotificationWarningLevel colorType =
      message_center::SystemNotificationWarningLevel::NORMAL;

  GnubbyNotification::notification_prompt_ = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      GnubbyNotification::kNotificationID, title, message, base::string16(),
      GURL(), message_center::NotifierId(),
      message_center::RichNotificationData(),
      new message_center::HandleNotificationClickDelegate(
          base::BindRepeating(&GnubbyNotification::DismissNotification,
                              weak_ptr_factory_.GetWeakPtr())),
      gfx::VectorIcon(), colorType);
}

void GnubbyNotification::ShowNotification() {
  GnubbyNotification::update_dismiss_notification_timer_->Stop();

  if (GnubbyNotification::notificationActive == false) {
    GnubbyNotification::notification_prompt_.reset();
    CreateNotification();
  }
  SystemNotificationHelper::GetInstance()->Display(
      *GnubbyNotification::notification_prompt_);
  GnubbyNotification::update_dismiss_notification_timer_->Start(
      FROM_HERE, kNotificationTimeout,
      base::BindRepeating(&GnubbyNotification::DismissNotification,
                          base::Unretained(this)));
  GnubbyNotification::notificationActive = true;
}

void GnubbyNotification::DismissNotification() {
  GnubbyNotification::notificationActive = false;
  SystemNotificationHelper::GetInstance()->Close(
      GnubbyNotification::kNotificationID);
}

}  // namespace chromeos

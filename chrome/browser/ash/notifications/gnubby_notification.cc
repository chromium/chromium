// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/gnubby_notification.h"

#include <string>

#include "ash/public/cpp/message_center/oobe_notification_constants.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/location.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/gnubby/gnubby_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace {
constexpr base::TimeDelta kNotificationTimeout = base::Seconds(2);
}  // namespace

namespace ash {

GnubbyNotification::GnubbyNotification()
    : update_dismiss_notification_timer_(new base::OneShotTimer()),
      weak_ptr_factory_(this) {
  DCHECK(GnubbyClient::Get());
  GnubbyClient::Get()->AddObserver(this);
}

GnubbyNotification::~GnubbyNotification() {
  DCHECK(GnubbyClient::Get());
  GnubbyClient::Get()->RemoveObserver(this);
}

void GnubbyNotification::PromptUserAuth() {
  ShowNotification();
}

void GnubbyNotification::CreateNotification() {
  const std::u16string title =
      l10n_util::GetStringUTF16(IDS_GNUBBY_NOTIFICATION_TITLE);
  const std::u16string message =
      l10n_util::GetStringUTF16(IDS_GNUBBY_NOTIFICATION_MESSAGE);
  const message_center::SystemNotificationWarningLevel colorType =
      message_center::SystemNotificationWarningLevel::NORMAL;

  GnubbyNotification::notification_prompt_ = ash::CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, kOOBEGnubbyNotificationId,
      title, message, std::u16string(), GURL(), message_center::NotifierId(),
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
      base::BindOnce(&GnubbyNotification::DismissNotification,
                     base::Unretained(this)));
  GnubbyNotification::notificationActive = true;
}

void GnubbyNotification::DismissNotification() {
  GnubbyNotification::notificationActive = false;
  SystemNotificationHelper::GetInstance()->Close(kOOBEGnubbyNotificationId);
}

}  // namespace ash

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/gnubby_notification.h"

#include <string>

#include "ash/public/cpp/message_center/oobe_notification_constants.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/check_is_test.h"
#include "base/location.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/gnubby/gnubby_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
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
  if (message_center::MessageCenter::Get()) {
    DismissNotification();
  } else {
    // TODO(crbug.com/454766826): Fix shutdown order so this isn't needed.
    CHECK_IS_TEST();
  }
  GnubbyClient::Get()->RemoveObserver(this);
}

void GnubbyNotification::PromptUserAuth() {
  ShowNotification();
}

void GnubbyNotification::ShowNotification() {
  update_dismiss_notification_timer_->Stop();

  if (!notification_active_) {
    const std::u16string title =
        l10n_util::GetStringUTF16(IDS_GNUBBY_NOTIFICATION_TITLE);
    const std::u16string message =
        l10n_util::GetStringUTF16(IDS_GNUBBY_NOTIFICATION_MESSAGE);
    const message_center::SystemNotificationWarningLevel colorType =
        message_center::SystemNotificationWarningLevel::NORMAL;

    auto notification = ash::CreateSystemNotificationPtr(
        message_center::NOTIFICATION_TYPE_SIMPLE, kOOBEGnubbyNotificationId,
        title, message, std::u16string(), GURL(), message_center::NotifierId(),
        message_center::RichNotificationData(),
        base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
            base::BindRepeating(&GnubbyNotification::DismissNotification,
                                weak_ptr_factory_.GetWeakPtr())),
        gfx::VectorIcon::EmptyIcon(), colorType);
    message_center::MessageCenter::Get()->AddNotification(
        std::move(notification));
    notification_active_ = true;
  }

  update_dismiss_notification_timer_->Start(
      FROM_HERE, kNotificationTimeout,
      base::BindOnce(&GnubbyNotification::DismissNotification,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GnubbyNotification::DismissNotification() {
  notification_active_ = false;
  message_center::MessageCenter::Get()->RemoveNotification(
      kOOBEGnubbyNotificationId, false /* by_user */);
}

}  // namespace ash

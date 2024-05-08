// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/message_center/arc_notification_delegate.h"

#include "ash/public/cpp/external_arc/message_center/arc_notification_content_view.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_item.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_view.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view.h"

namespace ash {

ArcNotificationDelegate::ArcNotificationDelegate(
    base::WeakPtr<ArcNotificationItem> item)
    : item_(item) {
  DCHECK(item_);
}

ArcNotificationDelegate::~ArcNotificationDelegate() = default;

std::unique_ptr<message_center::MessageView>
ArcNotificationDelegate::CreateCustomMessageView(
    const message_center::Notification& notification,
    bool shown_in_popup) {
  CHECK(item_);
  DCHECK_EQ(item_->GetNotificationId(), notification.id());
  return std::make_unique<ArcNotificationView>(item_.get(), notification,
                                               shown_in_popup);
}

void ArcNotificationDelegate::Close(bool by_user) {
  DCHECK(item_);
  item_->Close(by_user);
}

void ArcNotificationDelegate::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  DCHECK(item_);

  if (button_index) {
    item_->ClickButton(button_index.value(),
                       base::UTF16ToUTF8(reply.value_or(std::u16string())));
  } else {
    item_->Click();
  }
}

void ArcNotificationDelegate::SettingsClick() {
  DCHECK(item_);
  item_->OpenSettings();
}

void ArcNotificationDelegate::DisableNotification() {
  DCHECK(item_);
  item_->DisableNotification();
}

void ArcNotificationDelegate::ExpandStateChanged(bool expanded) {
  DCHECK(item_);
  item_->SetExpandState(expanded);
}

void ArcNotificationDelegate::SnoozeButtonClicked() {
  DCHECK(item_);
  item_->OpenSnooze();
}

message_center::NotificationDelegate*
ArcNotificationDelegate::GetDelegateForParentCopy() {
  return nullptr;
}

}  // namespace ash

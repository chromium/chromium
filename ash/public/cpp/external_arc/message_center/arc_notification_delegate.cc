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
    const absl::optional<int>& button_index,
    const absl::optional<std::u16string>& reply) {
  DCHECK(item_);
  item_->Click();
}

void ArcNotificationDelegate::SettingsClick() {
  DCHECK(item_);
  item_->OpenSettings();
}

}  // namespace ash

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/arc/arc_notification_delegate.h"

#include "ash/system/message_center/arc/arc_notification_content_view.h"
#include "ash/system/message_center/arc/arc_notification_item.h"
#include "ash/system/message_center/arc/arc_notification_view.h"
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
    const message_center::Notification& notification) {
  CHECK(item_);
  DCHECK_EQ(item_->GetNotificationId(), notification.id());
  return std::make_unique<ArcNotificationView>(item_.get(), notification);
}

void ArcNotificationDelegate::Close(bool by_user) {
  DCHECK(item_);
  item_->Close(by_user);
}

void ArcNotificationDelegate::Click(
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>& reply) {
  DCHECK(item_);
  item_->Click();
}

void ArcNotificationDelegate::SettingsClick() {
  DCHECK(item_);
  item_->OpenSettings();
}

}  // namespace ash

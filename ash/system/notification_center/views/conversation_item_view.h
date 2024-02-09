// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_CONVERSATION_ITEM_VIEW_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_CONVERSATION_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/layout/flex_layout_view.h"

namespace message_center {
class NotificationItem;
}  // namespace message_center

namespace ash {

// A view that displays an icon, title and message associated with a
// `notification_item`.
class ASH_EXPORT ConversationItemView : public views::FlexLayoutView {
  METADATA_HEADER(ConversationItemView, views::FlexLayoutView)

 public:
  explicit ConversationItemView(
      const message_center::NotificationItem& notification_item);
  ConversationItemView(const ConversationItemView&) = delete;
  ConversationItemView& operator=(const ConversationItemView&) = delete;
  ~ConversationItemView() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_CONVERSATION_ITEM_VIEW_H_

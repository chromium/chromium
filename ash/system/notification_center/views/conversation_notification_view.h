// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_CONVERSATION_NOTIFICATION_VIEW_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_CONVERSATION_NOTIFICATION_VIEW_H_

#include <ash/ash_export.h>

#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/metadata/view_factory_internal.h"

using MessageView = message_center::MessageView;
using Notification = message_center::Notification;

namespace message_center {
class NotificationControlButtonsView;
}

namespace views {
class FlexLayoutView;
class Label;
}

namespace ash {

class NotificationActionsView;
class AshNotificationExpandButton;
class TimestampView;

class ASH_EXPORT ConversationNotificationView
    : public message_center::MessageView {
  METADATA_HEADER(ConversationNotificationView, message_center::MessageView)

 public:
  enum ViewId {
    kControlButtonsView = 1,
    kCollapsedModeContainer,
    kCollapsedPreviewContainer,
    kCollapsedPreviewMessage,
    kCollapsedPreviewTitle,
    kConversationContainer,
    kExpandButton,
    kMainIcon,
    kTitleLabel,
    kAppNameLabel,
  };

  explicit ConversationNotificationView(const Notification& notification);
  ConversationNotificationView(const ConversationNotificationView&) = delete;
  ConversationNotificationView& operator=(const ConversationNotificationView&) =
      delete;
  ~ConversationNotificationView() override;

  // Toggles the expand state of the notification. This function should only be
  // used to handle user manually expand/collapse a notification.
  void ToggleExpand();

  // message_center::MessageView:
  bool IsExpanded() const override;
  void OnThemeChanged() override;
  void UpdateWithNotification(
      const message_center::Notification& notification) override;
  message_center::NotificationControlButtonsView* GetControlButtonsView()
      const override;
  void ToggleInlineSettings(const ui::Event& event) override;

 private:
  friend class ConversationNotificationViewTest;

  std::unique_ptr<views::FlexLayoutView> CreateMainContainer(
      const message_center::Notification& notification);
  std::unique_ptr<views::FlexLayoutView> CreateRightControlsContainer();
  std::unique_ptr<views::FlexLayoutView> CreateTextContainer(
      const message_center::Notification& notification);
  std::unique_ptr<views::FlexLayoutView> CreateTitleRow(
      const message_center::Notification& notification);

  // Whether this notification is expanded or not.
  bool expanded_ = true;

  raw_ptr<NotificationActionsView> actions_view_ = nullptr;
  raw_ptr<views::View> conversations_container_ = nullptr;
  raw_ptr<views::View> collapsed_preview_container_ = nullptr;
  raw_ptr<message_center::NotificationControlButtonsView>
      control_buttons_view_ = nullptr;
  raw_ptr<AshNotificationExpandButton> expand_button_ = nullptr;
  raw_ptr<views::View> inline_settings_view_ = nullptr;
  raw_ptr<views::View> right_controls_container_ = nullptr;
  raw_ptr<TimestampView> timestamp_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> app_name_view_ = nullptr;
  raw_ptr<views::Label> app_name_divider_ = nullptr;
};
}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_CONVERSATION_NOTIFICATION_VIEW_H_

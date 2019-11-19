// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_ARC_ARC_NOTIFICATION_VIEW_H_
#define ASH_SYSTEM_MESSAGE_CENTER_ARC_ARC_NOTIFICATION_VIEW_H_

#include "ash/system/message_center/arc/arc_notification_item.h"
#include "base/macros.h"
#include "ui/message_center/views/message_view.h"

namespace arc {
class ArcAccessibilityHelperBridgeTest;
}

namespace views {
class Painter;
}

namespace ash {

class ArcNotificationContentView;

// View for custom notification with NOTIFICATION_TYPE_CUSTOM which hosts the
// ArcNotificationContentView which shows content of the notification.
class ArcNotificationView : public message_center::MessageView,
                            public ArcNotificationItem::Observer {
 public:
  static ArcNotificationView* FromView(views::View* message_view);

  // |content_view| is a view to be hosted in this view.
  ArcNotificationView(ArcNotificationItem* item,
                      const message_center::Notification& notification);
  ~ArcNotificationView() override;

  // These method are called by the content view when focus handling is deferred
  // to the content.
  void OnContentFocused();
  void OnContentBlurred();

  // Overridden from MessageView:
  void UpdateWithNotification(
      const message_center::Notification& notification) override;
  void SetDrawBackgroundAsActive(bool active) override;
  void UpdateControlButtonsVisibility() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  message_center::NotificationControlButtonsView* GetControlButtonsView()
      const override;
  bool IsExpanded() const override;
  void SetExpanded(bool expanded) override;
  bool IsAutoExpandingAllowed() const override;
  bool IsManuallyExpandedOrCollapsed() const override;
  void OnContainerAnimationStarted() override;
  void OnContainerAnimationEnded() override;
  void OnSettingsButtonPressed(const ui::Event& event) override;
  void OnSnoozeButtonPressed(const ui::Event& event) override;
  void UpdateCornerRadius(int top_radius, int bottom_radius) override;

  // views::SlideOutControllerDelegate:
  void OnSlideChanged(bool in_progress) override;

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  bool HasFocus() const override;
  void RequestFocus() override;
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void ChildPreferredSizeChanged(View* child) override;
  bool HandleAccessibleAction(const ui::AXActionData& action) override;

  // ArcNotificationItem::Observer
  void OnItemDestroying() override;

 private:
  friend class ArcNotificationContentViewTest;
  friend class ArcNotificationViewTest;
  friend class arc::ArcAccessibilityHelperBridgeTest;

  // TODO(yoshiki): Mmove this to message_center::MessageView.
  void UpdateControlButtonsVisibilityWithNotification(
      const message_center::Notification& notification);

  ArcNotificationItem* item_;

  // The view for the custom content. Owned by view hierarchy.
  ArcNotificationContentView* const content_view_;

  std::unique_ptr<views::Painter> focus_painter_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_ARC_ARC_NOTIFICATION_VIEW_H_

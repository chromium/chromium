// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_VIEW_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_VIEW_H_

#include "ash/public/cpp/external_arc/message_center/arc_notification_item.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/message_center/views/message_view.h"

namespace aura {
class Window;
}

namespace arc {
class ArcAccessibilityHelperBridgeTest;
}

namespace ash {

class ArcNotificationContentView;

// View for custom notification with NOTIFICATION_TYPE_CUSTOM which hosts the
// ArcNotificationContentView which shows content of the notification.
class ArcNotificationView : public message_center::MessageView,
                            public ArcNotificationItem::Observer {
  METADATA_HEADER(ArcNotificationView, message_center::MessageView)

 public:
  static ArcNotificationView* FromView(views::View* message_view);
  // |content_view| is a view to be hosted in this view.
  ArcNotificationView(ArcNotificationItem* item,
                      const message_center::Notification& notification,
                      bool shown_in_popup);

  ArcNotificationView(const ArcNotificationView&) = delete;
  ArcNotificationView& operator=(const ArcNotificationView&) = delete;

  ~ArcNotificationView() override;

  // These method are called by the content view when focus handling is deferred
  // to the content.
  void OnContentFocused();
  void OnContentBlurred();

  // Overridden from MessageView:
  void UpdateWithNotification(
      const message_center::Notification& notification) override;
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
  void UpdateBackgroundPainter() override;
  base::TimeDelta GetBoundsAnimationDuration(
      const message_center::Notification&) const override;
  void AnimateGroupedChildExpandedCollapse(bool expanded) override;
  void AnimateSingleToGroup(const std::string& notification_id,
                            std::string parent_id) override;
  void SetGroupedChildExpanded(bool expanded) override;

  // views::SlideOutControllerDelegate:
  void OnSlideChanged(bool in_progress) override;

  // Overridden from views::View:
  void Layout(PassKey) override;
  bool HasFocus() const override;
  void RequestFocus() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void ChildPreferredSizeChanged(View* child) override;
  bool HandleAccessibleAction(const ui::AXActionData& action) override;
  void OnThemeChanged() override;

  // ArcNotificationItem::Observer
  void OnItemDestroying() override;

  // Returns the native container view for notification surface.
  aura::Window* GetNativeContainerWindowForTest() const;

 private:
  friend class ArcNotificationContentViewTest;
  friend class ArcNotificationViewTest;
  friend class arc::ArcAccessibilityHelperBridgeTest;

  // TODO(yoshiki): Mmove this to message_center::MessageView.
  void UpdateControlButtonsVisibilityWithNotification(
      const message_center::Notification& notification);

  raw_ptr<ArcNotificationItem> item_;

  // The view for the custom content. Owned by view hierarchy.
  const raw_ptr<ArcNotificationContentView> content_view_;

  const bool shown_in_popup_;

  bool is_group_child_;

  raw_ptr<views::View> collapsed_summary_view_ = nullptr;

  base::WeakPtrFactory<ArcNotificationView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_VIEW_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_VIEW_H_
#define ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_VIEW_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/views/notification_input_container.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/message_center/views/notification_view_base.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace message_center {
class Notification;
}  // namespace message_center

namespace views {
class BoxLayout;
class LabelButton;
class View;
}  // namespace views

namespace ash {

class RoundedImageView;
class AshNotificationExpandButton;
class IconButton;
class NotificationGroupingController;

// Customized NotificationView for notification on ChromeOS. This view is used
// to displays all current types of notification on ChromeOS (web, basic, image,
// and list) except custom notification.
class ASH_EXPORT AshNotificationView
    : public message_center::NotificationViewBase,
      public message_center::MessageCenterObserver,
      public views::WidgetObserver {
 public:
  static const char kViewClassName[];

  // TODO(crbug/1241983): Add metadata and builder support to this view.
  explicit AshNotificationView(const message_center::Notification& notification,
                               bool shown_in_popup);
  AshNotificationView(const AshNotificationView&) = delete;
  AshNotificationView& operator=(const AshNotificationView&) = delete;
  ~AshNotificationView() override;

  // Update the expanded state for grouped child notification.
  void SetGroupedChildExpanded(bool expanded);

  // Animate the grouped child notification when switching between expand and
  // collapse state.
  void AnimateGroupedChildExpandedCollapse(bool expanded);

  // Animations when converting from single to group notification.
  void AnimateSingleToGroup(NotificationGroupingController* grouping_controller,
                            const std::string& notification_id,
                            std::string parent_id);

  // Toggle the expand state of the notification. This function should only be
  // used to handle user manually expand/collapse a notification.
  void ToggleExpand();

  // Called when a child notificaiton's preferred size changes.
  void GroupedNotificationsPreferredSizeChanged();

  // Drag related functions ----------------------------------------------------

  // Returns the bounds of the area where the drag can be initiated. The
  // returned bounds are in `AshNotificationView` local coordinates. Returns
  // `absl::nullopt` if the notification view is not draggable.
  absl::optional<gfx::Rect> GetDragAreaBounds() const;

  // Returns the drag image shown when the ash notification is under drag.
  // Returns `absl::nullopt` if the notification view is not draggable.
  absl::optional<gfx::ImageSkia> GetDragImage();

  // Attaches the drop data. This method should be called only if this
  // notification view is draggable.
  void AttachDropData(ui::OSExchangeData* data);

  // Returns true if this notification view is draggable.
  bool IsDraggable() const;

  // message_center::MessageView:
  void AddGroupNotification(
      const message_center::Notification& notification) override;
  void PopulateGroupNotifications(
      const std::vector<const message_center::Notification*>& notifications)
      override;
  void RemoveGroupNotification(const std::string& notification_id) override;
  const char* GetClassName() const override;
  // Called after `PreferredSizeChanged()`, so the current state is the target
  // state.
  base::TimeDelta GetBoundsAnimationDuration(
      const message_center::Notification& notification) const override;

  // message_center::NotificationViewBase:
  void AddedToWidget() override;
  void Layout() override;
  void UpdateViewForExpandedState(bool expanded) override;
  void UpdateWithNotification(
      const message_center::Notification& notification) override;
  void CreateOrUpdateHeaderView(
      const message_center::Notification& notification) override;
  void CreateOrUpdateTitleView(
      const message_center::Notification& notification) override;
  void CreateOrUpdateSmallIconView(
      const message_center::Notification& notification) override;
  void CreateOrUpdateInlineSettingsViews(
      const message_center::Notification& notification) override;
  void CreateOrUpdateCompactTitleMessageView(
      const message_center::Notification& notification) override;
  void CreateOrUpdateProgressViews(
      const message_center::Notification& notification) override;
  void UpdateControlButtonsVisibility() override;
  bool IsIconViewShown() const override;
  void SetExpandButtonVisibility(bool visible) override;
  bool IsExpandable() const override;
  void UpdateCornerRadius(int top_radius, int bottom_radius) override;
  void SetDrawBackgroundAsActive(bool active) override;
  void OnThemeChanged() override;
  std::unique_ptr<message_center::NotificationInputContainer>
  GenerateNotificationInputContainer() override;
  std::unique_ptr<views::LabelButton> GenerateNotificationLabelButton(
      views::Button::PressedCallback callback,
      const std::u16string& label) override;
  gfx::Size GetIconViewSize() const override;
  int GetLargeImageViewMaxWidth() const override;
  void ToggleInlineSettings(const ui::Event& event) override;
  void OnInlineReplyUpdated() override;

  void set_is_animating(bool is_animating) { is_animating_ = is_animating; }
  bool is_animating() { return is_animating_; }

  AshNotificationExpandButton* expand_button_for_test() {
    return expand_button_;
  }

  message_center::NotificationControlButtonsView*
  control_buttons_view_for_test() {
    return control_buttons_view_;
  }

  // View containing all grouped notifications, propagates size changes
  // to the parent notification view.
  class GroupedNotificationsContainer : public views::BoxLayoutView {
   public:
    GroupedNotificationsContainer() = default;
    GroupedNotificationsContainer(const GroupedNotificationsContainer&) =
        delete;
    GroupedNotificationsContainer& operator=(
        const GroupedNotificationsContainer&) = delete;
    void ChildPreferredSizeChanged(views::View* view) override;
    void SetParentNotificationView(
        AshNotificationView* parent_notification_view);

   private:
    raw_ptr<AshNotificationView, ExperimentalAsh> parent_notification_view_ =
        nullptr;
  };
  BEGIN_VIEW_BUILDER(/*no export*/,
                     GroupedNotificationsContainer,
                     views::BoxLayoutView)
  VIEW_BUILDER_PROPERTY(AshNotificationView*, ParentNotificationView)
  END_VIEW_BUILDER

 private:
  friend class AshNotificationViewTestBase;
  friend class NotificationGroupingControllerTest;

  // Customized title row for this notification view with added timestamp in
  // collapse mode.
  class NotificationTitleRow : public views::View {
   public:
    METADATA_HEADER(NotificationTitleRow);
    explicit NotificationTitleRow(const std::u16string& title);
    NotificationTitleRow(const NotificationTitleRow&) = delete;
    NotificationTitleRow& operator=(const NotificationTitleRow&) = delete;
    ~NotificationTitleRow() override;

    // Update title view's text.
    void UpdateTitle(const std::u16string& title);

    // Update the text for `timestamp_in_collapsed_view_`. Also used the timer
    // to re-update this timestamp view when the next update is needed.
    void UpdateTimestamp(base::Time timestamp);

    // Update children's visibility based on the state of expand/collapse.
    void UpdateVisibility(bool in_collapsed_mode);

    // Perform expand/collapse animation in children views.
    void PerformExpandCollapseAnimation();

    // Set the maximum available width for this view.
    void SetMaxAvailableWidth(int max_available_width);

    // views::View:
    gfx::Size CalculatePreferredSize() const override;
    void OnThemeChanged() override;

    views::Label* title_view() { return title_view_; }

   private:
    friend class AshNotificationViewTestBase;
    // Showing notification title.
    const raw_ptr<views::Label, ExperimentalAsh> title_view_;

    // Timestamp view shown alongside the title in collapsed state.
    const raw_ptr<views::Label, ExperimentalAsh> title_row_divider_;
    const raw_ptr<views::Label, ExperimentalAsh> timestamp_in_collapsed_view_;

    // The maximum width available to the title row.
    int max_available_width_ = 0;

    // Timer that updates the timestamp over time.
    base::OneShotTimer timestamp_update_timer_;
    absl::optional<base::Time> timestamp_;
  };

  // message_center::MessageView:
  views::View* FindGroupNotificationView(
      const std::string& notification_id) override;

  // message_center::MessageCenterObserver:
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // Abort all currently running layer animations. This includes any animatios
  // on child notifications for parent notification views.
  void AbortAllAnimations();

  // Create or update the customized snooze button in action buttons row
  // according to the given notification.
  void CreateOrUpdateSnoozeButton(
      const message_center::Notification& notification);

  // Update visibility for grouped notifications to ensure only
  // `kMaxGroupedNotificationsInCollapsedState` are visible in the collapsed
  // state.
  void UpdateGroupedNotificationsVisibility();

  // Update `message_in_expanded_view_` according to the given notification.
  void UpdateMessageLabelInExpandedState(
      const message_center::Notification& notification);

  // Update the background color with rounded corner.
  void UpdateBackground(int top_radius, int bottom_radius);

  // Get the available space for `message_label_in_expanded_state_` width.
  int GetExpandedMessageLabelWidth();

  // Disable the notification of this view. Called after the turn of
  // notifications button is clicked.
  void DisableNotification();

  // Update the color and icon for `app_icon_view_`.
  void UpdateAppIconView(const message_center::Notification* notification);

  // Calculate the color used for the app icon and action buttons.
  SkColor CalculateIconAndButtonsColor(
      const message_center::Notification* notification);

  // Update the color of icon and buttons.
  void UpdateIconAndButtonsColor(
      const message_center::Notification* notification);

  // Animate resizing a parent notification view after a child notification view
  // has been removed from itself.
  void AnimateResizeAfterRemoval(views::View* to_be_removed);

  // AshNotificationView will animate its expand/collapse in the parent's
  // ChildPreferredSizeChange(). Child views are animated here.
  void PerformExpandCollapseAnimation();

  // Expand/collapse animation for large image within `image_container_view()`.
  void PerformLargeImageAnimation();

  // Animations when toggle inline settings.
  void PerformToggleInlineSettingsAnimation(bool should_show_inline_settings);

  // Fade in animation when converting from single to group notification.
  void AnimateSingleToGroupFadeIn();

  // Calculate vertical space available on screen for the
  // grouped_notifications_scroll_view_
  int CalculateMaxHeightForGroupedNotifications();

  // Return true is `message_label()` is truncated. We need this helper because
  // Label::IsDisplayTextTruncated doesn't work when `message_label()` hasn't
  // been laid out yet.
  bool IsMessageLabelTruncated();

  // Attaches the large image's binary data as drop data. This method should be
  // called only if this notification view is draggable.
  void AttachBinaryImageAsDropData(ui::OSExchangeData* data);

  // Owned by views hierarchy.
  raw_ptr<views::View, ExperimentalAsh> main_view_ = nullptr;
  raw_ptr<views::View, ExperimentalAsh> main_right_view_ = nullptr;
  raw_ptr<RoundedImageView, ExperimentalAsh> app_icon_view_ = nullptr;
  raw_ptr<AshNotificationExpandButton, ExperimentalAsh> expand_button_ =
      nullptr;
  raw_ptr<views::View, ExperimentalAsh> left_content_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> message_label_in_expanded_state_ =
      nullptr;
  raw_ptr<views::ScrollView, ExperimentalAsh>
      grouped_notifications_scroll_view_ = nullptr;
  raw_ptr<views::View, ExperimentalAsh> grouped_notifications_container_ =
      nullptr;
  raw_ptr<views::View, ExperimentalAsh> collapsed_summary_view_ = nullptr;
  raw_ptr<message_center::NotificationControlButtonsView, ExperimentalAsh>
      control_buttons_view_ = nullptr;
  raw_ptr<views::LabelButton, ExperimentalAsh> turn_off_notifications_button_ =
      nullptr;
  raw_ptr<views::LabelButton, ExperimentalAsh> inline_settings_cancel_button_ =
      nullptr;
  raw_ptr<views::View, ExperimentalAsh> snooze_button_spacer_ = nullptr;
  raw_ptr<IconButton, ExperimentalAsh> snooze_button_ = nullptr;

  // These views below are dynamically created inside view hierarchy.
  raw_ptr<NotificationTitleRow, DanglingUntriaged | ExperimentalAsh>
      title_row_ = nullptr;

  // Layout manager for the container of header and left content.
  raw_ptr<views::BoxLayout, ExperimentalAsh> header_left_content_layout_ =
      nullptr;

  // Corner radius of the notification view.
  int top_radius_ = 0;
  int bottom_radius_ = 0;

  // Count of grouped notifications contained in this view. Used for
  // modifying the visibility of the title and content views in the parent
  // notification as well as showing the number of grouped notifications not
  // shown in a collapsed grouped notification.
  int total_grouped_notifications_ = 0;

  // Cached background color to avoid unnecessary update.
  SkColor background_color_ = SK_ColorTRANSPARENT;

  // Used to prevent setting bounds in `AshNotificationView` while running
  // animations to resize this view.
  bool is_animating_ = false;

  // Whether the notification associated with this view is a parent or child
  // in a grouped notification. Used to update visibility of UI elements
  // specific to each type of notification.
  bool is_grouped_parent_view_ = false;
  bool is_grouped_child_view_ = false;

  // Whether this view is shown in a notification popup.
  bool shown_in_popup_ = false;

  base::ScopedObservation<message_center::MessageCenter, MessageCenterObserver>
      message_center_observer_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  base::WeakPtrFactory<AshNotificationView> weak_factory_{this};
};

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */,
                    ash::AshNotificationView::GroupedNotificationsContainer)

#endif  // ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_VIEW_H_

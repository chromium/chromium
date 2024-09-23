// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_ASH_NOTIFICATION_VIEW_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_ASH_NOTIFICATION_VIEW_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
class TimestampView;

// Customized NotificationView for notification on ChromeOS. This view is used
// to displays all current types of notification on ChromeOS (web, basic, image,
// and list) except custom notification.
class ASH_EXPORT AshNotificationView
    : public message_center::NotificationViewBase,
      public message_center::MessageCenterObserver,
      public views::WidgetObserver {
  METADATA_HEADER(AshNotificationView, message_center::NotificationViewBase)

 public:
  // TODO(crbug/1241983): Add metadata and builder support to this view.
  explicit AshNotificationView(const message_center::Notification& notification,
                               bool shown_in_popup);
  AshNotificationView(const AshNotificationView&) = delete;
  AshNotificationView& operator=(const AshNotificationView&) = delete;
  ~AshNotificationView() override;

  // Toggle the expand state of the notification. This function should only be
  // used to handle user manually expand/collapse a notification.
  void ToggleExpand();

  // Called when a child notificaiton's preferred size changes.
  void GroupedNotificationsPreferredSizeChanged();

  // Drag related functions ----------------------------------------------------

  // Returns the bounds of the area where the drag can be initiated. The
  // returned bounds are in `AshNotificationView` local coordinates. Returns
  // `std::nullopt` if the notification view is not draggable.
  std::optional<gfx::Rect> GetDragAreaBounds() const;

  // Returns the drag image shown when the ash notification is under drag.
  // Returns `std::nullopt` if the notification view is not draggable.
  std::optional<gfx::ImageSkia> GetDragImage();

  // Attaches the drop data. This method should be called only if this
  // notification view is draggable.
  void AttachDropData(ui::OSExchangeData* data);

  // Returns true if this notification view is draggable.
  bool IsDraggable() const;

  // message_center::MessageView:
  void AnimateGroupedChildExpandedCollapse(bool expanded) override;
  void AnimateSingleToGroup(const std::string& notification_id,
                            std::string parent_id) override;
  void AddGroupNotification(
      const message_center::Notification& notification) override;
  void PopulateGroupNotifications(
      const std::vector<const message_center::Notification*>& notifications)
      override;
  void RemoveGroupNotification(const std::string& notification_id) override;
  void SetGroupedChildExpanded(bool expanded) override;
  // Called after `PreferredSizeChanged()`, so the current state is the target
  // state.
  base::TimeDelta GetBoundsAnimationDuration(
      const message_center::Notification& notification) const override;

  // message_center::NotificationViewBase:
  void AddedToWidget() override;
  void Layout(PassKey) override;
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
  void CreateOrUpdateSnoozeSettingsViews(
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
  void OnThemeChanged() override;
  std::unique_ptr<message_center::NotificationInputContainer>
  GenerateNotificationInputContainer() override;
  std::unique_ptr<views::LabelButton> GenerateNotificationLabelButton(
      views::Button::PressedCallback callback,
      const std::u16string& label) override;
  gfx::Size GetIconViewSize() const override;
  int GetLargeImageViewMaxWidth() const override;
  void ToggleInlineSettings(const ui::Event& event) override;
  void ToggleSnoozeSettings(const ui::Event& event) override;
  void OnInlineReplyUpdated() override;
  views::View* FindGroupNotificationView(
      const std::string& notification_id) override;

  void set_is_animating(bool is_animating) { is_animating_ = is_animating; }
  bool is_animating() { return is_animating_; }

  AshNotificationExpandButton* expand_button_for_test() {
    return expand_button_;
  }

  message_center::NotificationControlButtonsView*
  control_buttons_view_for_test() {
    return control_buttons_view_;
  }

  std::vector<raw_ptr<views::LabelButton, VectorExperimental>>
  GetActionButtonsForTest();

  views::Label* GetTitleRowLabelForTest();

  message_center::NotificationInputContainer* GetInlineReplyForTest();

  // View containing all grouped notifications, propagates size changes
  // to the parent notification view.
  class GroupedNotificationsContainer : public views::BoxLayoutView {
    METADATA_HEADER(GroupedNotificationsContainer, views::BoxLayoutView)

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
    raw_ptr<AshNotificationView> parent_notification_view_ = nullptr;
  };
  BEGIN_VIEW_BUILDER(/*no export*/,
                     GroupedNotificationsContainer,
                     views::BoxLayoutView)
  VIEW_BUILDER_PROPERTY(AshNotificationView*, ParentNotificationView)
  END_VIEW_BUILDER

 private:
  friend class AshNotificationViewTestBase;
  friend class MessageCenterMetricsUtilsTest;
  friend class NotificationGroupingControllerTest;

  // Customized title row for this notification view with added timestamp in
  // collapse mode.
  class NotificationTitleRow : public views::View {
    METADATA_HEADER(NotificationTitleRow, views::View)

   public:
    explicit NotificationTitleRow(const std::u16string& title);
    NotificationTitleRow(const NotificationTitleRow&) = delete;
    NotificationTitleRow& operator=(const NotificationTitleRow&) = delete;
    ~NotificationTitleRow() override;

    // Update title view's text.
    void UpdateTitle(const std::u16string& title);

    // Update the text for `timestamp_in_collapsed_view_`.
    void UpdateTimestamp(base::Time timestamp);

    // Update children's visibility based on the state of expand/collapse.
    void UpdateVisibility(bool in_collapsed_mode);

    // Perform expand/collapse animation in children views.
    void PerformExpandCollapseAnimation();

    // Set the maximum available width for this view.
    void SetMaxAvailableWidth(int max_available_width);

    // views::View:
    gfx::Size CalculatePreferredSize(
        const views::SizeBounds& available_size) const override;
    void OnThemeChanged() override;

    views::Label* title_view() { return title_view_; }

   private:
    friend class AshNotificationViewTestBase;
    // Showing notification title.
    const raw_ptr<views::Label> title_view_;

    // Timestamp view shown alongside the title in collapsed state.
    const raw_ptr<views::Label> title_row_divider_;
    const raw_ptr<TimestampView> timestamp_in_collapsed_view_;

    // The maximum width available to the title row.
    int max_available_width_ = 0;
  };

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

  // Get the available space for `message_label_in_expanded_state_` width.
  int GetExpandedMessageLabelWidth();

  // Update the color and icon for `app_icon_view_`.
  void UpdateAppIconView(const message_center::Notification* notification);

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

  // Called when the fade out animation for `view` has ended. This function
  // resets the views's opacity to 1.0f and makes it invisible.
  void OnFadeOutAnimationEnded(views::View* view);

  // Called when the grouped animation for this view has ended, or has been
  // aborted.
  void OnGroupedAnimationEnded(views::View* left_content,
                               views::View* right_content,
                               views::View* message_label_in_expanded_state,
                               views::View* image_container_view,
                               views::View* action_buttons_row,
                               AshNotificationExpandButton* expand_button,
                               std::string notification_id,
                               std::string parent_id);

  // A helper wrapping `OnFadeOutAnimationEnded` for `view` as a closure.
  base::OnceClosure OnFadeOutAnimationEndedClosure(views::View* view);

  // A helper for grouped animations ending/aborting.
  base::OnceClosure OnGroupedAnimationEndedClosure(
      views::View* left_content,
      views::View* right_content,
      views::View* message_label_in_expanded_state,
      views::View* image_container_view,
      views::View* action_buttons_row,
      AshNotificationExpandButton* expand_button,
      const std::string& notification_id,
      std::string parent_id);

  // Owned by views hierarchy.
  raw_ptr<views::View> main_view_ = nullptr;
  raw_ptr<views::View> main_right_view_ = nullptr;
  raw_ptr<RoundedImageView> app_icon_view_ = nullptr;
  raw_ptr<AshNotificationExpandButton> expand_button_ = nullptr;
  raw_ptr<views::View> left_content_ = nullptr;
  raw_ptr<views::Label> message_label_in_expanded_state_ = nullptr;
  raw_ptr<views::ScrollView> grouped_notifications_scroll_view_ = nullptr;
  raw_ptr<views::View> grouped_notifications_container_ = nullptr;
  raw_ptr<views::View> collapsed_summary_view_ = nullptr;
  raw_ptr<message_center::NotificationControlButtonsView>
      control_buttons_view_ = nullptr;
  raw_ptr<views::View> snooze_button_spacer_ = nullptr;
  raw_ptr<IconButton> snooze_button_ = nullptr;

  // These views below are dynamically created inside view hierarchy.
  raw_ptr<NotificationTitleRow, DanglingUntriaged> title_row_ = nullptr;

  // Layout manager for the container of header and left content.
  raw_ptr<views::BoxLayout> header_left_content_layout_ = nullptr;

  // Corner radius of the notification view.
  int top_radius_ = 0;
  int bottom_radius_ = 0;

  // Count of grouped notifications contained in this view. Used for
  // modifying the visibility of the title and content views in the parent
  // notification as well as showing the number of grouped notifications not
  // shown in a collapsed grouped notification.
  int total_grouped_notifications_ = 0;

  // Cached background color id to avoid unnecessary update.
  ui::ColorId background_color_id_ = 0;

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

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_ASH_NOTIFICATION_VIEW_H_

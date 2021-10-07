// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_VIEW_H_
#define ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_VIEW_H_

#include "ash/ash_export.h"
#include "base/timer/timer.h"
#include "ui/message_center/views/notification_input_container.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/message_center/views/notification_view_base.h"

namespace message_center {
class Notification;
}  // namespace message_center

namespace views {
class BoxLayout;
class ImageView;
class LabelButton;
}  // namespace views

namespace ash {

class RoundedImageView;

// Customized NotificationView for notification on ChromeOS. This view is used
// to displays all current types of notification on ChromeOS (web, basic, image,
// and list) except custom notification.
class ASH_EXPORT AshNotificationView
    : public message_center::NotificationViewBase {
 public:
  // TODO(crbug/1241983): Add metadata and builder support to this view.
  explicit AshNotificationView(const message_center::Notification& notification,
                               bool shown_in_popup);
  AshNotificationView(const AshNotificationView&) = delete;
  AshNotificationView& operator=(const AshNotificationView&) = delete;
  ~AshNotificationView() override;

  // Create a view containing the title and message for the notification in a
  // single line. This is used when a grouped child notification is in a
  // collapsed parent notification.
  std::unique_ptr<views::View> CreateCollapsedSummaryView(
      const message_center::Notification& notification);

  // Update the expanded state for grouped child notification.
  void SetGroupedChildExpanded(bool expanded);

  // Toggle the expand state of the notification.
  void ToggleExpand();

  // message_center::MessageView:
  void AddGroupNotification(const message_center::Notification& notification,
                            bool newest_first) override;
  void PopulateGroupNotifications(
      const std::vector<const message_center::Notification*>& notifications)
      override;
  void RemoveGroupNotification(const std::string& notification_id) override;

  // message_center::NotificationViewBase:
  void UpdateViewForExpandedState(bool expanded) override;
  void UpdateWithNotification(
      const message_center::Notification& notification) override;
  void CreateOrUpdateTitleView(
      const message_center::Notification& notification) override;
  void CreateOrUpdateSmallIconView(
      const message_center::Notification& notification) override;
  bool IsIconViewShown() const override;
  void SetExpandButtonEnabled(bool enabled) override;
  void UpdateCornerRadius(int top_radius, int bottom_radius) override;
  void SetDrawBackgroundAsActive(bool active) override;
  void OnThemeChanged() override;
  std::unique_ptr<message_center::NotificationInputContainer>
  GenerateNotificationInputContainer() override;
  std::unique_ptr<views::LabelButton> GenerateNotificationLabelButton(
      views::Button::PressedCallback callback,
      const std::u16string& label) override;
  gfx::Size GetIconViewSize() const override;
  void ToggleInlineSettings(const ui::Event& event) override;

 private:
  friend class AshNotificationViewTest;

  // Customized title row for this notification view with added timestamp in
  // collapse mode.
  class NotificationTitleRow : public views::View {
   public:
    METADATA_HEADER(NotificationTitleRow);
    explicit NotificationTitleRow(const std::u16string& title);
    NotificationTitleRow(const NotificationTitleRow&) = delete;
    NotificationTitleRow& operator=(const NotificationTitleRow&) = delete;
    ~NotificationTitleRow() override;

    // Changed the expand state. Title view size will change based on the state.
    void SetExpanded(bool expanded);

    // Update title view's text.
    void UpdateTitle(const std::u16string& title);

    // Update the text for `timestamp_in_collapsed_view_`. Also used the timer
    // to re-update this timestamp view when the next update is needed.
    void UpdateTimestamp(base::Time timestamp);

    // Update children's visibility based on the state of expand/collapse.
    void UpdateVisibility(bool in_collapsed_mode);

   private:
    friend class AshNotificationViewTest;

    // Showing notification title.
    views::Label* const title_view_;

    // Timestamp view shown alongside the title in collapsed state.
    views::Label* const title_row_divider_;
    views::Label* const timestamp_in_collapsed_view_;

    // Timer that updates the timestamp over time.
    base::OneShotTimer timestamp_update_timer_;
    absl::optional<base::Time> timestamp_;
  };

  // Customized expand button for this notification view. Used for grouped as
  // well as singular notifications.
  class ExpandButton : public views::Button {
   public:
    METADATA_HEADER(ExpandButton);
    explicit ExpandButton(PressedCallback callback);
    ExpandButton(const ExpandButton&) = delete;
    ExpandButton& operator=(const ExpandButton&) = delete;
    ~ExpandButton() override;

    // Change the expanded state. The icon will change.
    void SetExpanded(bool expanded);

    // Whether the label displaying the number of notifications in a grouped
    // notification needs to be displayed.
    bool ShouldShowLabel() const;

    // Update the count of total grouped notifications in the parent view and
    // update the text for the label accordingly.
    void UpdateGroupedNotificationsCount(int count);

    // Generate the icons used for chevron in the expanded and collapsed state.
    void UpdateIcons();

    // views::Button:
    gfx::Size CalculatePreferredSize() const override;
    void OnThemeChanged() override;

    views::Label* label_for_test() { return label_; }

   private:
    // Owned by views hierarchy.
    views::Label* label_;
    views::ImageView* image_;

    // Cached icons used to display the chevron in the button.
    gfx::ImageSkia expanded_image_;
    gfx::ImageSkia collapsed_image_;

    // total number of grouped child notifications in this button's parent view.
    int total_grouped_notifications_ = 0;

    // The expand state of the button.
    bool expanded_ = false;
  };

  // Update the background color with rounded corner.
  void UpdateBackground(int top_radius, int bottom_radius);

  // Owned by views hierarchy.
  RoundedImageView* app_icon_view_ = nullptr;
  ExpandButton* expand_button_ = nullptr;
  views::View* left_content_ = nullptr;
  views::View* grouped_notifications_container_ = nullptr;
  views::View* collapsed_summary_view_ = nullptr;
  views::View* control_buttons_view_ = nullptr;
  views::View* main_view_ = nullptr;
  views::BoxLayout* const layout_manager_ = nullptr;

  // These views below are dynamically created inside view hierarchy.
  NotificationTitleRow* title_row_ = nullptr;

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
  // Whether the notification associated with this view is a parent or child
  // in a grouped notification. Used to update visibility of UI elements
  // specific to each type of notification.
  bool is_grouped_parent_view_ = false;
  bool is_grouped_child_view_ = false;
  // Whether this view is shown in a notification popup.
  bool shown_in_popup_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_VIEW_H_

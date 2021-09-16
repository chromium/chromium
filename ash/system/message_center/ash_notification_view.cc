// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/ash_notification_view.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/message_center/ash_notification_input_container.h"
#include "ash/system/message_center/message_center_style.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/check.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_background_painter.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/message_center/views/relative_time_formatter.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace {

constexpr int kTitleRowSpacing = 6;

// Bullet character. The divider symbol between the title and the timestamp.
constexpr char16_t kTitleRowDivider[] = u"\u2022";

constexpr int kExpandButtonSize = 24;
constexpr int kTitleCharacterLimit =
    message_center::kNotificationWidth * message_center::kMaxTitleLines /
    message_center::kMinPixelsPerTitleCharacter;

}  // namespace

namespace ash {

using Orientation = views::BoxLayout::Orientation;

BEGIN_METADATA(AshNotificationView, NotificationTitleRow, views::View)
END_METADATA
BEGIN_METADATA(AshNotificationView, ExpandButton, views::ImageButton)
END_METADATA

AshNotificationView::NotificationTitleRow::NotificationTitleRow(
    const std::u16string& title)
    : title_view_(AddChildView(GenerateTitleView(title))),
      title_row_divider_(AddChildView(std::make_unique<views::Label>(
          kTitleRowDivider,
          views::style::CONTEXT_DIALOG_BODY_TEXT))),
      timestamp_in_collapsed_view_(
          AddChildView(std::make_unique<views::Label>())) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kHorizontal, gfx::Insets(), kTitleRowSpacing));
}

AshNotificationView::NotificationTitleRow::~NotificationTitleRow() {
  timestamp_update_timer_.Stop();
}

void AshNotificationView::NotificationTitleRow::UpdateTitle(
    const std::u16string& title) {
  title_view_->SetText(title);
}

void AshNotificationView::NotificationTitleRow::UpdateTimestamp(
    base::Time timestamp) {
  std::u16string relative_time;
  base::TimeDelta next_update;
  message_center::GetRelativeTimeStringAndNextUpdateTime(
      timestamp - base::Time::Now(), &relative_time, &next_update);

  timestamp_ = timestamp;
  timestamp_in_collapsed_view_->SetText(relative_time);

  // Unretained is safe as the timer cancels the task on destruction.
  timestamp_update_timer_.Start(
      FROM_HERE, next_update,
      base::BindOnce(&NotificationTitleRow::UpdateTimestamp,
                     base::Unretained(this), timestamp));
}

void AshNotificationView::NotificationTitleRow::UpdateVisibility(
    bool in_collapsed_mode) {
  timestamp_in_collapsed_view_->SetVisible(in_collapsed_mode);
  title_row_divider_->SetVisible(in_collapsed_mode);
}

AshNotificationView::ExpandButton::ExpandButton(PressedCallback callback)
    : ImageButton(std::move(callback)) {
  views::InstallCircleHighlightPathGenerator(this);
  TrayPopupUtils::ConfigureTrayPopupButton(this);
}

AshNotificationView::ExpandButton::~ExpandButton() = default;

void AshNotificationView::ExpandButton::SetExpanded(bool expanded) {
  if (expanded_ == expanded)
    return;
  expanded_ = expanded;
  SetTooltipText(l10n_util::GetStringUTF16(
      expanded_ ? IDS_ASH_NOTIFICATION_COLLAPSE_TOOLTIP
                : IDS_ASH_NOTIFICATION_EXPAND_TOOLTIP));
  SchedulePaint();
}

gfx::Size AshNotificationView::ExpandButton::CalculatePreferredSize() const {
  return gfx::Size(kExpandButtonSize, kExpandButtonSize);
}

void AshNotificationView::ExpandButton::PaintButtonContents(
    gfx::Canvas* canvas) {
  gfx::ScopedCanvas scoped(canvas);
  canvas->Translate(gfx::Vector2d(size().width() / 2, size().height() / 2));
  if (!expanded_)
    canvas->sk_canvas()->rotate(180.);
  gfx::ImageSkia image = GetImageToPaint();
  canvas->DrawImageInt(image, -image.width() / 2, -image.height() / 2);
}

void AshNotificationView::ExpandButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();

  const gfx::ImageSkia image = gfx::CreateVectorIcon(
      kUnifiedMenuExpandIcon, kExpandButtonSize,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary));
  SetImage(views::Button::STATE_NORMAL, image);

  views::FocusRing::Get(this)->SetColor(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kFocusRingColor));

  SkColor background_color = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  SetBackground(views::CreateRoundedRectBackground(background_color,
                                                   kTrayItemCornerRadius));
}

AshNotificationView::AshNotificationView(
    const message_center::Notification& notification,
    bool shown_in_popup)
    : NotificationViewBase(notification), shown_in_popup_(shown_in_popup) {
  // TODO(crbug/1232197): fix views and layout to match spec.
  // Instantiate view instances and define layout and view hierarchy.
  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kVertical, gfx::Insets(), 0));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kEnd);

  control_buttons_view_ = AddChildView(CreateControlButtonsView());

  // Header left content contains header row and left content.
  auto header_left_content = std::make_unique<views::View>();
  header_left_content->SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kVertical, gfx::Insets(), 0));
  header_left_content->AddChildView(CreateHeaderRow());
  left_content_ = header_left_content->AddChildView(CreateLeftContentView());

  auto content_row = CreateContentRow();
  auto* header_left_content_ptr =
      content_row->AddChildView(std::move(header_left_content));
  static_cast<views::BoxLayout*>(content_row->GetLayoutManager())
      ->SetFlexForView(header_left_content_ptr, 1);
  content_row->AddChildView(CreateRightContentView());

  expand_button_ = content_row->AddChildView(
      std::make_unique<ExpandButton>(base::BindRepeating(
          &AshNotificationView::ToggleExpand, base::Unretained(this))));

  // TODO(crbug/1241990): add an icon view here.
  auto icon_view = std::make_unique<views::View>();

  // Main right view contains all the views besides control buttons and icon.
  auto main_right_view = std::make_unique<views::View>();
  main_right_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kVertical, gfx::Insets(), 0));
  main_right_view->AddChildView(std::move(content_row));
  main_right_view->AddChildView(CreateInlineSettingsView());
  main_right_view->AddChildView(CreateImageContainerView());
  main_right_view->AddChildView(CreateActionsRow());

  // Main view contains all the views besides control buttons.
  auto main_view = std::make_unique<views::View>();
  main_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kHorizontal, gfx::Insets(), 0));
  main_view->AddChildView(std::move(icon_view));
  main_view->AddChildView(std::move(main_right_view));
  main_view_ = AddChildView(std::move(main_view));

  collapsed_summary_view_ =
      AddChildView(CreateCollapsedSummaryView(notification));

  auto grouped_notifications_container = std::make_unique<views::View>();
  grouped_notifications_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          Orientation::kVertical,
          message_center_style::kGroupedNotificationContainerInsets,
          /*between_child_spacing=*/0));
  grouped_notifications_container_ =
      AddChildView(std::move(grouped_notifications_container));

  auto collapsed_count_view = std::make_unique<views::Label>();
  collapsed_count_view->SetFontList(views::Label::GetDefaultFontList().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  collapsed_count_view->SetBorder(views::CreateEmptyBorder(
      message_center_style::kGroupedCollapsedCountViewInsets));
  collapsed_count_view_ = AddChildView(std::move(collapsed_count_view));

  if (shown_in_popup_ && !notification.group_child()) {
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  }

  UpdateWithNotification(notification);
}

AshNotificationView::~AshNotificationView() = default;

void AshNotificationView::CreateOrUpdateTitleView(
    const message_center::Notification& notification) {
  if (notification.title().empty()) {
    if (title_row_) {
      DCHECK(left_content()->Contains(title_row_));
      left_content()->RemoveChildViewT(title_row_);
      title_row_ = nullptr;
    }
    return;
  }

  const std::u16string& title = gfx::TruncateString(
      notification.title(), kTitleCharacterLimit, gfx::WORD_BREAK);

  if (!title_row_) {
    title_row_ =
        AddViewToLeftContent(std::make_unique<NotificationTitleRow>(title));
  } else {
    title_row_->UpdateTitle(title);
    ReorderViewInLeftContent(title_row_);
  }

  title_row_->UpdateTimestamp(notification.timestamp());
}

void AshNotificationView::ToggleExpand() {
  SetExpanded(!IsExpanded());
}

void AshNotificationView::AddGroupNotification(
    const message_center::Notification& notification,
    bool newest_first) {
  auto notification_view =
      std::make_unique<AshNotificationView>(notification,
                                            /*shown_in_popup=*/false);
  notification_view->SetVisible(
      total_grouped_notifications_ <
          message_center_style::kMaxGroupedNotificationsInCollapsedState ||
      IsExpanded());
  notification_view->SetGroupedChildExpanded(IsExpanded());
  grouped_notifications_container_->AddChildViewAt(
      std::move(notification_view),
      newest_first ? 0 : grouped_notifications_container_->children().size());

  total_grouped_notifications_++;
  left_content_->SetVisible(false);
  UpdateCollapsedCountView();
  PreferredSizeChanged();
}

void AshNotificationView::PopulateGroupNotifications(
    const std::vector<const message_center::Notification*>& notifications) {
  DCHECK(total_grouped_notifications_ == 0);
  for (auto* notification : notifications) {
    auto notification_view =
        std::make_unique<AshNotificationView>(*notification,
                                              /*shown_in_popup=*/false);
    notification_view->SetVisible(
        total_grouped_notifications_ <
            message_center_style::kMaxGroupedNotificationsInCollapsedState ||
        IsExpanded());
    notification_view->SetGroupedChildExpanded(IsExpanded());
    grouped_notifications_container_->AddChildViewAt(
        std::move(notification_view), 0);
  }
  total_grouped_notifications_ = notifications.size();
  left_content_->SetVisible(total_grouped_notifications_ == 0);
  UpdateCollapsedCountView();
}

void AshNotificationView::RemoveGroupNotification(
    const std::string& notification_id) {
  AshNotificationView* to_be_deleted = nullptr;
  for (auto* child : grouped_notifications_container_->children()) {
    AshNotificationView* group_notification =
        static_cast<AshNotificationView*>(child);
    if (group_notification->notification_id() == notification_id) {
      to_be_deleted = group_notification;
      break;
    }
  }
  if (to_be_deleted)
    delete to_be_deleted;

  total_grouped_notifications_--;
  left_content_->SetVisible(total_grouped_notifications_ == 0);
  UpdateCollapsedCountView();
  PreferredSizeChanged();
}

std::unique_ptr<views::View> AshNotificationView::CreateCollapsedSummaryView(
    const message_center::Notification& notification) {
  auto collapsed_summary_view = std::make_unique<views::View>();
  collapsed_summary_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      message_center_style::kGroupedCollapsedSummaryInsets,
      message_center_style::kGroupedCollapsedSummaryLabelSpacing));
  collapsed_summary_view->SetVisible(false);

  auto title_label = std::make_unique<views::Label>(
      notification.title(), views::style::CONTEXT_DIALOG_BODY_TEXT);
  title_label->SetMaximumWidthSingleLine(
      message_center_style::kGroupedCollapsedSummaryTitleLength);

  auto content_label = std::make_unique<views::Label>(
      notification.message(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  content_label->SetMaximumWidthSingleLine(
      message_center_style::kGroupedCollapsedSummaryMessageLength);

  collapsed_summary_view->AddChildView(std::move(title_label));
  collapsed_summary_view->AddChildView(std::move(content_label));

  return collapsed_summary_view;
}

void AshNotificationView::UpdateCollapsedCountView() {
  collapsed_count_view_->SetVisible(
      is_grouped_parent_view_ && !IsExpanded() &&
      total_grouped_notifications_ >
          message_center_style::kMaxGroupedNotificationsInCollapsedState);
  collapsed_count_view_->SetText(l10n_util::GetStringFUTF16Int(
      IDS_ASH_MESSAGE_CENTER_HIDDEN_NOTIFICATION_COUNT_LABEL,
      total_grouped_notifications_ -
          message_center_style::kMaxGroupedNotificationsInCollapsedState));
}

void AshNotificationView::SetGroupedChildExpanded(bool expanded) {
  collapsed_summary_view_->SetVisible(!expanded);
  main_view_->SetVisible(expanded);
  control_buttons_view_->SetVisible(expanded);
}

void AshNotificationView::UpdateViewForExpandedState(bool expanded) {
  bool is_single_expanded_notification =
      !is_grouped_child_view_ && !is_grouped_parent_view_ && expanded;
  header_row()->SetVisible(is_grouped_parent_view_ ||
                           (is_single_expanded_notification));

  // TODO(crbug/1243889): call SizeToFit() `title_view_` to fit the space after
  // the padding and spacing are done.
  if (title_row_)
    title_row_->UpdateVisibility(is_grouped_child_view_ ||
                                 (IsExpandable() && !expanded));

  expand_button_->SetVisible(IsExpandable());
  expand_button_->SetExpanded(expanded);

  int notification_count = 0;
  for (auto* child : grouped_notifications_container_->children()) {
    auto* notification_view = static_cast<AshNotificationView*>(child);
    notification_view->SetGroupedChildExpanded(expanded);
    notification_count++;
    if (notification_count >
        message_center_style::kMaxGroupedNotificationsInCollapsedState) {
      notification_view->SetVisible(expanded);
    }
  }

  UpdateCollapsedCountView();

  NotificationViewBase::UpdateViewForExpandedState(expanded);
}

void AshNotificationView::UpdateWithNotification(
    const message_center::Notification& notification) {
  is_grouped_child_view_ = notification.group_child();
  is_grouped_parent_view_ = notification.group_parent();
  grouped_notifications_container_->SetVisible(is_grouped_parent_view_);
  header_row()->SetVisible(!is_grouped_child_view_);
  UpdateCollapsedCountView();
  NotificationViewBase::UpdateWithNotification(notification);
}

void AshNotificationView::SetExpandButtonEnabled(bool enabled) {
  expand_button_->SetVisible(enabled);
}

void AshNotificationView::UpdateCornerRadius(int top_radius,
                                             int bottom_radius) {
  // Call parent's SetCornerRadius to update radius used for highlight path.
  NotificationViewBase::SetCornerRadius(top_radius, bottom_radius);
  UpdateBackground(top_radius, bottom_radius);
}

void AshNotificationView::SetDrawBackgroundAsActive(bool active) {}

void AshNotificationView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateBackground(top_radius_, bottom_radius_);
}

std::unique_ptr<message_center::NotificationInputContainer>
AshNotificationView::GenerateNotificationInputContainer() {
  return std::make_unique<AshNotificationInputContainer>(this);
}

void AshNotificationView::UpdateBackground(int top_radius, int bottom_radius) {
  SkColor background_color;
  if (shown_in_popup_) {
    background_color = AshColorProvider::Get()->GetBaseLayerColor(
        AshColorProvider::BaseLayerType::kTransparent80);
  } else {
    background_color = AshColorProvider::Get()->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  }

  if (background_color == background_color_ && top_radius_ == top_radius &&
      bottom_radius_ == bottom_radius) {
    return;
  }

  if (!is_grouped_child_view_)
    background_color_ = background_color;
  top_radius_ = top_radius;
  bottom_radius_ = bottom_radius;

  SetBackground(views::CreateBackgroundFromPainter(
      std::make_unique<message_center::NotificationBackgroundPainter>(
          top_radius_, bottom_radius_, background_color_)));
}
void AshNotificationView::UpdateActionButtonsRowBackground() {
  // No background.
}

}  // namespace ash

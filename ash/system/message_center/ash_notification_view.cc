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
  // Instantiate view instances and define layout and view hierarchy.
  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kVertical, gfx::Insets(), 0));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kEnd);

  // TODO(crbug/1232197): fix views and layout to match spec.

  // Main view contains all the views besides control buttons.
  auto main_view = std::make_unique<views::View>();
  main_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kHorizontal, gfx::Insets(), 0));

  // TODO(crbug/1241990): add an icon view here.
  auto icon_view = std::make_unique<views::View>();

  // Main right view contains all the views besides control buttons and icon.
  auto main_right_view = std::make_unique<views::View>();
  main_right_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kVertical, gfx::Insets(), 0));

  auto content_row = CreateContentRow();

  // Header left content contains header row and left content.
  auto header_left_content = std::make_unique<views::View>();
  header_left_content->SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kVertical, gfx::Insets(), 0));

  auto expand_button = std::make_unique<ExpandButton>(base::BindRepeating(
      &AshNotificationView::ToggleExpand, base::Unretained(this)));

  auto* header_row = header_left_content->AddChildView(CreateHeaderRow());
  left_content_ = header_left_content->AddChildView(CreateLeftContentView());

  auto* header_left_content_ptr =
      content_row->AddChildView(std::move(header_left_content));
  static_cast<views::BoxLayout*>(content_row->GetLayoutManager())
      ->SetFlexForView(header_left_content_ptr, 1);
  content_row->AddChildView(CreateRightContentView());
  expand_button_ = content_row->AddChildView(std::move(expand_button));

  main_right_view->AddChildView(std::move(content_row));
  main_right_view->AddChildView(CreateInlineSettingsView());
  main_right_view->AddChildView(CreateImageContainerView());
  main_right_view->AddChildView(CreateActionsRow());

  main_view->AddChildView(std::move(icon_view));
  main_view->AddChildView(std::move(main_right_view));

  AddChildView(CreateControlButtonsView());
  AddChildView(std::move(main_view));

  if (notification.group_child()) {
    header_row->SetVisible(false);
    show_background_color_ = false;
  }

  grouped_notifications_container_ =
      AddChildView(std::make_unique<views::View>());
  grouped_notifications_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(Orientation::kVertical, gfx::Insets(),
                                         0));

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
  grouped_notifications_container_->AddChildViewAt(
      std::make_unique<AshNotificationView>(notification,
                                            /*shown_in_popup=*/false),
      newest_first ? 0 : grouped_notifications_container_->children().size());

  total_grouped_notifications_++;
  left_content_->SetVisible(false);
  PreferredSizeChanged();
}

void AshNotificationView::PopulateGroupNotifications(
    const std::vector<const message_center::Notification*>& notifications) {
  DCHECK(total_grouped_notifications_ == 0);
  for (auto* notification : notifications) {
    grouped_notifications_container_->AddChildViewAt(
        std::make_unique<AshNotificationView>(*notification,
                                              /*shown_in_popup=*/false),
        0);
  }
  total_grouped_notifications_ = notifications.size();
  left_content_->SetVisible(total_grouped_notifications_ == 0);
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
  PreferredSizeChanged();
}

void AshNotificationView::UpdateViewForExpandedState(bool expanded) {
  header_row()->SetVisible(expanded);

  // TODO(crbug/1243889): call SizeToFit() `title_view_` to fit the space after
  // the padding and spacing are done.
  if (title_row_)
    title_row_->UpdateVisibility(IsExpandable() && !expanded);

  expand_button_->SetVisible(IsExpandable());
  expand_button_->SetExpanded(expanded);

  NotificationViewBase::UpdateViewForExpandedState(expanded);
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

  if (show_background_color_)
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

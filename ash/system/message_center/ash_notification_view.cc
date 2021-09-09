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
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/message_center/views/notification_background_painter.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace {
constexpr int kExpandButtonSize = 24;
}  // namespace

namespace ash {

BEGIN_METADATA(AshNotificationView, ExpandButton, views::ImageButton)
END_METADATA

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
  using Orientation = views::BoxLayout::Orientation;

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

  header_left_content->AddChildView(CreateHeaderRow());
  header_left_content->AddChildView(CreateLeftContentView());

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

  CreateOrUpdateViews(notification);
  UpdateControlButtonsVisibilityWithNotification(notification);

  expand_button_->SetVisible(IsExpandable());

  if (shown_in_popup_) {
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  }
}

AshNotificationView::~AshNotificationView() = default;

void AshNotificationView::UpdateWithNotification(
    const message_center::Notification& notification) {
  NotificationViewBase::UpdateWithNotification(notification);
  expand_button_->SetVisible(IsExpandable());
}

void AshNotificationView::ToggleExpand() {
  SetExpanded(!IsExpanded());
}

void AshNotificationView::SetExpanded(bool expanded) {
  expand_button_->SetExpanded(expanded);
  NotificationViewBase::SetExpanded(expanded);
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
  background_color_ = background_color;
  top_radius_ = top_radius;
  bottom_radius_ = bottom_radius;

  SetBackground(views::CreateBackgroundFromPainter(
      std::make_unique<message_center::NotificationBackgroundPainter>(
          top_radius_, bottom_radius_, background_color_)));
}

}  // namespace ash
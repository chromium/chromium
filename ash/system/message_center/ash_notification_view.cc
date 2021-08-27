// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/ash_notification_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

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
    const message_center::Notification& notification)
    : NotificationView(notification) {
  auto expand_button = std::make_unique<ExpandButton>(base::BindRepeating(
      &AshNotificationView::ToggleExpand, base::Unretained(this)));
  expand_button->SetVisible(IsExpandable());
  expand_button_ = GetViewByID(NotificationView::kContentRow)
                       ->AddChildView(std::move(expand_button));
}

AshNotificationView::~AshNotificationView() = default;

void AshNotificationView::UpdateWithNotification(
    const message_center::Notification& notification) {
  NotificationView::UpdateWithNotification(notification);
  expand_button_->SetVisible(IsExpandable());
}

void AshNotificationView::ToggleExpand() {
  SetExpanded(!IsExpanded());
}

void AshNotificationView::SetExpanded(bool expanded) {
  expand_button_->SetExpanded(expanded);
  NotificationView::SetExpanded(expanded);
}

void AshNotificationView::SetExpandButtonEnabled(bool enabled) {
  expand_button_->SetVisible(enabled);
}

}  // namespace ash
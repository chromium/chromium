// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_tray.h"

#include "ash/constants/tray_background_view_catalog.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"

namespace ash {

FocusModeTray::FocusModeTray(Shelf* shelf)
    : TrayBackgroundView(shelf,
                         TrayBackgroundViewCatalogName::kFocusMode,
                         RoundedCornerBehavior::kAllRounded),
      image_view_(tray_container()->AddChildView(
          std::make_unique<views::ImageView>())) {
  SetPressedCallback(base::BindRepeating(&FocusModeTray::FocusModeIconActivated,
                                         weak_ptr_factory_.GetWeakPtr()));

  image_view_->SetTooltipText(GetAccessibleNameForTray());
  image_view_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize));

  SetVisiblePreferred(FocusModeController::Get()->in_focus_session());
}

FocusModeTray::~FocusModeTray() {
  if (bubble_) {
    bubble_->bubble_view()->ResetDelegate();
  }
}

void FocusModeTray::ClickedOutsideBubble() {
  CloseBubble();
}

std::u16string FocusModeTray::GetAccessibleNameForTray() {
  // TODO(b/288975135): Update once we get UX writing.
  return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY);
}

void FocusModeTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view) {
    CloseBubble();
  }
}

void FocusModeTray::CloseBubble() {
  if (!bubble_) {
    return;
  }

  if (auto* bubble_view = bubble_->GetBubbleView()) {
    bubble_view->ResetDelegate();
  }

  bubble_.reset();
  SetIsActive(false);
}

void FocusModeTray::ShowBubble() {
  if (bubble_) {
    return;
  }

  auto bubble_view =
      std::make_unique<TrayBubbleView>(CreateInitParamsForTrayBubble(
          /*tray=*/this, /*anchor_to_shelf_corner=*/false));

  // TODO(b/286932322): Implement Focus Pod.

  bubble_ = std::make_unique<TrayBubbleWrapper>(this);
  bubble_->ShowBubble(std::move(bubble_view));

  SetIsActive(true);
}

void FocusModeTray::UpdateTrayItemColor(bool is_active) {
  CHECK(chromeos::features::IsJellyEnabled());
  UpdateTrayIcon();
}

void FocusModeTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  UpdateTrayIcon();
}

void FocusModeTray::UpdateTrayIcon() {
  SkColor color;
  if (chromeos::features::IsJellyEnabled()) {
    color = GetColorProvider()->GetColor(
        is_active() ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                    : cros_tokens::kCrosSysOnSurface);
  } else {
    color = GetColorProvider()->GetColor(kColorAshIconColorPrimary);
  }
  image_view_->SetImage(CreateVectorIcon(kFocusModeLampIcon, color));
}

void FocusModeTray::FocusModeIconActivated(const ui::Event& event) {
  if (bubble_ && bubble_->bubble_view()->GetVisible()) {
    CloseBubble();
    return;
  }

  ShowBubble();
}

}  // namespace ash

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/pods_overflow_tray.h"

#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

constexpr gfx::Insets kPodsBubbleInteriorMargin = gfx::Insets(8);
constexpr int kPodsBubbleBetweenChildSpacing = 8;
constexpr int kPodsBubbleCornerRadius = 24;

ui::ImageModel GetTrayContainerImage(bool is_active) {
  return ui::ImageModel::FromVectorIcon(
      /*vector_icon=*/kShelfOverflowHorizontalDotsIcon,
      /*color_id=*/is_active ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                             : cros_tokens::kCrosSysOnSurface);
}

}  // namespace

PodsOverflowTray::PodsOverflowTray(Shelf* shelf)
    : TrayBackgroundView(shelf, TrayBackgroundViewCatalogName::kPodsOverflow) {
  CHECK(features::IsScalableShelfPodsEnabled());
  SetCallback(base::BindRepeating(&PodsOverflowTray::OnTrayButtonPressed,
                                  weak_ptr_factory_.GetWeakPtr()));

  // TODO(b/337965177): Implement pods overflow tray container view.
  tray_icon_ = tray_container()->AddChildView(
      views::Builder<views::ImageView>()
          .SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize))
          .SetImage(GetTrayContainerImage(is_active()))
          .Build());

  // TODO(b/337925759): Make visible only when pods overflow on the shelf.
  TrayBackgroundView::SetVisiblePreferred(true);
}

PodsOverflowTray::~PodsOverflowTray() {
  if (bubble_) {
    bubble_->bubble_view()->ResetDelegate();
  }
}

std::u16string PodsOverflowTray::GetAccessibleNameForTray() {
  // TODO(b/337963043): Update a11y strings.
  return u"Pods overflow tray";
}

void PodsOverflowTray::HandleLocaleChange() {
  TooltipTextChanged();
}

void PodsOverflowTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view) {
    CloseBubble();
  }
}

void PodsOverflowTray::ClickedOutsideBubble(const ui::LocatedEvent& event) {
  CloseBubble();
}

void PodsOverflowTray::UpdateTrayItemColor(bool is_active) {
  tray_icon_->SetImage(GetTrayContainerImage(is_active));
}

void PodsOverflowTray::CloseBubbleInternal() {
  pods_container_ = nullptr;
  bubble_.reset();
  SetIsActive(false);
  shelf()->UpdateAutoHideState();
}

void PodsOverflowTray::ShowBubble() {
  TrayBubbleView::InitParams init_params = CreateInitParamsForTrayBubble(this);
  init_params.corner_radius = kPodsBubbleCornerRadius;
  std::unique_ptr<TrayBubbleView> bubble_view =
      std::make_unique<TrayBubbleView>(init_params);

  pods_container_ = bubble_view->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetInsideBorderInsets(kPodsBubbleInteriorMargin)
          .SetBetweenChildSpacing(kPodsBubbleBetweenChildSpacing)
          .Build());

  // TODO(b/337925727): Replace with real tray buttons.
  // Currently four views will be added that mock shelf pod buttons.
  for (int i = 0; i < 4; i++) {
    pods_container_->AddChildView(
        views::Builder<views::View>()
            .SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize))
            .SetBackground(views::CreateThemedRoundedRectBackground(
                cros_tokens::kCrosSysSystemOnBase, kPodsBubbleCornerRadius))
            .Build());
  }

  bubble_view->SetPreferredWidth(pods_container_->GetPreferredSize().width());

  bubble_ = std::make_unique<TrayBubbleWrapper>(this);
  bubble_->ShowBubble(std::move(bubble_view));
  SetIsActive(true);
}

TrayBubbleView* PodsOverflowTray::GetBubbleView() {
  return bubble_ ? bubble_->GetBubbleView() : nullptr;
}

views::Widget* PodsOverflowTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->GetBubbleWidget() : nullptr;
}

std::u16string PodsOverflowTray::GetAccessibleNameForBubble() {
  // TODO(b/337963043): Update a11y strings.
  return u"Pods overflow bubble";
}

bool PodsOverflowTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

void PodsOverflowTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void PodsOverflowTray::OnTrayButtonPressed() {
  if (GetBubbleWidget()) {
    CloseBubble();
    return;
  }

  ShowBubble();
}

BEGIN_METADATA(PodsOverflowTray)
END_METADATA

}  // namespace ash

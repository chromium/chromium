// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray.h"
#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/pinned_files_container.h"
#include "ash/system/holding_space/recent_files_container.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"

namespace ash {

namespace {

// Padding for tray icon (dp; the button that shows the palette menu).
constexpr int kTrayIconMainAxisInset = 6;

// Width of the holding space bubble itself (dp).
constexpr int kHoldingSpaceWidth = 360;

void SetupChildLayer(views::View* child) {
  child->SetPaintToLayer(ui::LAYER_SOLID_COLOR);

  auto* layer = child->layer();
  layer->SetRoundedCornerRadius(gfx::RoundedCornersF{kUnifiedTrayCornerRadius});
  layer->SetColor(UnifiedSystemTrayView::GetBackgroundColor());
  layer->SetBackgroundBlur(kUnifiedMenuBackgroundBlur);
  layer->SetFillsBoundsOpaquely(false);
  layer->SetIsFastRoundedCorner(true);
}

}  // namespace

HoldingSpaceTray::HoldingSpaceTray(Shelf* shelf) : TrayBackgroundView(shelf) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  icon_ = tray_container()->AddChildView(std::make_unique<views::ImageView>());
  icon_->set_tooltip_text(
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_SCREENSHOTS_TITLE));

  icon_->SetImage(CreateVectorIcon(kSystemMenuArrowBackIcon,
                                   ShelfConfig::Get()->shelf_icon_color()));

  tray_container()->SetMargin(kTrayIconMainAxisInset, 0);
}

HoldingSpaceTray::~HoldingSpaceTray() {
  if (bubble_)
    bubble_->bubble_view()->ResetDelegate();
}

bool HoldingSpaceTray::ContainsPointInScreen(const gfx::Point& point) {
  if (GetBoundsInScreen().Contains(point))
    return true;

  return bubble_ && bubble_->bubble_view()->GetBoundsInScreen().Contains(point);
}

void HoldingSpaceTray::ClickedOutsideBubble() {
  CloseBubble();
}

base::string16 HoldingSpaceTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_SCREENSHOTS_TITLE);
}

void HoldingSpaceTray::HandleLocaleChange() {
  icon_->set_tooltip_text(
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_SCREENSHOTS_TITLE));
}

void HoldingSpaceTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {}

base::string16 HoldingSpaceTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

bool HoldingSpaceTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

void HoldingSpaceTray::HideBubble(const TrayBubbleView* bubble_view) {
  CloseBubble();
}

void HoldingSpaceTray::AnchorUpdated() {
  if (bubble_)
    bubble_->bubble_view()->UpdateBubble();
}

bool HoldingSpaceTray::PerformAction(const ui::Event& event) {
  if (bubble_) {
    CloseBubble();
    return true;
  }

  ShowBubble(event.IsMouseEvent() || event.IsGestureEvent());
  return true;
}

void HoldingSpaceTray::UpdateAfterLoginStatusChange() {
  SetVisiblePreferred(true);
  PreferredSizeChanged();
}

void HoldingSpaceTray::CloseBubble() {
  bubble_.reset();
  SetIsActive(false);
}

void HoldingSpaceTray::ShowBubble(bool show_by_click) {
  if (bubble_)
    return;

  DCHECK(tray_container());

  TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  init_params.parent_window = GetBubbleWindowContainer();
  init_params.anchor_view = GetBubbleAnchor();
  init_params.shelf_alignment = shelf()->alignment();
  init_params.preferred_width = kHoldingSpaceWidth;
  init_params.close_on_deactivate = true;
  init_params.show_by_click = show_by_click;
  init_params.has_shadow = false;

  // Create and customize bubble view.
  TrayBubbleView* bubble_view = new TrayBubbleView(init_params);
  bubble_view->set_anchor_view_insets(GetBubbleAnchorInsets());

  // Add pinned files container.
  pinned_files_container_ =
      bubble_view->AddChildView(std::make_unique<PinnedFilesContainer>());
  SetupChildLayer(pinned_files_container_);

  // Separator between the two containers, gives illusion of 2 separate bubbles.
  auto* separator =
      bubble_view->AddChildView(std::make_unique<views::Separator>());
  separator->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(kHoldingSpaceContainerSeparation, 0, 0, 0)));

  recent_files_container_ =
      bubble_view->AddChildView(std::make_unique<RecentFilesContainer>());
  SetupChildLayer(recent_files_container_);

  // Show the bubble.
  bubble_ = std::make_unique<TrayBubbleWrapper>(this, bubble_view,
                                                false /* is_persistent */);

  // Set bubble frame to be invisible.
  bubble_->GetBubbleWidget()->non_client_view()->frame_view()->SetVisible(
      false);

  SetIsActive(true);
}

TrayBubbleView* HoldingSpaceTray::GetBubbleView() {
  return bubble_ ? bubble_->bubble_view() : nullptr;
}

const char* HoldingSpaceTray::GetClassName() const {
  return "HoldingSpaceTray";
}

}  // namespace ash

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
#include "ash/system/holding_space/holding_space_tray_bubble.h"
#include "ash/system/tray/tray_container.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"

namespace ash {

namespace {
// Padding for tray icon (dp; the button that shows the palette menu).
constexpr int kTrayIconMainAxisInset = 6;
}  // namespace

HoldingSpaceTray::HoldingSpaceTray(Shelf* shelf) : TrayBackgroundView(shelf) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  icon_ = tray_container()->AddChildView(std::make_unique<views::ImageView>());
  icon_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_SCREENSHOTS_TITLE));

  icon_->SetImage(CreateVectorIcon(kHoldingSpaceIcon,
                                   ShelfConfig::Get()->shelf_icon_color()));

  tray_container()->SetMargin(kTrayIconMainAxisInset, 0);
}

HoldingSpaceTray::~HoldingSpaceTray() = default;

void HoldingSpaceTray::ClickedOutsideBubble() {
  CloseBubble();
}

base::string16 HoldingSpaceTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_SCREENSHOTS_TITLE);
}

void HoldingSpaceTray::HandleLocaleChange() {
  icon_->SetTooltipText(
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
    bubble_->AnchorUpdated();
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
  SetVisiblePreferred(shelf()->GetStatusAreaWidget()->login_status() ==
                      LoginStatus::USER);
}

void HoldingSpaceTray::CloseBubble() {
  bubble_.reset();
  SetIsActive(false);
}

void HoldingSpaceTray::ShowBubble(bool show_by_click) {
  if (bubble_)
    return;

  DCHECK(tray_container());

  bubble_ = std::make_unique<HoldingSpaceTrayBubble>(this, show_by_click);

  SetIsActive(true);
}

const char* HoldingSpaceTray::GetClassName() const {
  return "HoldingSpaceTray";
}

}  // namespace ash

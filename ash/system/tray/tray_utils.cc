// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_utils.h"

#include <string>

#include "ash/bubble/bubble_constants.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/work_area_insets.h"
#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/label.h"

namespace ash {

void SetupLabelForTray(views::Label* label) {
  // The text is drawn on an transparent bg, so we must disable subpixel
  // rendering.
  label->SetSubpixelRenderingEnabled(false);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetFontList(gfx::FontList().Derive(
      kTrayTextFontSizeIncrease, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
}

void SetupConnectedScrollListItem(HoverHighlightView* view) {
  SetupConnectedScrollListItem(view, absl::nullopt /* battery_percentage */);
}

void SetupConnectedScrollListItem(HoverHighlightView* view,
                                  absl::optional<uint8_t> battery_percentage) {
  DCHECK(view->is_populated());

  std::u16string status;

  if (battery_percentage) {
    view->SetSubText(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_CONNECTED_WITH_BATTERY_LABEL,
        base::NumberToString16(battery_percentage.value())));
  } else {
    view->SetSubText(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED));
  }

  view->sub_text_label()->SetAutoColorReadabilityEnabled(false);
  view->sub_text_label()->SetEnabledColorId(kColorAshTextColorPositive);
}

void SetupConnectingScrollListItem(HoverHighlightView* view) {
  DCHECK(view->is_populated());

  view->SetSubText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTING));
}

void SetWarningSubText(HoverHighlightView* view, std::u16string subtext) {
  DCHECK(view->is_populated());

  view->SetSubText(subtext);
  view->sub_text_label()->SetAutoColorReadabilityEnabled(false);
  view->sub_text_label()->SetEnabledColorId(kColorAshTextColorWarning);
}

gfx::Insets GetTrayBubbleInsets() {
  // Decrease bottom and right insets to compensate for the adjustment of
  // the respective edges in Shelf::GetSystemTrayAnchorRect().
  gfx::Insets insets = gfx::Insets::TLBR(
      kBubbleMenuPadding, kBubbleMenuPadding, kBubbleMenuPadding - 1,
      kBubbleMenuPadding - (base::i18n::IsRTL() ? 0 : 1));

  // The work area in tablet mode always uses the in-app shelf height, which is
  // shorter than the standard shelf height. In this state, we need to add back
  // the difference to compensate (see crbug.com/1033302).
  bool in_tablet_mode = Shell::Get()->tablet_mode_controller() &&
                        Shell::Get()->tablet_mode_controller()->InTabletMode();
  if (!in_tablet_mode)
    return insets;

  Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
  bool is_bottom_alignment =
      shelf->alignment() == ShelfAlignment::kBottom ||
      shelf->alignment() == ShelfAlignment::kBottomLocked;

  if (!is_bottom_alignment)
    return insets;

  int height_compensation = GetBubbleInsetHotseatCompensation();
  insets.set_bottom(insets.bottom() + height_compensation);
  return insets;
}

int GetBubbleInsetHotseatCompensation() {
  int height_compensation = kTrayBubbleInsetHotseatCompensation;
  Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());

  switch (shelf->GetBackgroundType()) {
    case ShelfBackgroundType::kInApp:
    case ShelfBackgroundType::kOverview:
      // Certain modes do not require a height compensation.
      height_compensation = 0;
      break;
    case ShelfBackgroundType::kLogin:
      // The hotseat is not visible on the lock screen, so we need a smaller
      // height compensation.
      height_compensation = kTrayBubbleInsetTabletModeCompensation;
      break;
    default:
      break;
  }
  return height_compensation;
}

gfx::Insets GetSecondaryBubbleInsets() {
  Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
  gfx::Insets insets;

  switch (shelf->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      insets.set_bottom(kBubbleMenuPadding);
      break;
    case ShelfAlignment::kLeft:
      insets.set_left(kBubbleMenuPadding);
      break;
    case ShelfAlignment::kRight:
      insets.set_right(kBubbleMenuPadding);
      break;
  }
  return insets;
}

gfx::Insets GetInkDropInsets(TrayPopupInkDropStyle ink_drop_style) {
  if (ink_drop_style == TrayPopupInkDropStyle::HOST_CENTERED ||
      ink_drop_style == TrayPopupInkDropStyle::INSET_BOUNDS) {
    return gfx::Insets(kTrayPopupInkDropInset);
  }
  return gfx::Insets();
}

int CalculateMaxTrayBubbleHeight() {
  Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());

  // We calculate the available height from the top of the screen to the top of
  // the bubble's anchor rect. We can not use the bottom of the screen since the
  // anchor's position is not always exactly at the bottom of the screen.
  int anchor_rect_top = shelf->GetSystemTrayAnchorRect().y();
  WorkAreaInsets* work_area =
      WorkAreaInsets::ForWindow(shelf->GetWindow()->GetRootWindow());
  int free_space_height_above_anchor =
      anchor_rect_top - work_area->user_work_area_bounds().y();
  return free_space_height_above_anchor - kBubbleMenuPadding * 2;
}

}  // namespace ash

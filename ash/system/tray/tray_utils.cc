// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_utils.h"

#include <string>

#include "ash/bubble/bubble_constants.h"
#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/work_area_insets.h"
#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/display/screen.h"
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
  SetupConnectedScrollListItem(view, std::nullopt /* battery_percentage */);
}

void SetupConnectedScrollListItem(HoverHighlightView* view,
                                  std::optional<uint8_t> battery_percentage) {
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

  view->sub_text_label()->SetEnabledColorId(cros_tokens::kCrosSysPositive);
  ash::TypographyProvider::Get()->StyleLabel(
      ash::TypographyToken::kCrosAnnotation1, *view->sub_text_label());
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
  view->sub_text_label()->SetEnabledColorId(cros_tokens::kCrosSysWarning);
  ash::TypographyProvider::Get()->StyleLabel(
      ash::TypographyToken::kCrosAnnotation1, *view->sub_text_label());
}

gfx::Insets GetTrayBubbleInsets(aura::Window* window) {
  // Decrease bottom and side insets by `kShelfDisplayOffset` to compensate for
  // the adjustment of the respective edges in Shelf::GetSystemTrayAnchorRect().
  gfx::Insets insets = gfx::Insets::TLBR(
      kBubbleMenuPadding, kBubbleMenuPadding,
      kBubbleMenuPadding - kShelfDisplayOffset,
      kBubbleMenuPadding - (base::i18n::IsRTL() ? 0 : kShelfDisplayOffset));

  // The work area in tablet mode always uses the in-app shelf height, which is
  // shorter than the standard shelf height. In this state, we need to add back
  // the difference to compensate (see crbug.com/1033302).
  if (!display::Screen::GetScreen()->InTabletMode()) {
    return insets;
  }

  Shelf* shelf = Shelf::ForWindow(window);
  bool is_bottom_alignment =
      shelf->alignment() == ShelfAlignment::kBottom ||
      shelf->alignment() == ShelfAlignment::kBottomLocked;

  if (!is_bottom_alignment)
    return insets;

  int height_compensation = GetBubbleInsetHotseatCompensation(window);
  insets.set_bottom(insets.bottom() + height_compensation);
  return insets;
}

int GetBubbleInsetHotseatCompensation(aura::Window* window) {
  int height_compensation = kTrayBubbleInsetHotseatCompensation;
  Shelf* shelf = Shelf::ForWindow(window);

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

gfx::Insets GetInkDropInsets(TrayPopupInkDropStyle ink_drop_style) {
  if (ink_drop_style == TrayPopupInkDropStyle::HOST_CENTERED ||
      ink_drop_style == TrayPopupInkDropStyle::INSET_BOUNDS) {
    return gfx::Insets(kTrayPopupInkDropInset);
  }
  return gfx::Insets();
}

int CalculateMaxTrayBubbleHeight(aura::Window* window) {
  Shelf* shelf = Shelf::ForWindow(window);

  // We calculate the available height from the top of the screen to the top of
  // the bubble's anchor rect. We can not use the bottom of the screen since the
  // anchor's position is not always exactly at the bottom of the screen. If
  // we're in tablet mode then we also need to subtract out any extra padding
  // that may be present due to the hotseat.
  int anchor_rect_top = shelf->GetSystemTrayAnchorRect().y();
  WorkAreaInsets* work_area =
      WorkAreaInsets::ForWindow(shelf->GetWindow()->GetRootWindow());
  int free_space_height_above_anchor =
      anchor_rect_top - work_area->user_work_area_bounds().y();
  if (display::Screen::GetScreen()->InTabletMode()) {
    free_space_height_above_anchor -= GetBubbleInsetHotseatCompensation(window);
  }
  return free_space_height_above_anchor - kBubbleMenuPadding * 2;
}

TrayBubbleView::InitParams CreateInitParamsForTrayBubble(
    TrayBackgroundView* tray,
    bool anchor_to_shelf_corner) {
  TrayBubbleView::InitParams init_params;
  init_params.delegate = tray->GetWeakPtr();
  init_params.parent_window = tray->GetBubbleWindowContainer();
  if (anchor_to_shelf_corner) {
    init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
    init_params.anchor_rect = tray->shelf()->GetSystemTrayAnchorRect();
  } else {
    init_params.anchor_view = tray;
  }
  init_params.insets = GetTrayBubbleInsets(tray->GetBubbleWindowContainer());
  init_params.shelf_alignment = tray->shelf()->alignment();
  init_params.preferred_width = kTrayMenuWidth;
  init_params.close_on_deactivate = true;
  init_params.translucent = true;
  if (!features::IsBubbleCornerRadiusUpdateEnabled()) {
    init_params.corner_radius = kTrayItemCornerRadius;
  }
  init_params.reroute_event_handler = true;
  init_params.anchor_to_shelf_corner = anchor_to_shelf_corner;

  return init_params;
}

}  // namespace ash

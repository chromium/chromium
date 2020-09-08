// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_utils.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/label.h"

namespace ash {

void SetupLabelForTray(views::Label* label) {
  // The text is drawn on an transparent bg, so we must disable subpixel
  // rendering.
  label->SetSubpixelRenderingEnabled(false);
  label->SetFontList(gfx::FontList().Derive(
      kTrayTextFontSizeIncrease, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
}

SkColor TrayIconColor(session_manager::SessionState session_state) {
  if (session_state == session_manager::SessionState::OOBE)
    return kIconColorInOobe;
  return AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
}

gfx::Insets GetTrayBubbleInsets() {
  // Decrease bottom and right insets to compensate for the adjustment of
  // the respective edges in Shelf::GetSystemTrayAnchorRect().
  gfx::Insets insets = gfx::Insets(
      kUnifiedMenuPadding, kUnifiedMenuPadding, kUnifiedMenuPadding - 1,
      kUnifiedMenuPadding - (base::i18n::IsRTL() ? 0 : 1));

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

  if (!chromeos::switches::ShouldShowShelfHotseat())
    return insets;

  int height_compensation = kTrayBubbleInsetHotseatCompensation;
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

  insets.set_bottom(insets.bottom() + height_compensation);
  return insets;
}

gfx::Insets GetSecondaryBubbleInsets() {
  Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
  gfx::Insets insets;

  switch (shelf->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      insets.set_bottom(kUnifiedMenuPadding);
      break;
    case ShelfAlignment::kLeft:
      insets.set_left(kUnifiedMenuPadding);
      break;
    case ShelfAlignment::kRight:
      insets.set_right(kUnifiedMenuPadding);
      break;
  }
  return insets;
}

}  // namespace ash

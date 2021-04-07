// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/oobe_dialog_size_utils.h"

#include "ash/public/cpp/shelf_config.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"

namespace chromeos {

namespace {

constexpr int kDialogHeightForWidePadding = 640;
constexpr double kEightPrecent = 0.08;

}  // namespace

constexpr gfx::Size kMaxDialogSize{768, 768};
// Min height should match --oobe-dialog-min-height;
constexpr gfx::Size kMinDialogSize{464, 384};
constexpr gfx::Insets kMinMargins{48, 48};

// Sizes come from specs except min widths which are taken as maximal zoomed
// display widths of smallest device ChromeTab (960x600).
constexpr gfx::Size kMaxLandscapeDialogSize{1040, 680};
constexpr gfx::Size kMinLandscapeDialogSize{738, 540};
constexpr gfx::Size kMaxPortraitDialogSize{680, 1040};
constexpr gfx::Size kMinPortraitDialogSize{461, 820};

gfx::Size CalculateOobeDialogSizeForWebDialog(const gfx::Rect& host_bounds,
                                              int shelf_height,
                                              bool is_horizontal,
                                              bool is_new_oobe_layout_enabled) {
  if (is_new_oobe_layout_enabled) {
    return CalculateOobeDialogSize(host_bounds.size(), shelf_height,
                                   is_horizontal);
  }
  gfx::Rect available_area = host_bounds;
  available_area.Inset(0, 0, 0, shelf_height);

  // Inset minimum margin on each side of area.
  gfx::Rect area_no_margins = available_area;
  area_no_margins.Inset(kMinMargins);

  gfx::Size dialog_size = area_no_margins.size();
  dialog_size.SetToMin(kMaxDialogSize);
  dialog_size.SetToMax(kMinDialogSize);

  // Still, dialog should fit into available area.
  dialog_size.SetToMin(available_area.size());

  return dialog_size;
}

gfx::Size CalculateOobeDialogSize(const gfx::Size& host_size,
                                  int shelf_height,
                                  bool is_horizontal) {
  gfx::Size margin = ScaleToCeiledSize(host_size, kEightPrecent);
  gfx::Size margins = margin + margin;
  margins.SetToMax(kMinMargins.size());
  margins.Enlarge(0, shelf_height);
  gfx::Size result = host_size - margins;
  if (is_horizontal) {
    result.SetToMin(kMaxLandscapeDialogSize);
    result.SetToMax(kMinLandscapeDialogSize);
  } else {
    result.SetToMin(kMaxPortraitDialogSize);
    result.SetToMax(kMinPortraitDialogSize);
  }
  return result;
}

gfx::Size CalculateOobeDialogSizeForPrimrayDisplay() {
  const gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  const bool is_horizontal = display_size.width() > display_size.height();
  // ShellConfig is only non-existent in test scenarios.
  const int shelf_height = ash::ShelfConfig::Get()
                               ? ash::ShelfConfig::Get()->shelf_size()
                               : 48 /* default shelf height */;
  return CalculateOobeDialogSize(display_size, shelf_height, is_horizontal);
}

void CalculateOobeDialogBounds(const gfx::Rect& host_bounds,
                               int shelf_height,
                               bool is_horizontal,
                               bool is_new_oobe_layout_enabled,
                               gfx::Rect* result,
                               OobeDialogPaddingMode* result_padding) {
  // Area to position dialog.
  *result = host_bounds;
  result->Inset(0, 0, 0, shelf_height);

  // Center dialog within an available area.
  result->ClampToCenteredSize(CalculateOobeDialogSizeForWebDialog(
      host_bounds, shelf_height, is_horizontal, is_new_oobe_layout_enabled));
  if (!result_padding)
    return;
  if ((result->width() >= kMaxDialogSize.width()) &&
      (result->height() >= kDialogHeightForWidePadding)) {
    *result_padding = OobeDialogPaddingMode::PADDING_WIDE;
  } else {
    *result_padding = OobeDialogPaddingMode::PADDING_NARROW;
  }
}

}  // namespace chromeos

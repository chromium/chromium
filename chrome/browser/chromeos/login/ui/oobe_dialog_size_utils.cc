// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/oobe_dialog_size_utils.h"
#include "ui/gfx/geometry/insets.h"

namespace chromeos {

namespace {

constexpr gfx::Size kMaxDialogSize{768, 768};
constexpr int kDialogHeightForWidePadding = 640;
constexpr gfx::Size kMinDialogSize{464, 464};
constexpr gfx::Insets kMinMargins{48, 48};

}  // namespace

void CalculateOobeDialogBounds(const gfx::Rect& host_bounds,
                               int shelf_height,
                               gfx::Rect* result,
                               OobeDialogPaddingMode* result_padding) {
  // Area to position dialog.
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

  // Center dialog within an available area.
  *result = available_area;
  result->ClampToCenteredSize(dialog_size);
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

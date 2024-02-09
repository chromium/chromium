// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/collapse_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/scoped_canvas.h"

namespace ash {

CollapseButton::CollapseButton(PressedCallback callback)
    : IconButton(std::move(callback),
                 IconButton::Type::kMediumFloating,
                 &kUnifiedMenuExpandIcon,
                 IDS_ASH_STATUS_TRAY_COLLAPSE) {}

CollapseButton::~CollapseButton() = default;

void CollapseButton::SetExpandedAmount(double expanded_amount) {
  expanded_amount_ = expanded_amount;
  if (expanded_amount == 0.0 || expanded_amount == 1.0) {
    SetTooltipText(l10n_util::GetStringUTF16(expanded_amount == 1.0
                                                 ? IDS_ASH_STATUS_TRAY_COLLAPSE
                                                 : IDS_ASH_STATUS_TRAY_EXPAND));
  }
  SchedulePaint();
}

void CollapseButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::ScopedCanvas scoped(canvas);
  canvas->Translate(gfx::Vector2d(size().width() / 2, size().height() / 2));
  canvas->sk_canvas()->rotate(expanded_amount_ * 180.);
  gfx::ImageSkia image = GetImageToPaint();
  canvas->DrawImageInt(image, -image.width() / 2, -image.height() / 2);
}

BEGIN_METADATA(CollapseButton)
END_METADATA

}  // namespace ash

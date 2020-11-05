// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/top_shortcut_button.h"

#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_class_properties.h"

namespace ash {

TopShortcutButton::TopShortcutButton(PressedCallback callback,
                                     const gfx::VectorIcon& icon,
                                     int accessible_name_id)
    : views::ImageButton(std::move(callback)), icon_(icon) {
  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  if (accessible_name_id)
    SetTooltipText(l10n_util::GetStringUTF16(accessible_name_id));
  TrayPopupUtils::ConfigureTrayPopupButton(this);
  views::InstallCircleHighlightPathGenerator(this);
}

TopShortcutButton::~TopShortcutButton() = default;

gfx::Size TopShortcutButton::CalculatePreferredSize() const {
  return gfx::Size(kTrayItemSize, kTrayItemSize);
}

void TopShortcutButton::PaintButtonContents(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawPath(views::GetHighlightPath(this), flags);

  views::ImageButton::PaintButtonContents(canvas);
}

std::unique_ptr<views::InkDrop> TopShortcutButton::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(this);
}

std::unique_ptr<views::InkDropRipple> TopShortcutButton::CreateInkDropRipple()
    const {
  return TrayPopupUtils::CreateInkDropRipple(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      GetInkDropCenterBasedOnLastEvent());
}

std::unique_ptr<views::InkDropHighlight>
TopShortcutButton::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(this);
}

const char* TopShortcutButton::GetClassName() const {
  return "TopShortcutButton";
}

void TopShortcutButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();
  auto* color_provider = AshColorProvider::Get();
  color_provider->DecorateIconButton(this, icon_,
                                     /*toggled_=*/false,
                                     kTrayTopShortcutButtonIconSize);
  focus_ring()->SetColor(color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
  SchedulePaint();
}

}  // namespace ash

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/top_shortcut_button.h"

#include "ash/style/ash_color_provider.h"
#include "ash/style/default_color_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/unified_system_tray_view.h"
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

TopShortcutButton::TopShortcutButton(const gfx::VectorIcon& icon,
                                     int accessible_name_id)
    : TopShortcutButton(nullptr /* listener */, accessible_name_id) {
  SetImage(views::Button::STATE_DISABLED,
           gfx::CreateVectorIcon(
               icon, kTrayTopShortcutButtonIconSize,
               AshColorProvider::Get()->GetContentLayerColor(
                   AshColorProvider::ContentLayerType::kIconPrimary,
                   AshColorProvider::AshColorMode::kDark)));
  SetEnabled(false);
}

TopShortcutButton::TopShortcutButton(views::ButtonListener* listener,
                                     const gfx::VectorIcon& icon,
                                     int accessible_name_id)
    : TopShortcutButton(listener, accessible_name_id) {
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconPrimary,
      AshColorProvider::AshColorMode::kDark);
  SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(icon, kTrayTopShortcutButtonIconSize, icon_color));
  SetImage(
      views::Button::STATE_DISABLED,
      gfx::CreateVectorIcon(icon, kTrayTopShortcutButtonIconSize,
                            AshColorProvider::GetDisabledColor(icon_color)));
}

TopShortcutButton::TopShortcutButton(views::ButtonListener* listener,
                                     int accessible_name_id)
    : views::ImageButton(listener) {
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
  flags.setColor(AshColorProvider::Get()->DeprecatedGetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kInactiveControlBackground,
      kUnifiedMenuButtonColor));
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
      GetInkDropCenterBasedOnLastEvent(),
      UnifiedSystemTrayView::GetBackgroundColor());
}

std::unique_ptr<views::InkDropHighlight>
TopShortcutButton::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      UnifiedSystemTrayView::GetBackgroundColor());
}

const char* TopShortcutButton::GetClassName() const {
  return "TopShortcutButton";
}

}  // namespace ash

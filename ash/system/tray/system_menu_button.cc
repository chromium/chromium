// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_menu_button.h"

#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_ink_drop_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/focus_ring.h"

namespace ash {

SystemMenuButton::SystemMenuButton(PressedCallback callback,
                                   const gfx::ImageSkia& normal_icon,
                                   const gfx::ImageSkia& disabled_icon,
                                   int accessible_name_id)
    : views::ImageButton(std::move(callback)) {
  DCHECK_EQ(normal_icon.width(), disabled_icon.width());
  DCHECK_EQ(normal_icon.height(), disabled_icon.height());

  SetImage(STATE_NORMAL, normal_icon);
  SetImage(STATE_DISABLED, disabled_icon);

  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  SetPreferredSize(gfx::Size(kMenuButtonSize, kMenuButtonSize));

  SetTooltipText(l10n_util::GetStringUTF16(accessible_name_id));

  TrayPopupUtils::ConfigureTrayPopupButton(
      this, TrayPopupInkDropStyle::HOST_CENTERED);
  TrayPopupUtils::InstallHighlightPathGenerator(
      this, TrayPopupInkDropStyle::HOST_CENTERED);
  views::FocusRing::Get(this)->SetColor(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kFocusRingColor));
}

SystemMenuButton::SystemMenuButton(PressedCallback callback,
                                   const gfx::VectorIcon& icon,
                                   int accessible_name_id)
    : SystemMenuButton(std::move(callback),
                       gfx::ImageSkia(),
                       gfx::ImageSkia(),
                       accessible_name_id) {
  SetVectorIcon(icon);
}

void SystemMenuButton::SetVectorIcon(const gfx::VectorIcon& icon) {
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(icon, icon_color));
  SetImage(views::Button::STATE_DISABLED,
           gfx::CreateVectorIcon(
               icon, AshColorProvider::GetDisabledColor(icon_color)));
}

SystemMenuButton::~SystemMenuButton() = default;

const char* SystemMenuButton::GetClassName() const {
  return "SystemMenuButton";
}

}  // namespace ash

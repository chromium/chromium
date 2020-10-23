// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/floating_menu_button.h"

#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {
const SkColor kFloatingMenuButtonIconColorActive = SkColorSetRGB(32, 33, 36);
}

FloatingMenuButton::FloatingMenuButton(views::ButtonListener* listener,
                                       const gfx::VectorIcon& icon,
                                       int accessible_name_id,
                                       bool flip_for_rtl)
    : FloatingMenuButton(listener,
                         icon,
                         accessible_name_id,
                         flip_for_rtl,
                         /*size=*/kTrayItemSize,
                         /*draw_highlight=*/true,
                         /*is_a11y_togglable=*/true) {}

FloatingMenuButton::FloatingMenuButton(views::ButtonListener* listener,
                                       const gfx::VectorIcon& icon,
                                       int accessible_name_id,
                                       bool flip_for_rtl,
                                       int size,
                                       bool draw_highlight,
                                       bool is_a11y_togglable)
    : views::ImageButton(listener),
      icon_(&icon),
      size_(size),
      draw_highlight_(draw_highlight),
      is_a11y_togglable_(is_a11y_togglable) {
  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  SetFlipCanvasOnPaintForRTLUI(flip_for_rtl);
  SetPreferredSize(gfx::Size(size_, size_));
  TrayPopupUtils::ConfigureTrayPopupButton(this);
  views::InstallCircleHighlightPathGenerator(this);
  focus_ring()->SetColor(AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
  SetTooltipText(l10n_util::GetStringUTF16(accessible_name_id));
  UpdateImage();
}

FloatingMenuButton::~FloatingMenuButton() = default;

void FloatingMenuButton::SetVectorIcon(const gfx::VectorIcon& icon) {
  icon_ = &icon;
  UpdateImage();
}

void FloatingMenuButton::SetToggled(bool toggled) {
  toggled_ = toggled;
  UpdateImage();
  SchedulePaint();
}

void FloatingMenuButton::PaintButtonContents(gfx::Canvas* canvas) {
  if (draw_highlight_) {
    gfx::Rect rect(GetContentsBounds());
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
        toggled_
            ? AshColorProvider::ControlsLayerType::kControlBackgroundColorActive
            : AshColorProvider::ControlsLayerType::
                  kControlBackgroundColorInactive));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawCircle(gfx::PointF(rect.CenterPoint()), size_ / 2, flags);
  }

  views::ImageButton::PaintButtonContents(canvas);
}

gfx::Size FloatingMenuButton::CalculatePreferredSize() const {
  return gfx::Size(size_, size_);
}

void FloatingMenuButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (!GetEnabled())
    return;
  views::ImageButton::GetAccessibleNodeData(node_data);
  if (!is_a11y_togglable_)
    return;
  node_data->role = ax::mojom::Role::kToggleButton;
  node_data->SetCheckedState(toggled_ ? ax::mojom::CheckedState::kTrue
                                      : ax::mojom::CheckedState::kFalse);
}

const char* FloatingMenuButton::GetClassName() const {
  return "FloatingMenuButton";
}

std::unique_ptr<views::InkDrop> FloatingMenuButton::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(this);
}

std::unique_ptr<views::InkDropRipple> FloatingMenuButton::CreateInkDropRipple()
    const {
  return TrayPopupUtils::CreateInkDropRipple(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      GetInkDropCenterBasedOnLastEvent());
}

std::unique_ptr<views::InkDropHighlight>
FloatingMenuButton::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(this);
}

void FloatingMenuButton::SetId(int id) {
  views::View::SetID(id);
}

void FloatingMenuButton::UpdateImage() {
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
  SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(
          *icon_, toggled_ ? kFloatingMenuButtonIconColorActive : icon_color));
  SetImage(
      views::Button::STATE_DISABLED,
      gfx::CreateVectorIcon(
          *icon_, toggled_ ? kFloatingMenuButtonIconColorActive : icon_color));
}

}  // namespace ash

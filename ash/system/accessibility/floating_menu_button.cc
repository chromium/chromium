// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/floating_menu_button.h"

#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop_mask.h"

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
    : TopShortcutButton(listener, accessible_name_id),
      icon_(&icon),
      size_(size),
      draw_highlight_(draw_highlight),
      is_a11y_togglable_(is_a11y_togglable) {
  EnableCanvasFlippingForRTLUI(flip_for_rtl);
  SetPreferredSize(gfx::Size(size_, size_));
  UpdateImage();
}

FloatingMenuButton::~FloatingMenuButton() = default;

// views::Button:
const char* FloatingMenuButton::GetClassName() const {
  return "FloatingMenuButton";
}

// Set the vector icon shown in a circle.
void FloatingMenuButton::SetVectorIcon(const gfx::VectorIcon& icon) {
  icon_ = &icon;
  UpdateImage();
}

// Change the toggle state.
void FloatingMenuButton::SetToggled(bool toggled) {
  toggled_ = toggled;
  UpdateImage();
  SchedulePaint();
}

// TopShortcutButton:
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
  TopShortcutButton::GetAccessibleNodeData(node_data);
  if (!is_a11y_togglable_)
    return;
  node_data->role = ax::mojom::Role::kToggleButton;
  node_data->SetCheckedState(toggled_ ? ax::mojom::CheckedState::kTrue
                                      : ax::mojom::CheckedState::kFalse);
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

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_toggle_button.h"

#include "ash/capture_mode/capture_mode_button.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

CaptureModeToggleButton::CaptureModeToggleButton(
    views::Button::PressedCallback callback,
    const gfx::VectorIcon& icon,
    ui::ColorId toggled_background_color_id)
    : views::ToggleImageButton(callback),
      toggled_background_color_id_(toggled_background_color_id) {
  CaptureModeButton::ConfigureButton(this, views::FocusRing::Get(this));
  SetIcon(icon);
}

void CaptureModeToggleButton::OnPaintBackground(gfx::Canvas* canvas) {
  if (!GetToggled())
    return;

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(GetColorProvider()->GetColor(toggled_background_color_id_));
  const gfx::RectF bounds(GetContentsBounds());
  canvas->DrawCircle(bounds.CenterPoint(), bounds.width() / 2, flags);
}

void CaptureModeToggleButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ImageButton::GetAccessibleNodeData(node_data);
  const std::u16string tooltip = GetTooltipText(gfx::Point());
  DCHECK(!tooltip.empty());
  node_data->role = ax::mojom::Role::kToggleButton;
  node_data->SetName(tooltip);
  node_data->SetCheckedState(GetToggled() ? ax::mojom::CheckedState::kTrue
                                          : ax::mojom::CheckedState::kFalse);
}

views::View* CaptureModeToggleButton::GetView() {
  return this;
}

void CaptureModeToggleButton::SetIcon(const gfx::VectorIcon& icon) {
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(icon, kColorAshButtonIconColor));
  SetImageModel(
      views::Button::STATE_DISABLED,
      ui::ImageModel::FromVectorIcon(icon, kColorAshButtonIconDisabledColor));
  // Note that a disabled button cannot be toggled, so we don't need to set a
  // toggled icon for the disabled state.
  SetToggledImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(icon, kColorAshButtonIconColorPrimary));
}

BEGIN_METADATA(CaptureModeToggleButton, views::ToggleImageButton)
END_METADATA

}  // namespace ash

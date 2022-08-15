// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_toggle_button.h"

#include "ash/capture_mode/capture_mode_button.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/style/ash_color_provider.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

CaptureModeToggleButton::CaptureModeToggleButton(
    views::Button::PressedCallback callback,
    const gfx::VectorIcon& icon)
    : views::ToggleImageButton(callback) {
  CaptureModeButton::ConfigureButton(this, views::FocusRing::Get(this));

  SetIcon(icon);
  toggled_background_color_ = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorActive);
}

void CaptureModeToggleButton::OnPaintBackground(gfx::Canvas* canvas) {
  if (!GetToggled())
    return;

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(toggled_background_color_);
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
  auto* color_provider = AshColorProvider::Get();
  const SkColor normal_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  const SkColor toggled_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColorPrimary);

  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(icon, normal_color));
  SetImage(views::Button::STATE_DISABLED,
           gfx::CreateVectorIcon(
               icon, color_provider->GetDisabledColor(normal_color)));
  // Note that a disabled button cannot be toggled, so we don't need to set a
  // toggled icon for the disabled state.
  const auto toggled_icon = gfx::CreateVectorIcon(icon, toggled_color);
  SetToggledImage(views::Button::STATE_NORMAL, &toggled_icon);
}

BEGIN_METADATA(CaptureModeToggleButton, views::ToggleImageButton)
END_METADATA

}  // namespace ash

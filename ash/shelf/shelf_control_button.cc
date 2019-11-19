// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_control_button.h"

#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class ShelfControlButtonHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  ShelfControlButtonHighlightPathGenerator() = default;

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    const int border_radius = ShelfConfig::Get()->control_border_radius();
    // Some control buttons have a slightly larger size to fill the shelf and
    // maximize the click target, but we still want their "visual" size to be
    // the same, so we find the center point and draw a square around that.
    const gfx::Point center = view->GetLocalBounds().CenterPoint();
    const int half_size = ShelfConfig::Get()->control_size() / 2;
    const gfx::Rect visual_size(center.x() - half_size, center.y() - half_size,
                                ShelfConfig::Get()->control_size(),
                                ShelfConfig::Get()->control_size());
    return SkPath().addRoundRect(gfx::RectToSkRect(visual_size), border_radius,
                                 border_radius);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfControlButtonHighlightPathGenerator);
};

}  // namespace

ShelfControlButton::ShelfControlButton(
    Shelf* shelf,
    ShelfButtonDelegate* shelf_button_delegate)
    : ShelfButton(shelf, shelf_button_delegate) {
  set_has_ink_drop_action_on_click(true);
  SetInstallFocusRingOnFocus(true);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<ShelfControlButtonHighlightPathGenerator>());
  focus_ring()->SetColor(ShelfConfig::Get()->shelf_focus_border_color());
  SetFocusPainter(nullptr);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

ShelfControlButton::~ShelfControlButton() = default;

gfx::Point ShelfControlButton::GetCenterPoint() const {
  return GetLocalBounds().CenterPoint();
}

std::unique_ptr<views::InkDropRipple> ShelfControlButton::CreateInkDropRipple()
    const {
  const int button_radius = ShelfConfig::Get()->control_border_radius();
  gfx::Point center = GetCenterPoint();
  gfx::Rect bounds(center.x() - button_radius, center.y() - button_radius,
                   2 * button_radius, 2 * button_radius);
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetLocalBounds().InsetsFrom(bounds),
      GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
      ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDropMask> ShelfControlButton::CreateInkDropMask()
    const {
  return std::make_unique<views::CircleInkDropMask>(
      size(), GetCenterPoint(), ShelfConfig::Get()->control_border_radius());
}

const char* ShelfControlButton::GetClassName() const {
  return "ash/ShelfControlButton";
}

gfx::Size ShelfControlButton::CalculatePreferredSize() const {
  return gfx::Size(ShelfConfig::Get()->control_size(),
                   ShelfConfig::Get()->control_size());
}

void ShelfControlButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ShelfButton::GetAccessibleNodeData(node_data);
  node_data->SetName(GetAccessibleName());
}

void ShelfControlButton::PaintButtonContents(gfx::Canvas* canvas) {
  PaintBackground(canvas, GetContentsBounds());
}

void ShelfControlButton::PaintBackground(gfx::Canvas* canvas,
                                         const gfx::Rect& bounds) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(
      ShelfConfig::Get()->shelf_control_permanent_highlight_background());
  canvas->DrawRoundRect(bounds, ShelfConfig::Get()->control_border_radius(),
                        flags);
}

}  // namespace ash

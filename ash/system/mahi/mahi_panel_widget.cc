// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_widget.h"

#include <cstdint>
#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/shell.h"
#include "ui/aura/window.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kPanelCornerRadius = 16;
constexpr int kPanelDefaultWidth = 340;
constexpr int kPanelDefaultHeight = 450;
constexpr int kPanelBoundsPadding = 8;

gfx::Rect CalculateWidgetBounds(aura::Window* root_window) {
  auto display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window);
  auto bottom_right = display.work_area().bottom_right();

  // The panel is positioned at the bottom right corner of the screen.
  // TODO(b/319476980): Make sure Mahi main panel bounds work when shelf
  // alignment changes.
  return gfx::Rect(bottom_right.x() - kPanelDefaultWidth - kPanelBoundsPadding,
                   bottom_right.y() - kPanelDefaultHeight - kPanelBoundsPadding,
                   kPanelDefaultWidth, kPanelDefaultHeight);
}

}  // namespace

// static
views::UniqueWidgetPtr MahiPanelWidget::CreatePanelWidget(int64_t display_id) {
  auto* root_window = Shell::GetRootWindowForDisplayId(display_id);

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "MahiPanel";
  // TODO(b/319467834): Decide what container this widget should be on.
  params.parent = Shell::GetContainer(root_window, kShellWindowId_PipContainer);

  // The widget's view handles round corners and blur via layers.
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.layer_type = ui::LAYER_NOT_DRAWN;

  views::UniqueWidgetPtr widget =
      std::make_unique<views::Widget>(std::move(params));

  // TODO(b/319329821): Finish creating main panel layout.
  auto view = std::make_unique<views::BoxLayoutView>();
  view->SetBackground(views::CreateRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated, kPanelCornerRadius));

  // Create a layer for the view for background blur and rounded corners.
  view->SetPaintToLayer();
  view->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF{kPanelCornerRadius});
  view->layer()->SetFillsBoundsOpaquely(false);
  view->layer()->SetIsFastRoundedCorner(true);
  view->layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  view->layer()->SetBackdropFilterQuality(
      ColorProvider::kBackgroundBlurQuality);
  view->SetBorder(std::make_unique<views::HighlightBorder>(
      kPanelCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow,
      /*insets_type=*/views::HighlightBorder::InsetsType::kHalfInsets));

  auto label = std::make_unique<views::Label>(u"Mahi Panel");
  view->AddChildView(std::move(label));

  widget->SetContentsView(std::move(view));
  widget->SetBounds(CalculateWidgetBounds(root_window));
  return widget;
}

}  // namespace ash

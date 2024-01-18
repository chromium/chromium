// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_widget.h"

#include <cstdint>
#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/mahi/mahi_panel_view.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

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

  widget->SetContentsView(std::make_unique<MahiPanelView>());
  widget->SetBounds(CalculateWidgetBounds(root_window));
  return widget;
}

}  // namespace ash

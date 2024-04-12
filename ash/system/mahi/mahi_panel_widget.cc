// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_widget.h"

#include <cstdint>
#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_panel_view.h"
#include "ash/system/mahi/refresh_banner_view.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kPanelHeightWithRefreshBanner = 524;
constexpr int kPanelBoundsPadding = 8;

constexpr char kWidgetName[] = "MahiPanel";

gfx::Rect CalculateWidgetBounds(aura::Window* root_window,
                                bool refresh_banner_shown = false) {
  auto display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window);
  auto bottom_right = display.work_area().bottom_right();

  // TODO(b/319731776): Use panel bounds here instead of `kPanelDefaultHeight`
  // when the panel is resizable.
  int height = refresh_banner_shown ? kPanelHeightWithRefreshBanner
                                    : mahi_constants::kPanelDefaultHeight;

  // The panel is positioned at the bottom right corner of the screen.
  // TODO(b/319476980): Make sure Mahi main panel bounds work when shelf
  // alignment changes.
  // TODO(b/319731776): Use panel bounds here instead of `kPanelDefaultWidth`
  // when the panel is resizable.
  return gfx::Rect(bottom_right.x() - mahi_constants::kPanelDefaultWidth -
                       kPanelBoundsPadding,
                   bottom_right.y() - height - kPanelBoundsPadding,
                   mahi_constants::kPanelDefaultWidth, height);
}

}  // namespace

MahiPanelWidget::MahiPanelWidget(InitParams params)
    : views::Widget(std::move(params)) {
  auto* contents_view = SetContentsView(
      views::Builder<views::BoxLayoutView>()
          // We need to set a negative value for between child spacing here
          // because we need the `RefreshBannerView` to overlap with the
          // `MahiPanelView`.
          .SetBetweenChildSpacing(-mahi_constants::kRefreshBannerStackDepth)
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .Build());

  refresh_view_ = contents_view->AddChildView(
      std::make_unique<RefreshBannerView>(&ui_controller_));
  refresh_view_observation_.Observe(refresh_view_);

  auto* panel_view = contents_view->AddChildView(
      std::make_unique<MahiPanelView>(&ui_controller_));

  // Make sure the `MahiPanelView` is sized to fill up the available space.
  contents_view->SetFlexForView(panel_view, 1.0);
}

MahiPanelWidget::~MahiPanelWidget() = default;

// static
views::UniqueWidgetPtr MahiPanelWidget::CreatePanelWidget(int64_t display_id) {
  auto* root_window = Shell::GetRootWindowForDisplayId(display_id);

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = GetName();
  // TODO(b/319467834): Decide what container this widget should be on.
  params.parent = Shell::GetContainer(root_window, kShellWindowId_PipContainer);

  // The widget's view handles round corners and blur via layers.
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.layer_type = ui::LAYER_NOT_DRAWN;

  views::UniqueWidgetPtr widget =
      std::make_unique<MahiPanelWidget>(std::move(params));

  widget->SetBounds(CalculateWidgetBounds(root_window));
  return widget;
}

// static
const char* MahiPanelWidget::GetName() {
  return kWidgetName;
}

void MahiPanelWidget::NotifyRefreshAvailabilityChanged(bool available) {
  ui_controller_.NotifyRefreshAvailabilityChanged(available);
}

void MahiPanelWidget::SendQuestion(const std::u16string& question,
                                   bool current_panel_content) {
  ui_controller_.SendQuestion(question, current_panel_content,
                              MahiUiController::QuestionSource::kMenuView);
}

void MahiPanelWidget::OnViewVisibilityChanged(views::View* observed_view,
                                              views::View* starting_view) {
  CHECK_EQ(observed_view, refresh_view_);

  SetBounds(
      CalculateWidgetBounds(GetNativeWindow(), observed_view->GetVisible()));
}

void MahiPanelWidget::OnViewIsDeleting(views::View* observed_view) {
  CHECK_EQ(observed_view, refresh_view_);

  refresh_view_observation_.Reset();
  refresh_view_ = nullptr;
}

}  // namespace ash

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
#include "ash/system/mahi/mahi_ui_controller.h"
#include "ash/system/mahi/refresh_banner_view.h"
#include "ash/wm/work_area_insets.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kRefreshBannerHeight = 32;
constexpr int kPanelBoundsPadding = 8;

constexpr char kWidgetName[] = "MahiPanel";

// TODO(b/319731776): Use panel bounds in size calculations instead of
// `kPanelDefaultWidth` and `kPanelDefaultHeight` when the panel is resizable.

bool IsSpaceAvailableOnRight(const gfx::Rect& screen_work_area,
                             const gfx::Rect& mahi_menu_bounds) {
  return mahi_menu_bounds.x() + mahi_constants::kPanelDefaultWidth <
         screen_work_area.right();
}

int CalculateAvailableSpaceOnBottom(const gfx::Rect& screen_work_area,
                                    const gfx::Rect& mahi_menu_bounds) {
  return screen_work_area.bottom() - mahi_menu_bounds.y() -
         mahi_constants::kPanelDefaultHeight - kPanelBoundsPadding;
}

gfx::Rect CalculateAnimationStartBounds(const gfx::Rect& mahi_menu_bounds) {
  const gfx::Rect screen_work_area = display::Screen::GetScreen()
                                         ->GetDisplayMatching(mahi_menu_bounds)
                                         .work_area();

  return gfx::Rect(
      IsSpaceAvailableOnRight(screen_work_area, mahi_menu_bounds)
          ? mahi_menu_bounds.x()
          : mahi_menu_bounds.right() - mahi_constants::kPanelDefaultWidth,
      CalculateAvailableSpaceOnBottom(screen_work_area, mahi_menu_bounds)
          ? mahi_menu_bounds.y()
          : mahi_menu_bounds.bottom() - mahi_menu_bounds.height(),
      mahi_constants::kPanelDefaultWidth, mahi_menu_bounds.height());
}

gfx::Rect CalculateInitialWidgetBounds(const gfx::Rect& mahi_menu_bounds) {
  const gfx::Rect screen_work_area = display::Screen::GetScreen()
                                         ->GetDisplayMatching(mahi_menu_bounds)
                                         .work_area();

  int available_space_on_bottom =
      CalculateAvailableSpaceOnBottom(screen_work_area, mahi_menu_bounds);

  // The `MahiPanelWidget` will be aligned with the top of the
  // mahi context menu when possible. Otherwise we will try to keep the top of
  // the `MahiPanelWidget` as close to the top of the mahi context menu as
  // possible. This is to provide a seamless transition between the context menu
  // and the panel.
  return gfx::Rect(
      IsSpaceAvailableOnRight(screen_work_area, mahi_menu_bounds)
          ? mahi_menu_bounds.x()
          : mahi_menu_bounds.right() - mahi_constants::kPanelDefaultWidth,
      available_space_on_bottom > 0
          ? mahi_menu_bounds.y()
          : mahi_menu_bounds.y() + available_space_on_bottom,
      mahi_constants::kPanelDefaultWidth, mahi_constants::kPanelDefaultHeight);
}

}  // namespace

MahiPanelWidget::MahiPanelWidget(InitParams params,
                                 MahiUiController* ui_controller)
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
      std::make_unique<RefreshBannerView>(ui_controller));
  refresh_view_observation_.Observe(refresh_view_);

  auto* panel_view = contents_view->AddChildView(
      std::make_unique<MahiPanelView>(ui_controller));

  // Make sure the `MahiPanelView` is sized to fill up the available space.
  contents_view->SetFlexForView(panel_view, 1.0);
  shelf_observation_.Observe(Shelf::ForWindow(Shell::GetPrimaryRootWindow()));
}

MahiPanelWidget::~MahiPanelWidget() = default;

// static
views::UniqueWidgetPtr MahiPanelWidget::CreateAndShowPanelWidget(
    int64_t display_id,
    const gfx::Rect& mahi_menu_bounds,
    MahiUiController* ui_controller) {
  auto* root_window = Shell::GetRootWindowForDisplayId(display_id);

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = GetName();
  // `SystemModalContainer` can travel across displays, is not automatically
  // resizable on limited screen size and stays on top on full-screen.
  params.parent =
      Shell::GetContainer(root_window, kShellWindowId_SystemModalContainer);

  // The widget's view handles round corners and blur via layers.
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.layer_type = ui::LAYER_NOT_DRAWN;

  views::UniqueWidgetPtr widget =
      std::make_unique<MahiPanelWidget>(std::move(params), ui_controller);
  widget->SetBounds(CalculateInitialWidgetBounds(mahi_menu_bounds));

  widget->Show();

  views::AsViewClass<MahiPanelView>(widget->GetContentsView()->GetViewByID(
                                        mahi_constants::ViewId::kMahiPanelView))
      ->AnimatePopIn(CalculateAnimationStartBounds(mahi_menu_bounds));

  return widget;
}

// static
const char* MahiPanelWidget::GetName() {
  return kWidgetName;
}

void MahiPanelWidget::OnShelfWorkAreaInsetsChanged() {
  gfx::Rect work_area_bounds =
      WorkAreaInsets::ForWindow(GetNativeWindow())->user_work_area_bounds();
  gfx::Rect panel_bounds = GetWindowBoundsInScreen();

  // If the panel widget does not fit inside of the work area bounds (e.g. due
  // to opening the virtual keyboard), its bounds will be updated so it's
  // partially visible, prioritizing the bottom part of the panel.
  if (panel_bounds.bottom() + kPanelBoundsPadding >=
      work_area_bounds.bottom()) {
    SetY(work_area_bounds.bottom() - mahi_constants::kPanelDefaultHeight -
         kPanelBoundsPadding);
  } else if (panel_bounds.y() - kPanelBoundsPadding <= work_area_bounds.y()) {
    SetY(work_area_bounds.y() + kPanelBoundsPadding);
  }
}

void MahiPanelWidget::OnViewVisibilityChanged(views::View* observed_view,
                                              views::View* starting_view) {
  CHECK_EQ(observed_view, refresh_view_);

  if (is_refresh_view_visible_ == observed_view->GetVisible()) {
    return;
  }
  is_refresh_view_visible_ = observed_view->GetVisible();

  // Update widget bounds to provide space for the refresh banner if needed.
  gfx::Rect widget_bounds = GetWindowBoundsInScreen();
  if (is_refresh_view_visible_) {
    widget_bounds.Outset(gfx::Outsets::TLBR(kRefreshBannerHeight, 0, 0, 0));
  } else {
    widget_bounds.Inset(gfx::Insets::TLBR(kRefreshBannerHeight, 0, 0, 0));
  }
  SetBounds(widget_bounds);
}

void MahiPanelWidget::OnViewIsDeleting(views::View* observed_view) {
  CHECK_EQ(observed_view, refresh_view_);

  refresh_view_observation_.Reset();
  is_refresh_view_visible_ = false;
  refresh_view_ = nullptr;
}

}  // namespace ash

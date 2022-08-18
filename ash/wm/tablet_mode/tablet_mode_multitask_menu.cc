// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "ui/aura/window.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kMultitaskMenuVerticalPadding = 4;
constexpr int kMultitaskMenuWidth = 540;
constexpr int kMultitaskMenuHeight = 124;
constexpr int kBetweenButtonSpacing = 16;

}  // namespace

// The contents view of the multitask menu.
class TabletModeMultitaskMenuView : public views::View {
 public:
  TabletModeMultitaskMenuView(aura::Window* window,
                              base::RepeatingClosure hide_menu) {
    SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
    SetUseDefaultFillLayout(true);

    auto* multitask_menu_view = AddChildView(
        std::make_unique<chromeos::MultitaskMenuView>(window, hide_menu));

    auto* layout = multitask_menu_view->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
            kBetweenButtonSpacing));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
  }

  TabletModeMultitaskMenuView(const TabletModeMultitaskMenuView&) = delete;
  TabletModeMultitaskMenuView& operator=(const TabletModeMultitaskMenuView&) =
      delete;

  ~TabletModeMultitaskMenuView() override = default;
};

TabletModeMultitaskMenu::TabletModeMultitaskMenu(aura::Window* window)
    : window_(window) {
  DCHECK(window_);
  const gfx::Rect widget_bounds(
      window_->bounds().CenterPoint().x() - kMultitaskMenuWidth / 2,
      kMultitaskMenuVerticalPadding, kMultitaskMenuWidth, kMultitaskMenuHeight);

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = window->parent();
  params.bounds = widget_bounds;
  params.name = "TabletModeMultitaskMenuWidget";

  multitask_menu_widget_->Init(std::move(params));
  multitask_menu_widget_->SetContentsView(
      std::make_unique<TabletModeMultitaskMenuView>(
          window, base::BindRepeating(&TabletModeMultitaskMenu::Hide,
                                      base::Unretained(this))));
}

TabletModeMultitaskMenu::~TabletModeMultitaskMenu() = default;

void TabletModeMultitaskMenu::Show() {
  DCHECK(multitask_menu_widget_);
  auto* multitask_menu_window = multitask_menu_widget_->GetNativeWindow();
  // TODO(sophiewen): Consider adding transient child instead.
  multitask_menu_window->parent()->StackChildAbove(multitask_menu_window,
                                                   window_);
  multitask_menu_widget_->Show();
}

void TabletModeMultitaskMenu::Hide() {
  DCHECK(multitask_menu_widget_);
  multitask_menu_widget_->Hide();
}

}  // namespace ash
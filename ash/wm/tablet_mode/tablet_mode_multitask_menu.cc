// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"

#include "ui/aura/window.h"
#include "ui/views/background.h"

namespace ash {

namespace {

constexpr int kMultitaskMenuVerticalPadding = 4;
constexpr int kMultitaskMenuWidth = 540;
constexpr int kMultitaskMenuHeight = 124;

}  // namespace

// The contents view of the multitask menu.
class TabletModeMultitaskMenuView : public views::View {
 public:
  TabletModeMultitaskMenuView() {
    // TODO(sophiewen): Placeholder.
    SetBackground(views::CreateSolidBackground(SK_ColorBLACK));
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
      std::make_unique<TabletModeMultitaskMenuView>());
}

TabletModeMultitaskMenu::~TabletModeMultitaskMenu() = default;

void TabletModeMultitaskMenu::Show() {
  DCHECK(multitask_menu_widget_);
  multitask_menu_widget_->Show();
}

void TabletModeMultitaskMenu::Hide() {
  DCHECK(multitask_menu_widget_);
  multitask_menu_widget_->Hide();
}

}  // namespace ash
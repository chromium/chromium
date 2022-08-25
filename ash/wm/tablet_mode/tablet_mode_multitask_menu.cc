// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kMultitaskMenuVerticalPadding = 4;
constexpr int kMultitaskMenuWidth = 540;
constexpr int kMultitaskMenuLandscapeHeight = 124;
constexpr int kBetweenButtonSpacing = 16;

}  // namespace

// The contents view of the multitask menu.
class TabletModeMultitaskMenuView : public views::View {
 public:
  METADATA_HEADER(TabletModeMultitaskMenuView);

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

BEGIN_METADATA(TabletModeMultitaskMenuView, View)
END_METADATA

TabletModeMultitaskMenu::TabletModeMultitaskMenu(
    TabletModeMultitaskMenuEventHandler* event_handler,
    aura::Window* window)
    : event_handler_(event_handler), window_(window) {
  // Start observing the window.
  DCHECK(window);
  observed_window_.Observe(window);

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = window->parent();
  params.bounds =
      gfx::Rect(window->bounds().CenterPoint().x() - kMultitaskMenuWidth / 2,
                kMultitaskMenuVerticalPadding, kMultitaskMenuWidth,
                kMultitaskMenuLandscapeHeight);
  params.name = "TabletModeMultitaskMenuWidget";
  // TODO(crbug.com/1355572): Set widget as activatable and hide in overview.

  multitask_menu_widget_->Init(std::move(params));
  multitask_menu_widget_->SetContentsView(
      std::make_unique<TabletModeMultitaskMenuView>(
          window_, base::BindRepeating(&TabletModeMultitaskMenu::Hide,
                                       base::Unretained(this))));
}

TabletModeMultitaskMenu::~TabletModeMultitaskMenu() = default;

void TabletModeMultitaskMenu::OnWindowDestroying(aura::Window* window) {
  DCHECK(observed_window_.IsObservingSource(window));

  observed_window_.Reset();
  window_ = nullptr;

  // Destroys `this`.
  event_handler_->CloseMultitaskMenu();
}

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
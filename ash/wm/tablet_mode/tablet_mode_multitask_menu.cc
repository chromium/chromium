// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"

#include "ash/style/ash_color_id.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kMultitaskMenuVerticalPadding = 8;
constexpr int kMultitaskMenuWidth = 510;
constexpr int kMultitaskMenuLandscapeHeight = 133;
constexpr int kBetweenButtonSpacing = 12;
constexpr int kCornerRadius = 8;
constexpr int kShadowElevation = 3;

}  // namespace

// The contents view of the multitask menu.
class TabletModeMultitaskMenuView : public views::View {
 public:
  METADATA_HEADER(TabletModeMultitaskMenuView);

  TabletModeMultitaskMenuView(aura::Window* window,
                              base::RepeatingClosure hide_menu) {
    SetBackground(views::CreateThemedRoundedRectBackground(
        kColorAshShieldAndBase80, kCornerRadius));
    SetBorder(std::make_unique<views::HighlightBorder>(
        kCornerRadius, views::HighlightBorder::Type::kHighlightBorder1,
        /*use_light_colors=*/false));

    SetUseDefaultFillLayout(true);

    multitask_menu_view_for_testing_ = AddChildView(
        std::make_unique<chromeos::MultitaskMenuView>(window, hide_menu));

    auto* layout = multitask_menu_view_for_testing_->SetLayoutManager(
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

  chromeos::MultitaskMenuView* multitask_menu_view_for_testing() {
    return multitask_menu_view_for_testing_;
  }

 private:
  raw_ptr<chromeos::MultitaskMenuView> multitask_menu_view_for_testing_ =
      nullptr;
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
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.parent = window->parent();
  params.bounds =
      gfx::Rect(window->bounds().CenterPoint().x() - kMultitaskMenuWidth / 2,
                kMultitaskMenuVerticalPadding, kMultitaskMenuWidth,
                kMultitaskMenuLandscapeHeight);
  params.name = "TabletModeMultitaskMenuWidget";
  params.corner_radius = kCornerRadius;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.shadow_elevation = kShadowElevation;

  multitask_menu_widget_->Init(std::move(params));
  multitask_menu_widget_->SetContentsView(
      std::make_unique<TabletModeMultitaskMenuView>(
          window_,
          base::BindRepeating(&TabletModeMultitaskMenu::CloseMultitaskMenu,
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

void TabletModeMultitaskMenu::CloseMultitaskMenu() {
  event_handler_->CloseMultitaskMenu();
}

chromeos::MultitaskMenuView*
TabletModeMultitaskMenu::GetMultitaskMenuViewForTesting() {
  return static_cast<TabletModeMultitaskMenuView*>(
             multitask_menu_widget_->GetContentsView())
      ->multitask_menu_view_for_testing();  // IN-TEST
}

}  // namespace ash
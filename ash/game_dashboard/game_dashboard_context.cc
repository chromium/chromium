// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_context.h"

#include <memory>

#include "ash/game_dashboard/game_dashboard_main_menu_view.h"
#include "ash/game_dashboard/game_dashboard_toolbar_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "chromeos/ui/frame/frame_header.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Number of pixels to add to the top and bottom of the main menu button so
// that it's centered within the frame header.
static const int kMainMenuButtonVerticalPaddingDp = 3;

// Toolbar padding from the border of the game window.
static const int kToolbarEdgePadding = 10;

std::unique_ptr<GameDashboardWidget> CreateTransientChildWidget(
    aura::Window* game_window,
    const std::string& widget_name,
    std::unique_ptr<views::View> view) {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  // Sets the widget as a transient child, which is actually a sibling
  // of the window. This ensures that this widget will not show up in
  // screenshots or screen recordings.
  params.parent = game_window;
  params.name = widget_name;

  auto widget = std::make_unique<GameDashboardWidget>();
  widget->Init(std::move(params));
  wm::TransientWindowManager::GetOrCreate(widget->GetNativeWindow())
      ->set_parent_controls_visibility(true);
  widget->SetContentsView(std::move(view));
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);

  return widget;
}

}  // namespace

GameDashboardContext::GameDashboardContext(aura::Window* game_window)
    : game_window_(game_window),
      toolbar_snap_location_(ToolbarSnapLocation::kTopRight) {
  DCHECK(game_window_);
  CreateAndAddMainMenuButtonWidget();
}

GameDashboardContext::~GameDashboardContext() {
  if (main_menu_widget_) {
    main_menu_widget_->CloseNow();
  }
}

void GameDashboardContext::OnWindowBoundsChanged() {
  UpdateMainMenuButtonWidgetBounds();
  MaybeUpdateToolbarWidgetBounds();
}

void GameDashboardContext::SetMainMenuButtonEnabled(bool enable) {
  DCHECK(main_menu_button_widget_);
  auto* contents_view = main_menu_button_widget_->GetContentsView();
  DCHECK(contents_view);
  contents_view->SetEnabled(enable);
}

void GameDashboardContext::ToggleMainMenu() {
  if (!main_menu_widget_) {
    auto menu_delegate = std::make_unique<GameDashboardMainMenuView>(this);
    main_menu_widget_ =
        base::WrapUnique(views::BubbleDialogDelegateView::CreateBubble(
            std::move(menu_delegate)));
    main_menu_widget_->Show();
  } else {
    main_menu_widget_.reset();
  }
}

bool GameDashboardContext::ToggleToolbar() {
  if (!toolbar_widget_) {
    toolbar_widget_ = CreateTransientChildWidget(
        game_window_, "GameDashboardToolbar",
        std::make_unique<GameDashboardToolbarView>(this));
    DCHECK_EQ(game_window_,
              wm::GetTransientParent(toolbar_widget_->GetNativeWindow()));
    MaybeUpdateToolbarWidgetBounds();
    toolbar_widget_->Show();
    return true;
  }

  toolbar_widget_.reset();
  return false;
}

void GameDashboardContext::MaybeUpdateToolbarWidgetBounds() {
  if (toolbar_widget_) {
    toolbar_widget_->SetBounds(CalculateToolbarWidgetBounds());
  }
}

bool GameDashboardContext::IsToolbarVisible() const {
  return toolbar_widget_ && toolbar_widget_->IsVisible();
}

void GameDashboardContext::CreateAndAddMainMenuButtonWidget() {
  main_menu_button_widget_ = CreateTransientChildWidget(
      game_window_, "GameDashboardButton",
      std::make_unique<PillButton>(
          base::BindRepeating(&GameDashboardContext::OnMainMenuButtonPressed,
                              weak_ptr_factory_.GetWeakPtr()),
          l10n_util::GetStringUTF16(
              IDS_ASH_GAME_DASHBOARD_MAIN_MENU_BUTTON_TITLE)));
  DCHECK_EQ(game_window_, wm::GetTransientParent(
                              main_menu_button_widget_->GetNativeWindow()));
  UpdateMainMenuButtonWidgetBounds();
  main_menu_button_widget_->Show();
}

void GameDashboardContext::UpdateMainMenuButtonWidgetBounds() {
  DCHECK(main_menu_button_widget_);
  auto preferred_size =
      main_menu_button_widget_->GetContentsView()->GetPreferredSize();
  gfx::Point origin = game_window_->GetBoundsInScreen().top_center();

  auto* frame_header = chromeos::FrameHeader::Get(
      views::Widget::GetWidgetForNativeWindow(game_window_));
  if (!frame_header) {
    VLOG(1) << "No frame header found. Not updating main menu widget bounds.";
    return;
  }
  // Position the button in the top center of the `FrameHeader`.
  origin.set_x(origin.x() - preferred_size.width() / 2);
  origin.set_y(origin.y() + kMainMenuButtonVerticalPaddingDp);
  preferred_size.set_height(frame_header->GetHeaderHeight() -
                            2 * kMainMenuButtonVerticalPaddingDp);
  main_menu_button_widget_->SetBounds(gfx::Rect(origin, preferred_size));
}

void GameDashboardContext::OnMainMenuButtonPressed() {
  // TODO(b/273640775): Add metrics to know when the main menu button was
  // physically pressed.
  ToggleMainMenu();
}

const gfx::Rect GameDashboardContext::CalculateToolbarWidgetBounds() {
  const gfx::Rect game_bounds = game_window_->GetBoundsInScreen();
  const gfx::Size preferred_size =
      toolbar_widget_->GetContentsView()->GetPreferredSize();
  auto* frame_header = chromeos::FrameHeader::Get(
      views::Widget::GetWidgetForNativeWindow(game_window_));
  const int frame_header_height =
      (frame_header && frame_header->view()->GetVisible())
          ? frame_header->GetHeaderHeight()
          : 0;
  gfx::Point origin;

  switch (toolbar_snap_location_) {
    case ToolbarSnapLocation::kTopRight:
      origin = gfx::Point(
          game_bounds.right() - kToolbarEdgePadding - preferred_size.width(),
          game_bounds.y() + kToolbarEdgePadding + frame_header_height);
      break;
    case ToolbarSnapLocation::kTopLeft:
      origin = gfx::Point(
          game_bounds.x() + kToolbarEdgePadding,
          game_bounds.y() + kToolbarEdgePadding + frame_header_height);
      break;
    case ToolbarSnapLocation::kBottomRight:
      origin = gfx::Point(
          game_bounds.right() - kToolbarEdgePadding - preferred_size.width(),
          game_bounds.bottom() - kToolbarEdgePadding - preferred_size.height());
      break;
    case ToolbarSnapLocation::kBottomLeft:
      origin = gfx::Point(
          game_bounds.x() + kToolbarEdgePadding,
          game_bounds.bottom() - kToolbarEdgePadding - preferred_size.height());
      break;
  }

  return gfx::Rect(origin, preferred_size);
}

}  // namespace ash

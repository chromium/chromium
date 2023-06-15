// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_context.h"

#include <memory>

#include "ash/game_dashboard/game_dashboard_main_menu_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "chromeos/ui/frame/frame_header.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Number of pixels to add to the top and bottom of the main menu button so
// that it's centered within the frame header.
static const int kMainMenuButtonVerticalPaddingDp = 3;

}  // namespace

GameDashboardContext::GameDashboardContext(aura::Window* game_window)
    : game_window_(game_window) {
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
}

void GameDashboardContext::ToggleMainMenu() {
  if (!main_menu_widget_) {
    auto menu_delegate = std::make_unique<GameDashboardMainMenuView>(
        main_menu_button_widget_.get(), game_window_);
    main_menu_widget_ =
        base::WrapUnique(views::BubbleDialogDelegateView::CreateBubble(
            std::move(menu_delegate)));
    main_menu_widget_->Show();
  } else {
    main_menu_widget_.reset();
  }
}

void GameDashboardContext::CreateAndAddMainMenuButtonWidget() {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  // Sets the button widget as a transient child, which is actually a sibling
  // of the window. This ensures that the button will not show up in
  // screenshots or screen recordings.
  params.parent = game_window_;
  params.name = "GameDashboardButton";

  main_menu_button_widget_ = std::make_unique<views::Widget>();
  main_menu_button_widget_->Init(std::move(params));

  auto* widget_window = main_menu_button_widget_->GetNativeWindow();
  DCHECK_EQ(game_window_, wm::GetTransientParent(widget_window));
  wm::TransientWindowManager::GetOrCreate(widget_window)
      ->set_parent_controls_visibility(true);

  main_menu_button_widget_->SetContentsView(std::make_unique<PillButton>(
      base::BindRepeating(&GameDashboardContext::OnMainMenuButtonPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_MAIN_MENU_BUTTON_TITLE)));
  UpdateMainMenuButtonWidgetBounds();

  main_menu_button_widget_->SetVisibilityAnimationTransition(
      views::Widget::ANIMATE_NONE);
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

}  // namespace ash

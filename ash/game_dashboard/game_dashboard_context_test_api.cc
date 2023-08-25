// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_context_test_api.h"

#include <string>

#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/game_dashboard/game_dashboard_context.h"
#include "ash/game_dashboard/game_dashboard_main_menu_view.h"
#include "ash/game_dashboard/game_dashboard_toolbar_view.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/system/unified/feature_tile.h"
#include "base/timer/timer.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view_utils.h"

namespace ash {

GameDashboardContextTestApi::GameDashboardContextTestApi(
    GameDashboardContext* context,
    ui::test::EventGenerator* event_generator)
    : context_(context), event_generator_(event_generator) {
  CHECK(context_);
  CHECK(event_generator_);
}

const base::RepeatingTimer& GameDashboardContextTestApi::GetRecordingTimer()
    const {
  return context_->recording_timer_;
}

const std::u16string& GameDashboardContextTestApi::GetRecordingDuration()
    const {
  return context_->recording_duration_;
}

GameDashboardWidget* GameDashboardContextTestApi::GetMainMenuButtonWidget() {
  return context_->main_menu_button_widget();
}

PillButton* GameDashboardContextTestApi::GetMainMenuButton() {
  auto* main_menu_button_widget = GetMainMenuButtonWidget();
  CHECK(main_menu_button_widget);
  return views::AsViewClass<PillButton>(
      main_menu_button_widget->GetContentsView());
}

views::Widget* GameDashboardContextTestApi::GetMainMenuWidget() {
  return context_->main_menu_widget_.get();
}

GameDashboardMainMenuView* GameDashboardContextTestApi::GetMainMenuView() {
  return context_->main_menu_view_;
}

FeatureTile* GameDashboardContextTestApi::GetMainMenuGameControlsTile() {
  auto* main_menu_view = GetMainMenuView();
  CHECK(main_menu_view);
  return main_menu_view->game_controls_tile_;
}

FeatureTile* GameDashboardContextTestApi::GetMainMenuToolbarTile() {
  auto* main_menu_view = GetMainMenuView();
  CHECK(main_menu_view);
  return main_menu_view->toolbar_tile_;
}

FeatureTile* GameDashboardContextTestApi::GetMainMenuRecordGameTile() {
  auto* main_menu_view = GetMainMenuView();
  CHECK(main_menu_view);
  return main_menu_view->record_game_tile_;
}

FeatureTile* GameDashboardContextTestApi::GetMainMenuScreenshotTile() {
  return views::AsViewClass<FeatureTile>(
      GetMainMenuViewById(VIEW_ID_GD_SCREENSHOT_TILE));
}

views::Button*
GameDashboardContextTestApi::GetMainMenuScreenSizeSettingsButton() {
  return views::AsViewClass<views::Button>(
      GetMainMenuViewById(VIEW_ID_GD_SCREEN_SIZE_TILE));
}

views::Button*
GameDashboardContextTestApi::GetMainMenuGameControlsDetailsButton() {
  return views::AsViewClass<views::Button>(
      GetMainMenuViewById(VIEW_ID_GD_CONTROLS_DETAILS_ROW));
}

PillButton* GameDashboardContextTestApi::GetMainMenuGameControlsSetupButton() {
  auto* main_menu_view = GetMainMenuView();
  CHECK(main_menu_view);
  return main_menu_view->game_controls_setup_button_;
}

Switch* GameDashboardContextTestApi::GetMainMenuGameControlsHintSwitch() {
  auto* main_menu_view = GetMainMenuView();
  CHECK(main_menu_view);
  return main_menu_view->game_controls_hint_switch_;
}

views::LabelButton* GameDashboardContextTestApi::GetMainMenuFeedbackButton() {
  return views::AsViewClass<views::LabelButton>(
      GetMainMenuViewById(VIEW_ID_GD_FEEDBACK_BUTTON));
}

IconButton* GameDashboardContextTestApi::GetMainMenuHelpButton() {
  return views::AsViewClass<IconButton>(
      GetMainMenuViewById(VIEW_ID_GD_HELP_BUTTON));
}

IconButton* GameDashboardContextTestApi::GetMainMenuSettingsButton() {
  return views::AsViewClass<IconButton>(
      GetMainMenuViewById(VIEW_ID_GD_GENERAL_SETTINGS_BUTTON));
}

void GameDashboardContextTestApi::OpenTheMainMenu() {
  ASSERT_FALSE(GetMainMenuView()) << "The main menu view is already open.";
  ASSERT_FALSE(GetMainMenuWidget()) << "The main menu widget is already open.";
  auto* main_menu_button = GetMainMenuButton();
  ASSERT_TRUE(main_menu_button);
  ClickOnView(main_menu_button, event_generator_);
  ASSERT_TRUE(GetMainMenuView());
  ASSERT_TRUE(GetMainMenuWidget());
}

void GameDashboardContextTestApi::CloseTheMainMenu() {
  ASSERT_TRUE(GetMainMenuView()) << "The main menu view is already closed.";
  ASSERT_TRUE(GetMainMenuWidget()) << "The main menu widget is already closed.";
  auto* main_menu_button = GetMainMenuButton();
  ASSERT_TRUE(main_menu_button);
  ClickOnView(main_menu_button, event_generator_);
  ASSERT_FALSE(GetMainMenuView());
  ASSERT_FALSE(GetMainMenuWidget());
}

GameDashboardWidget* GameDashboardContextTestApi::GetToolbarWidget() {
  return context_->toolbar_widget_.get();
}

GameDashboardToolbarView* GameDashboardContextTestApi::GetToolbarView() {
  return context_->toolbar_view_;
}

IconButton* GameDashboardContextTestApi::GetToolbarGamepadButton() {
  auto* toolbar_view = GetToolbarView();
  CHECK(toolbar_view) << "The toolbar must be opened first before retrieving "
                         "the gamepad button.";
  return toolbar_view->gamepad_button_;
}

IconButton* GameDashboardContextTestApi::GetToolbarGameControlsButton() {
  auto* toolbar_view = GetToolbarView();
  CHECK(toolbar_view) << "The toolbar must be opened first before retrieving "
                         "the game controls button.";
  return toolbar_view->game_controls_button_;
}

IconButton* GameDashboardContextTestApi::GetToolbarRecordGameButton() {
  auto* toolbar_view = GetToolbarView();
  CHECK(toolbar_view) << "The toolbar must be opened first before retrieving "
                         "the record game button.";
  return toolbar_view->record_game_button_;
}

IconButton* GameDashboardContextTestApi::GetToolbarScreenshotButton() {
  auto* toolbar_view = GetToolbarView();
  CHECK(toolbar_view)
      << "The toolbar must be opened first before retrieving a button from it.";
  return views::AsViewClass<IconButton>(
      toolbar_view->GetViewByID(base::to_underlying(
          GameDashboardToolbarView::ToolbarViewId::kScreenshotButton)));
}

GameDashboardContext::ToolbarSnapLocation
GameDashboardContextTestApi::GetToolbarSnapLocation() const {
  return context_->toolbar_snap_location_;
}

void GameDashboardContextTestApi::OpenTheToolbar() {
  ASSERT_TRUE(GetMainMenuWidget())
      << "The main menu widget must be opened first before opening the toolbar";
  ASSERT_TRUE(GetMainMenuView())
      << "The main menu view must be opened first before opening the toolbar";
  ASSERT_FALSE(GetToolbarView())
      << "The toolbar view must be closed before opening it";
  ASSERT_FALSE(GetToolbarWidget())
      << "The toolbar widget must be closed before opening it";
  auto* main_menu_toolbar_tile = GetMainMenuToolbarTile();
  ASSERT_TRUE(main_menu_toolbar_tile);
  ClickOnView(main_menu_toolbar_tile, event_generator_);
  ASSERT_TRUE(GetToolbarView());
  ASSERT_TRUE(GetToolbarWidget());
}

void GameDashboardContextTestApi::CloseTheToolbar() {
  ASSERT_TRUE(GetMainMenuWidget())
      << "The main menu widget must be opened first before closing the toolbar";
  ASSERT_TRUE(GetMainMenuView())
      << "The main menu must be opened first before closing the toolbar";
  ASSERT_TRUE(GetToolbarView())
      << "The toolbar view must be opened before closing it";
  ASSERT_TRUE(GetToolbarWidget())
      << "The toolbar widget must be opened before closing it";
  ASSERT_TRUE(GetToolbarWidget());
  auto* main_menu_toolbar_tile = GetMainMenuToolbarTile();
  ASSERT_TRUE(main_menu_toolbar_tile);
  ClickOnView(main_menu_toolbar_tile, event_generator_);
  ASSERT_FALSE(GetToolbarView());
  ASSERT_FALSE(GetToolbarWidget());
}

views::View* GameDashboardContextTestApi::GetMainMenuViewById(int view_id) {
  auto* main_menu_view = GetMainMenuView();
  CHECK(main_menu_view) << "The main menu must be opened first before "
                           "retrieving the main menu view.";
  return main_menu_view->GetViewByID(view_id);
}

}  // namespace ash

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_context_test_api.h"

#include <string>

#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/game_dashboard/game_dashboard_battery_view.h"
#include "ash/game_dashboard/game_dashboard_button.h"
#include "ash/game_dashboard/game_dashboard_button_reveal_controller.h"
#include "ash/game_dashboard/game_dashboard_context.h"
#include "ash/game_dashboard/game_dashboard_main_menu_cursor_handler.h"
#include "ash/game_dashboard/game_dashboard_main_menu_view.h"
#include "ash/game_dashboard/game_dashboard_toolbar_view.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/switch.h"
#include "ash/system/time/time_view.h"
#include "ash/system/toast/anchored_nudge.h"
#include "ash/system/unified/feature_tile.h"
#include "base/timer/timer.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view_utils.h"
#include "ui/wm/core/window_util.h"

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
  return context_->GetRecordingDuration();
}

const GameDashboardMainMenuCursorHandler*
GameDashboardContextTestApi::GetMainMenuCursorHandler() const {
  return context_->main_menu_cursor_handler_.get();
}

views::Widget* GameDashboardContextTestApi::GetGameDashboardButtonWidget()
    const {
  return context_->game_dashboard_button_widget();
}

GameDashboardButton* GameDashboardContextTestApi::GetGameDashboardButton()
    const {
  return context_->game_dashboard_button_;
}

float GameDashboardContextTestApi::GetGameDashboardButtonCornerRadius() const {
  auto* game_dashboard_button = GetGameDashboardButton();
  CHECK(game_dashboard_button);
  return game_dashboard_button->container_corner_radius_;
}

views::Label* GameDashboardContextTestApi::GetGameDashboardButtonTitle() const {
  auto* game_dashboard_button = GetGameDashboardButton();
  CHECK(game_dashboard_button);
  return game_dashboard_button->title_view_;
}

GameDashboardButtonRevealController*
GameDashboardContextTestApi::GetGameDashboardButtonRevealController() const {
  return context_->game_dashboard_button_reveal_controller_.get();
}

base::OneShotTimer&
GameDashboardContextTestApi::GetRevealControllerTopEdgeHoverTimer() const {
  auto* reveal_controller = GetGameDashboardButtonRevealController();
  CHECK(reveal_controller);
  return reveal_controller->top_edge_hover_timer_;
}

views::Widget* GameDashboardContextTestApi::GetMainMenuWidget() {
  return context_->main_menu_widget_.get();
}

GameDashboardMainMenuView* GameDashboardContextTestApi::GetMainMenuView() {
  return context_->main_menu_view();
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

const std::u16string&
GameDashboardContextTestApi::GetMainMenuScreenSizeSubtitle() {
  auto* main_menu_view = GetMainMenuView();
  CHECK(main_menu_view);
  const views::Label* subtitle_label =
      main_menu_view->GetScreenSizeRowSubtitle();
  CHECK(subtitle_label);
  return subtitle_label->GetText();
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
  return main_menu_view->GetGameControlsSetupButton();
}

TimeView* GameDashboardContextTestApi::GetMainMenuClockView() {
  auto* main_menu_view = GetMainMenuView();
  CHECK(main_menu_view);
  return main_menu_view->clock_view_;
}

GameDashboardBatteryView*
GameDashboardContextTestApi::GetMainMenuBatteryView() {
  auto* main_menu_view = GetMainMenuView();
  CHECK(main_menu_view);
  return main_menu_view->battery_view_;
}

Switch* GameDashboardContextTestApi::GetMainMenuGameControlsFeatureSwitch() {
  auto* main_menu_view = GetMainMenuView();
  CHECK(main_menu_view);
  return main_menu_view->GetGameControlsFeatureSwitch();
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

views::BoxLayoutView* GameDashboardContextTestApi::GetMainMenuContainer() {
  auto* main_menu_view = GetMainMenuView();
  CHECK(main_menu_view);
  return main_menu_view->main_menu_container_;
}

views::BoxLayoutView* GameDashboardContextTestApi::GetSettingsContainer() {
  auto* main_menu_view = GetMainMenuView();
  CHECK(main_menu_view);
  return main_menu_view->settings_view_container_;
}

IconButton* GameDashboardContextTestApi::GetSettingsViewBackButton() {
  auto* main_menu_view = GetMainMenuView();
  CHECK(main_menu_view);
  return main_menu_view->settings_view_back_button_;
}

Switch* GameDashboardContextTestApi::GetSettingsViewWelcomeDialogSwitch() {
  auto* main_menu_view = GetMainMenuView();
  CHECK(main_menu_view);
  return main_menu_view->welcome_dialog_settings_switch_;
}

AnchoredNudge* GameDashboardContextTestApi::GetGameControlsSetupNudge() {
  if (auto* main_menu = GetMainMenuView()) {
    return main_menu->GetGameControlsSetupNudgeForTesting();
  }
  return nullptr;
}

views::Widget* GameDashboardContextTestApi::GetWelcomeDialogWidget() {
  return context_->welcome_dialog_widget_.get();
}

void GameDashboardContextTestApi::OpenTheMainMenu() {
  ASSERT_FALSE(GetMainMenuView()) << "The main menu view is already open.";
  ASSERT_FALSE(GetMainMenuWidget()) << "The main menu widget is already open.";
  ASSERT_FALSE(GetMainMenuCursorHandler())
      << "The cursor handler is already registered.";
  auto* game_dashboard_button = GetGameDashboardButton();
  ASSERT_TRUE(game_dashboard_button);
  ClickOnView(game_dashboard_button, event_generator_);
  // Pause to ensure any other open main menu views have had time to auto-close
  // and notify the `GameDashboardContext` that it's been destroyed.
  base::RunLoop().RunUntilIdle();
  VerifyAccessibilityTree();
  ASSERT_TRUE(GetMainMenuView());
  ASSERT_TRUE(GetMainMenuWidget());
  ASSERT_TRUE(GetMainMenuCursorHandler());
}

void GameDashboardContextTestApi::CloseTheMainMenu() {
  ASSERT_TRUE(GetMainMenuView()) << "The main menu view is already closed.";
  ASSERT_TRUE(GetMainMenuWidget()) << "The main menu widget is already closed.";
  ASSERT_TRUE(GetMainMenuCursorHandler())
      << "The cursor handler is already registered.";
  auto* game_dashboard_button = GetGameDashboardButton();
  ASSERT_TRUE(game_dashboard_button);
  ClickOnView(game_dashboard_button, event_generator_);
  // Pause to ensure the main menu view has had time to auto-close itself and
  // notify the `GameDashboardContext` that it's been destroyed.
  base::RunLoop().RunUntilIdle();
  VerifyAccessibilityTree();
  ASSERT_FALSE(GetMainMenuView());
  ASSERT_FALSE(GetMainMenuWidget());
  ASSERT_FALSE(GetMainMenuCursorHandler());
}

views::Widget* GameDashboardContextTestApi::GetToolbarWidget() {
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

bool GameDashboardContextTestApi::IsToolbarExpanded() {
  auto* toolbar_view = GetToolbarView();
  CHECK(toolbar_view)
      << "The toolbar must be opened first before checking expanded state.";
  return toolbar_view->is_expanded_;
}

GameDashboardToolbarSnapLocation
GameDashboardContextTestApi::GetToolbarSnapLocation() const {
  return context_->toolbar_snap_location_;
}

void GameDashboardContextTestApi::OpenTheToolbar() {
  ASSERT_TRUE(GetMainMenuWidget())
      << "The main menu widget must be opened first before opening the toolbar";
  ASSERT_TRUE(GetMainMenuView())
      << "The main menu view must be opened first before opening the toolbar";
  ASSERT_TRUE(GetMainMenuContainer()->GetVisible())
      << "The main menu container must be open before opening the toolbar";
  ASSERT_FALSE(GetToolbarView())
      << "The toolbar view must be closed before opening it";
  ASSERT_FALSE(GetToolbarWidget())
      << "The toolbar widget must be closed before opening it";
  auto* main_menu_toolbar_tile = GetMainMenuToolbarTile();
  ASSERT_TRUE(main_menu_toolbar_tile);
  ClickOnView(main_menu_toolbar_tile, event_generator_);
  VerifyAccessibilityTree();
  ASSERT_TRUE(GetToolbarView());
  ASSERT_TRUE(GetToolbarWidget());
}

void GameDashboardContextTestApi::SetFocusOnToolbar() {
  views::Widget* toolbar_widget = GetToolbarWidget();
  ASSERT_TRUE(toolbar_widget)
      << "The toolbar view must be opened before trying to place focus on it.";
  toolbar_widget->Activate();
}

void GameDashboardContextTestApi::CloseTheToolbar() {
  ASSERT_TRUE(GetMainMenuWidget())
      << "The main menu widget must be opened first before closing the toolbar";
  ASSERT_TRUE(GetMainMenuView())
      << "The main menu view must be open before closing the toolbar.";
  ASSERT_TRUE(GetMainMenuContainer()->GetVisible())
      << "The main menu must be opened first before closing the toolbar";
  ASSERT_TRUE(GetToolbarView())
      << "The toolbar view must be opened before closing it";
  ASSERT_TRUE(GetToolbarWidget())
      << "The toolbar widget must be opened before closing it";
  ASSERT_TRUE(GetToolbarWidget());
  auto* main_menu_toolbar_tile = GetMainMenuToolbarTile();
  ASSERT_TRUE(main_menu_toolbar_tile);
  ClickOnView(main_menu_toolbar_tile, event_generator_);
  VerifyAccessibilityTree();
  ASSERT_FALSE(GetToolbarView());
  ASSERT_FALSE(GetToolbarWidget());
}

void GameDashboardContextTestApi::OpenMainMenuSettings() {
  ASSERT_TRUE(GetMainMenuWidget()) << "The main menu widget must be opened "
                                      "first before opening the settings view.";
  ASSERT_TRUE(GetMainMenuView())
      << "The main menu view must be open first to open the settings view.";
  ASSERT_TRUE(GetMainMenuContainer()->GetVisible())
      << "The main menu view must be displayed to open the settings view.";
  auto* settings_container = GetSettingsContainer();
  ASSERT_TRUE(!settings_container || !settings_container->GetVisible())
      << "The settings container must either not be created or not visible "
         "when opening the settings view.";
  ClickOnView(GetMainMenuSettingsButton(), event_generator_);
  ASSERT_TRUE(GetSettingsContainer()->GetVisible());
  ASSERT_FALSE(GetMainMenuContainer()->GetVisible());
}

void GameDashboardContextTestApi::CloseTheSettings() {
  ASSERT_TRUE(GetMainMenuWidget()) << "The main menu widget must be open "
                                      "already when closing the settings view.";
  ASSERT_TRUE(GetMainMenuView()) << "The main menu view must be open already "
                                    "when closing the settings view.";
  ASSERT_TRUE(GetSettingsContainer()->GetVisible())
      << "The settings container must be visible when closing the settings "
         "view.";
  ASSERT_TRUE(!GetMainMenuContainer()->GetVisible())
      << "The main menu container must not be visible when closing the "
         "settings.";
  ClickOnView(GetSettingsViewBackButton(), event_generator_);
  ASSERT_FALSE(GetSettingsContainer()->GetVisible());
  ASSERT_TRUE(GetMainMenuContainer()->GetVisible());
}

void GameDashboardContextTestApi::ToggleWelcomeDialogSettingsSwitch() {
  ASSERT_TRUE(GetMainMenuWidget())
      << "The main menu widget must be open already "
         "when toggling the welcome dialog switch.";
  ASSERT_TRUE(GetMainMenuView())
      << "The main menu view must be open already when "
         "toggling the welcome dialog switch.";
  ASSERT_TRUE(GetSettingsContainer()->GetVisible())
      << "The settings container must be visible when toggling the welcome "
         "dialog switch.";
  auto* welcome_dialog_switch = GetSettingsViewWelcomeDialogSwitch();
  bool initial_state = welcome_dialog_switch->GetIsOn();
  ClickOnView(welcome_dialog_switch, event_generator_);
  ASSERT_EQ(GetSettingsViewWelcomeDialogSwitch()->GetIsOn(), !initial_state);
}

void GameDashboardContextTestApi::VerifyAccessibilityTree() {
  const std::vector<views::Widget*> widgets = context_->GetTraversableWidgets();
  const size_t widget_list_size = widgets.size();
  for (size_t i = 0; i < widget_list_size; i++) {
    auto* curr_view = widgets[i]->GetContentsView();
    auto& view_accessibility = curr_view->GetViewAccessibility();
    const size_t prev_index = (i + widget_list_size - 1u) % widget_list_size;
    const size_t next_index = (i + 1u) % widget_list_size;

    EXPECT_EQ(widgets[prev_index], view_accessibility.GetPreviousWindowFocus());
    EXPECT_EQ(widgets[next_index], view_accessibility.GetNextWindowFocus());
  }
}

views::View* GameDashboardContextTestApi::GetMainMenuViewById(int view_id) {
  auto* main_menu_view = GetMainMenuView();
  CHECK(main_menu_view) << "The main menu must be opened first before "
                           "retrieving the main menu view.";
  return main_menu_view->GetViewByID(view_id);
}

}  // namespace ash
